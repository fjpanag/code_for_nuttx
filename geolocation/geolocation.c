/*******************************************************************************
 *
 *	Geolocation service.
 *
 *	File:	geolocation.c
 *  Author:	Fotis Panagiotopoulos
 *  Date:	22/10/2023
 *
 *
 ******************************************************************************/

#include "geolocation.h"
#include "webclient.h"
#include "timezone.h"
#include "network.h"
#include "json.h"
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <time.h>
#include <syslog.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <nuttx/config.h>
#include <sys/types.h>

#ifdef CONFIG_GEOLOCATION

#define GEO_SIGNAL		(SIGRTMIN + 8)

#define GEOLOCATION_PROVIDER_URL	"http://de-api.ipgeolocation.io/ipgeo?apiKey=%s&%s"
#define GEOLOCATION_FIELDS			"fields=continent_name,country_name,city,latitude,longitude,time_zone"

static Geolocation_t geo;
static pthread_mutex_t mtx;
static int init;
static int retries;

static struct sigevent sev;
static struct itimerspec trigger;
static timer_t timerid;

static void query(union sigval value);
static void response_cb(char ** buffer, int offset, int datend, int * buflen, void * arg);


void Geolocation_start()
{
	pthread_mutexattr_t attr;
	pthread_mutexattr_init(&attr);
	pthread_mutexattr_setprotocol(&attr, PTHREAD_PRIO_INHERIT);
	pthread_mutex_init(&mtx, &attr);

	init = 0;
	retries = 0;
	memset(&geo, 0, sizeof(geo));

	memset(&sev, 0, sizeof(struct sigevent));
	sev.sigev_notify = SIGEV_THREAD;
	sev.sigev_signo = GEO_SIGNAL;
	sev.sigev_value.sival_int = 0;
	sev.sigev_notify_function = query;

	memset(&trigger, 0, sizeof(struct itimerspec));
	trigger.it_value.tv_sec = CONFIG_GEOLOCATION_START_DELAY;
	trigger.it_value.tv_nsec = 0;

	timer_create(CLOCK_REALTIME, &sev, &timerid);
	timer_settime(timerid, 0, &trigger, NULL);
}

void Geolocation_getData(Geolocation_t * geo_data)
{
	pthread_mutex_lock(&mtx);
	memcpy(geo_data, &geo, sizeof(Geolocation_t));
	pthread_mutex_unlock(&mtx);
}


void query(union sigval value)
{
	(void)value;

	char * url = NULL;
	char * buffer = NULL;
	int q_res = -1;
	int q_valid = 0;

	if (!Network_isUp())
		goto end;

	url = malloc(strlen(GEOLOCATION_PROVIDER_URL) + strlen(CONFIG_GEOLOCATION_API_KEY) + strlen(GEOLOCATION_FIELDS) + 1);
	if (url == NULL)
		goto end;

	buffer = malloc(CONFIG_GEOLOCATION_BUFFER_SIZE);
	if (buffer == NULL)
		goto end;

	sprintf(url, GEOLOCATION_PROVIDER_URL, CONFIG_GEOLOCATION_API_KEY, GEOLOCATION_FIELDS);

	struct webclient_context ctx;
	webclient_set_defaults(&ctx);
	ctx.method = "GET";
	ctx.buffer = buffer;
	ctx.buflen = CONFIG_GEOLOCATION_BUFFER_SIZE;
	ctx.callback = response_cb;
	ctx.sink_callback_arg = &q_valid;
	ctx.url = url;

	q_res = webclient_perform(&ctx);

end:
	if ((q_res == 0) && q_valid)
	{
		syslog(LOG_INFO, "%s geolocation data: %s/%s\n",
			   !init ? "Received" : "Updated",
			   geo.location.city ? geo.location.city : "Unknown",
			   geo.location.country);

		init = 1;
		retries = 0;

		//Update the timezone.
		Timezone_setGeo(geo.timezone.offset, geo.timezone.dst);

		//Schedule the next update, at 4:30 tomorrow.
		//This time is deliberately chosen, as it is
		//"just after" a DST change (if it happens).
		time_t now = time(NULL);

		struct tm sched;
		gmtime_r(&now, &sched);
		sched.tm_hour = 4;
		sched.tm_min = 30;
		sched.tm_sec = 0;

		time_t next = (timegm(&sched) + 86400);
		time_t offset = next - now;

		trigger.it_value.tv_sec = offset;

		timer_settime(timerid, 0, &trigger, NULL);
	}
	else
	{
		retries++;

		if (retries == 3)
			syslog(LOG_WARNING, "Error downloading geolocation data.\n");

		//Retry in a few seconds.
		trigger.it_value.tv_sec += CONFIG_GEOLOCATION_RETRY_INTERVAL;

		if (trigger.it_value.tv_sec > 120)
			trigger.it_value.tv_sec = 120;

		timer_settime(timerid, 0, &trigger, NULL);
	}

	free(url);
	free(buffer);
}

