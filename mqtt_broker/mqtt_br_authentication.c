/*******************************************************************************
 *
 *	MQTT broker authentication.
 *
 *	File:	mqtt_br_authentication.c
 *  Author:	Fotis Panagiotopoulos
 *  Date:	15/09/2022
 *
 *
 ******************************************************************************/

#include "mqtt_br_authentication.h"
#include "mqtt_broker.h"
#include <stdint.h>
#include <nuttx/config.h>
#include <sys/types.h>

#ifdef CONFIG_MQTT_BROKER


int MQTT_authenticate(MQTT_Broker_t * broker, const char * client_id, const char * username, const uint8_t * password, size_t pass_len)
{
	(void)broker;
	(void)client_id;
	(void)username;
	(void)password;
	(void)pass_len;

	//NOT IMPLEMENTED

	return 1;
}

#endif

