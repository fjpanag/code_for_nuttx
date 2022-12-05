/*******************************************************************************
 *
 *	MQTT broker logger.
 *
 *	File:	mqtt_br_logger.c
 *  Author:	Fotis Panagiotopoulos
 *  Date:	04/10/2022
 *
 *
 ******************************************************************************/

#include "mqtt_br_logger.h"
#include "syslog.h"
#include <time.h>
#include <nuttx/clock.h>
#include <stdio.h>
#include <stdarg.h>
#include <nuttx/config.h>
#include <sys/types.h>

#ifdef CONFIG_MQTT_BROKER

#ifdef CONFIG_MQTT_BROKER_LOG_FILE
static void file_init(void);
static void file_log(int level, const char * fmt, va_list ap);
#endif


void MQTT_logger_init()
{
#ifdef CONFIG_MQTT_BROKER_LOG_FILE
	file_init();
#endif
}

void MQTT_log(int level, const char * fmt, ...)
{
	(void)level;

#ifndef CONFIG_MQTT_BROKER_LOG_DEBUG
	if (level == LOG_DEBUG)
		return;
#endif

	va_list ap;
	va_start(ap, fmt);

#ifdef CONFIG_MQTT_BROKER_LOG_SYSLOG
	vsyslog(level, fmt, ap);
#endif

#ifdef CONFIG_MQTT_BROKER_LOG_FILE
	file_log(level, fmt, ap);
#endif

	va_end(ap);
}


#ifdef CONFIG_MQTT_BROKER_LOG_FILE
void file_init(void)
{
	//Delete any old logs.
	//A new log is created on every system boot.
	remove(CONFIG_MQTT_BROKER_LOG_FILENAME);
}

void file_log(int level, const char * fmt, va_list ap)
{
	static const char * lvl_str[] =	{
		"EMERG", "ALERT", "CRIT", "ERROR",
		"WARN", "NOTICE", "INFO", "DEBUG"
	};

	if ((level < 0) || (level > 7))
		return;

	FILE * f = fopen(CONFIG_MQTT_BROKER_LOG_FILENAME, "a");
	if (f == NULL)
		return;

	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	fprintf(f, "[%5lu.%06ld] ", (long unsigned)ts.tv_sec, ts.tv_nsec / NSEC_PER_USEC);
	fprintf(f, "[%6s] ", lvl_str[level]);
	vfprintf(f, fmt, ap);

	fclose(f);
}
#endif

#endif

