/*******************************************************************************
 *
 *	Settings storage.
 *
 *	File:	settings.c
 *  Author:	Fotis Panagiotopoulos
 *  Date:	05/05/2021
 *
 *
 ******************************************************************************/

#include "settings.h"
#include "storage.h"
#include <pthread.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <nuttx/crc32.h>
#include <signal.h>
#include <time.h>
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>
#include <nuttx/config.h>
#include <sys/types.h>

static int sanityCheck(char * str);
static uint32_t hashCalc(void);
Setting_t * getSetting(char * key);
static size_t getString(Setting_t * setting, char * buffer, size_t size);
static int setString(Setting_t * setting, char * str);
static int getInt(Setting_t * setting, int * i);
static int setInt(Setting_t * setting, int i);
static int getBool(Setting_t * setting, int * i);
static int setBool(Setting_t * setting, int i);
static int getFloat(Setting_t * setting, double * f);
static int setFloat(Setting_t * setting, double f);
static int getIP(Setting_t * setting, struct in_addr * ip);
static int setIP(Setting_t * setting, struct in_addr * ip);
static void load(void);
static void save(void);
static void sig_notify(void);
static void dump_cache(union sigval value);

static pthread_mutex_t mtx;
static uint32_t hash;
static int write_pending;
Setting_t map[CONFIG_SETTINGS_MAP_SIZE];
static Storage_t store[CONFIG_SETTINGS_MAX_STORAGES];

static struct {
	pid_t pid;
	uint8_t signo;
} notify[CONFIG_SETTINGS_MAX_SIGNALS];

#ifdef CONFIG_SETTINGS_CACHED_SAVES
#define TIMER_SIGNAL	SIGRTMIN
static struct sigevent sev;
static struct itimerspec trigger;
static timer_t timerid;
#endif


void Settings_init()
{
	pthread_mutexattr_t attr;
	pthread_mutexattr_init(&attr);
	pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
	pthread_mutexattr_setprotocol(&attr, PTHREAD_PRIO_INHERIT);
	pthread_mutex_init(&mtx, &attr);

	hash = 0;
	write_pending = 0;
	memset(map, 0, sizeof(map));
	memset(store, 0, sizeof(store));

	memset(notify, 0, sizeof(notify));

#ifdef CONFIG_SETTINGS_CACHED_SAVES
	memset(&sev, 0, sizeof(struct sigevent));
	sev.sigev_notify = SIGEV_THREAD;
	sev.sigev_signo = TIMER_SIGNAL;
	sev.sigev_value.sival_int = 0;
	sev.sigev_notify_function = dump_cache;

	memset(&trigger, 0, sizeof(struct itimerspec));
	trigger.it_value.tv_sec = 0;
	trigger.it_value.tv_nsec = (50 * 1000000);

	timer_create(CLOCK_REALTIME, &sev, &timerid);
#endif
}

void Settings_setStorage(char * file, int type)
{
	Storage_t * storage = NULL;

	pthread_mutex_lock(&mtx);

	int idx = 0;
	while ((idx < CONFIG_SETTINGS_MAX_STORAGES) && (store[idx].file[0] != '\0')) idx++;

	DEBUGASSERT(idx < CONFIG_SETTINGS_MAX_STORAGES);
	if (idx >= CONFIG_SETTINGS_MAX_STORAGES)
		goto end;

	DEBUGASSERT(strlen(file) < CONFIG_SETTINGS_MAX_FILENAME);
	if (strlen(file) >= CONFIG_SETTINGS_MAX_FILENAME)
		goto end;

	storage = &store[idx];
	strncpy(storage->file, file, sizeof(storage->file));
	storage->file[sizeof(storage->file) - 1] = '\0';

	switch (type)
	{
		case STORAGE_BINARY:
			storage->load_fn = load_bin;
			storage->save_fn = save_bin;
			break;

		case STORAGE_TEXT:
			storage->load_fn = load_text;
			storage->save_fn = save_text;
			break;

		default:
			DEBUGASSERT(0);
			break;
	}


	storage->load_fn(storage->file);

	uint32_t h = hashCalc();

	//Only save if there are more than 1 storages.
	if ((storage != &store[0]) && ((h != hash) || (access(file, F_OK) != 0)))
	{
		sig_notify();
		save();
	}

	hash = h;

end:
	pthread_mutex_unlock(&mtx);
}

void Settings_sync()
{
	pthread_mutex_lock(&mtx);

	load();

	uint32_t h = hashCalc();
	if (h != hash)
	{
		hash = h;

		sig_notify();
		save();
	}

	pthread_mutex_unlock(&mtx);
}

