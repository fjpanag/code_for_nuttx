/*******************************************************************************
 *
 *	Sun calculations.
 *
 *	File:	sun.c
 *  Author:	Fotis Panagiotopoulos
 *  Date:	27/10/2022
 *
 *
 ******************************************************************************/

#include "sun.h"
#include "timezone.h"
#include <time.h>
#include <math.h>
#include <string.h>
#include <sys/types.h>

#define SOLAR_CONST			1367

#define RAD_TO_HOURS(x)		((x) * 12 / M_PI)
#define HOURS_TO_RAD(x)		((x) * M_PI / 12)

static time_t sun_sunrise(struct tm * date, double lat_r, double lon_r);
static time_t sun_sunset(struct tm * date, double lat_r, double lon_r);
static time_t sun_dayDuration(struct tm * date, double lat_r, double lon_r);
static double sun_height(struct tm * date, double lat_r, double lon_r);
static double sun_radiation(struct tm * date, double lat_r, double lon_r);

static double angle_of_day(struct tm * date);
static double declination(double day_angle);
static double eq_of_time(double day_angle);
static double Tsv_Tu(double lon_r, double eqOfTime);
static double dayDuration_rad(double solar_declination, double lat_r);
static double eccentricity(double day_angle);


void Sun_getData(Sun_t * sun, double lat, double lon)
{
	time_t t = time(NULL);

	struct tm now;
	gmtime_r(&t, &now);

	double lat_r = (lat * M_PI / 180);
	double lon_r = (lon * M_PI / 180);

	sun->sunrise = sun_sunrise(&now, lat_r, lon_r);
	sun->sunset = sun_sunset(&now, lat_r, lon_r);
	sun->day_duration = sun_dayDuration(&now, lat_r, lon_r);
	sun->sun_height = sun_height(&now, lat_r, lon_r);
	sun->max_radiation = sun_radiation(&now, lat_r, lon_r);
}


time_t sun_sunrise(struct tm * date, double lat_r, double lon_r)
{
	double day_angle = angle_of_day(date);
	double solar_declination = declination(day_angle);
	double eth = eq_of_time(day_angle);
	double diffUTC_TSV = Tsv_Tu(lon_r, eth);
	double dayDurationj = RAD_TO_HOURS(dayDuration_rad(solar_declination, lat_r));

	double sunrise_hour = (12 - fabs(dayDurationj / 2) - diffUTC_TSV);

	struct tm sunrise;
	memcpy(&sunrise, date, sizeof(struct tm));
	sunrise.tm_hour = (int)sunrise_hour;
	sunrise.tm_min = (int)((double)(sunrise_hour - sunrise.tm_hour) * 60.0);
	sunrise.tm_sec = 0;

	time_t sunrise_ts = timegm(&sunrise);
	sunrise_ts -= (4 * 60);  //Offset the difference between the sun's center and edge.

	return sunrise_ts;
}

time_t sun_sunset(struct tm * date, double lat_r, double lon_r)
{
	double day_angle = angle_of_day(date);
	double solar_declination = declination(day_angle);
	double eth = eq_of_time(day_angle);
	double diffUTC_TSV = Tsv_Tu(lon_r, eth);
	double dayDurationj = RAD_TO_HOURS(dayDuration_rad(solar_declination, lat_r));

	double sunset_hour = (12 + fabs(dayDurationj / 2) - diffUTC_TSV);

	struct tm sunset;
	memcpy(&sunset, date, sizeof(struct tm));
	sunset.tm_hour = (int)sunset_hour;
	sunset.tm_min = (int)((double)(sunset_hour - sunset.tm_hour) * 60.0);
	sunset.tm_sec = 0;

	time_t sunset_ts = timegm(&sunset);
	sunset_ts += (4 * 60);  //Offset the difference between the sun's center and edge.

	return sunset_ts;
}

time_t sun_dayDuration(struct tm * date, double lat_r, double lon_r)
{
	(void)lon_r;

	double day_angle = angle_of_day(date);
	double solar_declination = declination(day_angle);
	double hours = RAD_TO_HOURS(dayDuration_rad(solar_declination, lat_r));

	time_t duration = (time_t)(hours * 3600.0);
	duration += (8 * 60);  //Offset the difference between the sun's center and edge.

	return duration;
}

double sun_height(struct tm * date, double lat_r, double lon_r)
{
	double day_angle = angle_of_day(date);
	double solar_declination = declination(day_angle);
	double eq = eq_of_time(day_angle);
	double tsvh = ((double)date->tm_hour + ((double)date->tm_min / 60.0)) + lon_r * 180 / (15 * M_PI) + eq;
	double ah = acos(-cos((M_PI / 12) * tsvh));

	double height_r = (asin(sin(lat_r) * sin(solar_declination) + cos(lat_r) * cos(solar_declination) * cos(ah)));

	return (height_r * 180 / M_PI);
}

double sun_radiation(struct tm * date, double lat_r, double lon_r)
{
	(void)lon_r;

	double day_angle = angle_of_day(date);
	double decl = declination(day_angle);
	double E0 = eccentricity(day_angle);
	double sunrise_hour_angle = (dayDuration_rad(decl, lat_r) / 2);

	return (SOLAR_CONST * E0 * (cos(decl) * cos(lat_r) * sin(sunrise_hour_angle) / sunrise_hour_angle + sin(decl) * sin(lat_r)));
}


double angle_of_day(struct tm * date)
{
	int days = date->tm_yday;
	int tz_offset = (Timezone_getOffset() + Timezone_getDST());

	int local_hour = (date->tm_hour + tz_offset);
	if (local_hour >= 24)
		days++;
	else if (local_hour < 0)
		days--;

	int leap = 0;
	int year = (date->tm_year + 1900);
	if ((year % 400) == 0)
		leap = 1;
	else if ((year % 100) == 0)
		leap = 0;
	else if ((year % 4) == 0)
		leap = 1;

	return ((2 * M_PI * days) / (365 + leap));
}

double declination(double day_angle)
{
	double solar_declination = 0.006918
							   - 0.399912 * cos(day_angle)
							   + 0.070257 * sin(day_angle)
							   - 0.006758 * cos(2 * day_angle)
							   + 0.000907 * sin(2 * day_angle)
							   - 0.002697 * cos(3 * day_angle)
							   + 0.001480 * sin(3 * day_angle);

	return solar_declination;
}

double eq_of_time(double day_angle)
{
	double et = 0.000075
				+ 0.001868 * cos(day_angle)
				- 0.032077 * sin(day_angle)
				- 0.014615 * cos(2 * day_angle)
				- 0.040890 * sin(2 * day_angle);

	return RAD_TO_HOURS(et);
}

double Tsv_Tu(double lon_r, double eqOfTime)
{
	return (lon_r * (12 / M_PI) + eqOfTime);
}

double dayDuration_rad(double solar_declination, double lat_r)
{
	return (2 * acos(-tan(lat_r) * tan(solar_declination)));
}

double eccentricity(double day_angle)
{
	double E0 = 1.000110
				+ 0.034221 * cos(day_angle)
				+ 0.001280 * sin(day_angle)
				+ 0.000719 * cos(2 * day_angle)
				+ 0.000077 * sin(2 * day_angle);

	return E0;
}