void response_cb(char ** buffer, int offset, int datend, int * buflen, void * arg)
{
	DEBUGASSERT(*buffer && arg);
	DEBUGASSERT(offset < *buflen);
	DEBUGASSERT(datend <= *buflen);
	*buflen = CONFIG_GEOLOCATION_BUFFER_SIZE;  //Silence warning.

	int * valid = ((int*)arg);
	*valid = 0;

	struct in_addr ip = { .s_addr = INADDR_ANY };
	char * continent = NULL;
	char * country = NULL;
	char * city = NULL;
	double latitude = 0;
	double longitude = 0;
	int tz_offset = 0;
	int tz_dst = 0;

	JSON_Object_t json;
	if (!JSON_open(&json, &((*buffer)[offset]), datend - offset))
		return;

	//Public IP.
	JSON_Node_t ip_node;
	if (!JSON_get(&json, "ip", &ip_node))
		goto parse_error;

	const char * ip_str = JSON_getString(&json, &ip_node);
	if ((ip_str == NULL) || (strlen(ip_str) == 0))
		goto parse_error;

	if (inet_pton(AF_INET, ip_str, &ip) <= 0)
		goto parse_error;

	//Continent.
	JSON_Node_t continent_node;
	if (!JSON_get(&json, "continent_name", &continent_node))
		goto parse_error;

	const char * continent_str = JSON_getString(&json, &continent_node);
	if ((continent_str == NULL) || (strlen(continent_str) == 0))
		goto parse_error;

	continent = strdup(continent_str);
	if (continent == NULL)
		goto parse_error;

	//Country.
	JSON_Node_t country_node;
	if (!JSON_get(&json, "country_name", &country_node))
		goto parse_error;

	const char * country_str = JSON_getString(&json, &country_node);
	if ((country_str == NULL) || (strlen(country_str) == 0))
		goto parse_error;

	country = strdup(country_str);
	if (country == NULL)
		goto parse_error;

	//City.
	JSON_Node_t city_node;
	if (!JSON_get(&json, "city", &city_node))
		goto parse_error;

	const char * city_str = JSON_getString(&json, &city_node);
	if (city_str == NULL)
		goto parse_error;

	if (strlen(city_str))  //This field can be empty.
	{
		city = strdup(city_str);
		if (city == NULL)
			goto parse_error;
	}

	//Latitude.
	JSON_Node_t latitude_node;
	if (!JSON_get(&json, "latitude", &latitude_node))
		goto parse_error;

	const char * lat_str = JSON_getString(&json, &latitude_node);
	if ((lat_str == NULL) || (strlen(lat_str) == 0))
		goto parse_error;

	if (sscanf(lat_str, "%lf", &latitude) != 1)
		goto parse_error;

	if ((latitude < -180.0) || (latitude > 180.0))
		goto parse_error;

	//Longitude.
	JSON_Node_t longitude_node;
	if (!JSON_get(&json, "longitude", &longitude_node))
		goto parse_error;

	const char * lon_str = JSON_getString(&json, &longitude_node);
	if ((lon_str == NULL) || (strlen(lon_str) == 0))
		goto parse_error;

	if (sscanf(lon_str, "%lf", &longitude) != 1)
		goto parse_error;

	if ((longitude < -180.0) || (longitude > 180.0))
		goto parse_error;

	//Timezone.
	JSON_Object_t timezone_obj;
	JSON_Node_t timezone_node;
	if (!JSON_get(&json, "time_zone", &timezone_node))
		goto parse_error;

	if (JSON_getType(&json, &timezone_node) != JSON_OBJECT)
		goto parse_error;

	if (!JSON_getObject(&json, &timezone_node, &timezone_obj))
		goto parse_error;

	//Timezone offset.
	JSON_Node_t offset_node;
	if (!JSON_get(&timezone_obj, "offset", &offset_node))
		goto parse_error;

	tz_offset = JSON_getInt(&timezone_obj, &offset_node);
	if (tz_offset == JSON_ERROR)
		goto parse_error;

	if ((tz_offset < -12) || (tz_offset > 14))
		goto parse_error;

	//Timezone DST.
	JSON_Node_t dst_node;
	if (!JSON_get(&timezone_obj, "is_dst", &dst_node))
		goto parse_error;

	tz_dst = JSON_getBoolean(&timezone_obj, &dst_node);
	if (tz_dst == JSON_ERROR)
		goto parse_error;

	pthread_mutex_lock(&mtx);

	*valid = 1;

	free(geo.location.continent);
	free(geo.location.country);
	free(geo.location.city);
	memset(&geo, 0, sizeof(geo));

	geo.ip.s_addr = ip.s_addr;
	geo.location.continent = continent;
	geo.location.country = country;
	geo.location.city = city;
	geo.coordinates.latitude = latitude;
	geo.coordinates.longitude = longitude;
	geo.timezone.offset = tz_offset;
	geo.timezone.dst = tz_dst;

	pthread_mutex_unlock(&mtx);

	return;


parse_error:
	*valid = 0;

	free(continent);
	free(country);
	free(city);

	return;
}

#endif