void Settings_notify(uint8_t signo)
{
	pthread_mutex_lock(&mtx);

	int idx = 0;
	while ((idx < CONFIG_SETTINGS_MAX_SIGNALS) && notify[idx].pid)
		idx++;

	DEBUGASSERT(idx < CONFIG_SETTINGS_MAX_SIGNALS);
	if (idx >= CONFIG_SETTINGS_MAX_SIGNALS)
		return;

	notify[idx].pid = getpid();
	notify[idx].signo = signo;

	pthread_mutex_unlock(&mtx);
}

uint32_t Settings_hash()
{
	return hash;
}

void Settings_clear()
{
	pthread_mutex_lock(&mtx);

	memset(map, 0, sizeof(map));
	hash = 0;

	save();

	pthread_mutex_unlock(&mtx);

	while (write_pending)
		usleep(10 * 1000);
}


int Settings_create(char * key, int type, ...)
{
	Setting_t * setting = NULL;

	DEBUGASSERT(strlen(key));
	if (strlen(key) == 0)
		return 0;

	DEBUGASSERT(strlen(key) < CONFIG_SETTINGS_KEY_SIZE);
	if (strlen(key) >= CONFIG_SETTINGS_KEY_SIZE)
		return 0;

	DEBUGASSERT(isalpha(key[0]) && sanityCheck(key));
	if (!isalpha(key[0]) || !sanityCheck(key))
		return 0;

	pthread_mutex_lock(&mtx);

	for (int i = 0; i < CONFIG_SETTINGS_MAP_SIZE; i++)
	{
		if (strcmp(key, map[i].key) == 0)
		{
			setting = &map[i];
			break;
		}

		if (map[i].type == SETTING_EMPTY)
		{
			setting = &map[i];
			break;
		}
	}

	DEBUGASSERT(setting);

	if (setting && (setting->type != type))
	{
		int ok = 0;

		strncpy(setting->key, key, CONFIG_SETTINGS_KEY_SIZE);
		setting->key[CONFIG_SETTINGS_KEY_SIZE - 1] = '\0';
		setting->type = type;

		va_list ap;
		va_start(ap, type);

		switch (type)
		{
			case SETTING_STRING:
			{
				char * str = va_arg(ap, char*);
				ok = setString(setting, str);
				break;
			}

			case SETTING_INT:
			{
				int i = va_arg(ap, int);
				ok = setInt(setting, i);
				break;
			}

			case SETTING_BOOL:
			{
				int i = va_arg(ap, int);
				ok = setBool(setting, i);
				break;
			}

			case SETTING_FLOAT:
			{
				double f = va_arg(ap, double);
				ok = setFloat(setting, f);
				break;
			}

			case SETTING_IP_ADDR:
			{
				struct in_addr * ip = va_arg(ap, struct in_addr*);
				ok = setIP(setting, ip);
				break;
			}

			default:
			case SETTING_EMPTY:
			{
				DEBUGASSERT(0);
				break;
			}
		}

		va_end(ap);

		if (ok)
		{
			hash = hashCalc();
			save();
		}
		else
		{
			memset(setting, 0, sizeof(Setting_t));
			setting = NULL;
		}
	}

	pthread_mutex_unlock(&mtx);

	return (setting != NULL);
}

int Settings_type(char * key)
{
	Setting_t * setting = NULL;
	int type = SETTING_EMPTY;

	pthread_mutex_lock(&mtx);

	setting = getSetting(key);

	if (setting)
		type = setting->type;

	pthread_mutex_unlock(&mtx);

	return type;
}

int Settings_get(char * key, int type, ...)
{
	int ret = 0;
	Setting_t * setting = NULL;

	pthread_mutex_lock(&mtx);

	setting = getSetting(key);
	if (!setting)
		goto end;

	va_list ap;
	va_start(ap, type);

	switch (type)
	{
		case SETTING_STRING:
		{
			char * buf = va_arg(ap, char*);
			size_t len = va_arg(ap, size_t);
			ret = (int)getString(setting, buf, len);
			break;
		}

		case SETTING_INT:
		{
			int * i = va_arg(ap, int*);
			ret = getInt(setting, i);
			break;
		}

		case SETTING_BOOL:
		{
			int * i = va_arg(ap, int*);
			ret = getBool(setting, i);
			break;
		}

		case SETTING_FLOAT:
		{
			double * f = va_arg(ap, double*);
			ret = getFloat(setting, f);
			break;
		}

		case SETTING_IP_ADDR:
		{
			struct in_addr * ip = va_arg(ap, struct in_addr*);
			ret = getIP(setting, ip);
			break;
		}

		case SETTING_EMPTY:
		{
			DEBUGASSERT(0);
			break;
		}
	}

	va_end(ap);

end:
	pthread_mutex_unlock(&mtx);

	return ret;
}

