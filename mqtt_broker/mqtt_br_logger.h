/*******************************************************************************
 *
 *	MQTT broker logger.
 *
 *	File:	mqtt_br_logger.h
 *  Author:	Fotis Panagiotopoulos
 *  Date:	04/10/2022
 *
 *
 ******************************************************************************/

#ifndef MQTT_BR_LOGGER_H_
#define MQTT_BR_LOGGER_H_

#include <syslog.h>
#include <stdarg.h>
#include <nuttx/config.h>

#ifdef CONFIG_MQTT_BROKER


/*
 *	Initializes the broker logger.
 */
void MQTT_logger_init(void);

/*
 *	Prints a message to the broker logger.
 *
 *	Parameters:
 *		level		The log message level (i.e. severity).
 *		fmt, ...	The formatted message to print.
 */
void MQTT_log(int level, const char * fmt, ...);


#endif

#endif

