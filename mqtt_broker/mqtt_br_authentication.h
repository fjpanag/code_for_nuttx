/*******************************************************************************
 *
 *	MQTT broker authentication.
 *
 *	File:	mqtt_br_authentication.h
 *  Author:	Fotis Panagiotopoulos
 *  Date:	15/09/2022
 *
 *
 ******************************************************************************/

#ifndef MQTT_BR_AUTHENTICATION_H_
#define MQTT_BR_AUTHENTICATION_H_

#include "mqtt_broker.h"
#include <stdint.h>
#include <nuttx/config.h>
#include <sys/types.h>

#ifdef CONFIG_MQTT_BROKER


/*
 *	Authenticates a client during connect.
 *
 *	Parameters:
 *		broker			MQTT broker handle.
 *		session			Session handle.
 *		client_id		The client ID.
 *		username		The provided username.
 *		password		The provided password.
 *		pass_len		The size of the password.
 */
int MQTT_authenticate(MQTT_Broker_t * broker, const char * client_id, const char * username, const uint8_t * password, size_t pass_len);


#endif

#endif