int Settings_set(char * key, int type, ...)
{
	int ret = 0;
	Setting_t * setting = NULL;

	pthread_mutex_lock(&mtx);

	setting = getSetting(key);
	if (!setting)
		goto end;

	va_list ap;
	va_start(ap, type);

	switch (type)
	{
		case SETTING_STRING:
		{
			char * str = va_arg(ap, char*);
			ret = setString(setting, str);
			break;
		}

		case SETTING_INT:
		{
			int i = va_arg(ap, int);
			ret = setInt(setting, i);
			break;
		}

		case SETTING_BOOL:
		{
			int i = va_arg(ap, int);
			ret = setBool(setting, i);
			break;
		}

		case SETTING_FLOAT:
		{
			double f = va_arg(ap, double);
			ret = setFloat(setting, f);
			break;
		}

		case SETTING_IP_ADDR:
		{
			struct in_addr * ip = va_arg(ap, struct in_addr*);
			ret = setIP(setting, ip);
			break;
		}

		case SETTING_EMPTY:
		{
			DEBUGASSERT(0);
			break;
		}
	}

	va_end(ap);

	uint32_t h = hashCalc();
	if (h != hash)
	{
		hash = h;

		sig_notify();
		save();
	}

end:
	pthread_mutex_unlock(&mtx);

	return ret;
}


int Settings_iterate(int idx, Setting_t * setting)
{
	if ((idx < 0) || (idx >= CONFIG_SETTINGS_MAP_SIZE))
	{
		memset(setting, 0, sizeof(Setting_t));
		return 0;
	}

	pthread_mutex_lock(&mtx);

	memcpy(setting, &map[idx], sizeof(Setting_t));

	pthread_mutex_unlock(&mtx);

	return (map[idx].type != SETTING_EMPTY);
}


int sanityCheck(char * str)
{
#ifdef CONFIG_DEBUG_ASSERTIONS
	DEBUGASSERT(strchr(str, '=') == NULL);
	DEBUGASSERT(strchr(str, ';') == NULL);
	DEBUGASSERT(strchr(str, '\n') == NULL);
	DEBUGASSERT(strchr(str, '\r') == NULL);
#else
	if (strchr(str, '=') != NULL) return 0;
	if (strchr(str, ';') != NULL) return 0;
	if (strchr(str, '\n') != NULL) return 0;
	if (strchr(str, '\r') != NULL) return 0;
#endif

	return 1;
}

uint32_t hashCalc()
{
	return crc32((uint8_t*)map, sizeof(map));
}

Setting_t * getSetting(char * key)
{
	for (int i = 0; i < CONFIG_SETTINGS_MAP_SIZE; i++)
	{
		if (map[i].type == SETTING_EMPTY)
			break;

		if (strcmp(map[i].key, key) == 0)
			return &map[i];
	}

	return NULL;
}

size_t getString(Setting_t * setting, char * buffer, size_t size)
{
	DEBUGASSERT(setting);
	DEBUGASSERT((setting->type == SETTING_STRING) || (setting->type == SETTING_IP_ADDR));

	if (setting->type == SETTING_STRING)
	{
		const char * s = setting->s;
		size_t len = strlen(s);

		DEBUGASSERT(len < size);
		if (len >= size)
			return 0;

		strncpy(buffer, s, size);
		buffer[size - 1] = '\0';

		return len;
	}
	else if (setting->type == SETTING_IP_ADDR)
	{
		DEBUGASSERT(INET_ADDRSTRLEN < size);

		inet_ntop(AF_INET, &setting->ip, buffer, size);
		buffer[size - 1] = '\0';

		return strlen(buffer);
	}

	return 0;
}

int setString(Setting_t * setting, char * str)
{
	DEBUGASSERT(setting);
	DEBUGASSERT((setting->type == SETTING_STRING) || (setting->type == SETTING_IP_ADDR));

	ASSERT(strlen(str) < CONFIG_SETTINGS_VALUE_SIZE);
	if (strlen(str) >= CONFIG_SETTINGS_VALUE_SIZE)
		return 0;

	if (strlen(str) && !sanityCheck(str))
		return 0;

	setting->type = SETTING_STRING;
	strncpy(setting->s, str, CONFIG_SETTINGS_VALUE_SIZE);
	setting->s[CONFIG_SETTINGS_VALUE_SIZE - 1] = '\0';

	return 1;
}

int getInt(Setting_t * setting, int * i)
{
	DEBUGASSERT(setting);
	DEBUGASSERT((setting->type == SETTING_INT) || (setting->type == SETTING_BOOL) || (setting->type == SETTING_FLOAT));

	if (setting->type == SETTING_INT)
		*i = setting->i;
	else if (setting->type == SETTING_BOOL)
		*i = !!setting->i;
	else if (setting->type == SETTING_FLOAT)
		*i = (int)setting->f;
	else
		return 0;

	return 1;
}

int setInt(Setting_t * setting, int i)
{
	DEBUGASSERT(setting);
	DEBUGASSERT((setting->type == SETTING_INT) || (setting->type == SETTING_BOOL) || (setting->type == SETTING_FLOAT));

	setting->type = SETTING_INT;
	setting->i = i;

	return 1;
}

int getBool(Setting_t * setting, int * i)
{
	DEBUGASSERT(setting);
	DEBUGASSERT((setting->type == SETTING_BOOL) || (setting->type == SETTING_INT));

	if ((setting->type == SETTING_INT) || (setting->type == SETTING_BOOL))
		*i = !!setting->i;
	else
		return 0;

	return 1;
}

int setBool(Setting_t * setting, int i)
{
	DEBUGASSERT(setting);
	DEBUGASSERT((setting->type == SETTING_BOOL) || (setting->type == SETTING_INT));

	setting->type = SETTING_BOOL;
	setting->i = !!i;

	return 1;
}

int getFloat(Setting_t * setting, double * f)
{
	DEBUGASSERT(setting);
	DEBUGASSERT((setting->type == SETTING_FLOAT) || (setting->type == SETTING_INT));

	if (setting->type == SETTING_FLOAT)
		*f = setting->f;
	else if (setting->type == SETTING_INT)
		*f = (double)setting->i;
	else
		return 0;

	return 1;
}

int setFloat(Setting_t * setting, double f)
{
	DEBUGASSERT(setting);
	DEBUGASSERT((setting->type == SETTING_FLOAT) || (setting->type == SETTING_INT));

	setting->type = SETTING_FLOAT;
	setting->f = f;

	return 1;
}

int getIP(Setting_t * setting, struct in_addr * ip)
{
	DEBUGASSERT(setting);
	DEBUGASSERT((setting->type == SETTING_IP_ADDR) || (setting->type == SETTING_STRING));

	if (setting->type == SETTING_IP_ADDR)
	{
		memcpy(ip, &setting->ip, sizeof(struct in_addr));
		return 1;
	}
	else if (setting->type == SETTING_STRING)
	{
		return inet_pton(AF_INET, setting->s, ip);
	}

	return 0;
}

int setIP(Setting_t * setting, struct in_addr * ip)
{
	DEBUGASSERT(setting);
	DEBUGASSERT((setting->type == SETTING_IP_ADDR) || (setting->type == SETTING_STRING));

	setting->type = SETTING_IP_ADDR;
	memcpy(&setting->ip, ip, sizeof(struct in_addr));

	return 1;
}

void load()
{
	for (int i = 0; i < CONFIG_SETTINGS_MAX_STORAGES; i++)
	{
		if ((store[i].file[0] != '\0') && store[i].load_fn)
			store[i].load_fn(store[i].file);
	}
}

void save()
{
	write_pending = 1;

#ifdef CONFIG_SETTINGS_CACHED_SAVES
	timer_settime(timerid, 0, &trigger, NULL);
#else
	union sigval value = { .sival_int = 0 };
	dump_cache(value);
#endif
}

void sig_notify()
{
	for (int i = 0; i < CONFIG_SETTINGS_MAX_SIGNALS; i++)
	{
		if (notify[i].pid == 0)
			break;

		kill(notify[i].pid, notify[i].signo);
	}
}

void dump_cache(union sigval value)
{
	(void)value;

	pthread_mutex_lock(&mtx);

	for (int i = 0; i < CONFIG_SETTINGS_MAX_STORAGES; i++)
	{
		if ((store[i].file[0] != '\0') && store[i].save_fn)
			store[i].save_fn(store[i].file);
	}

	write_pending = 0;

	pthread_mutex_unlock(&mtx);
}

