/*******************************************************************************
 *
 *	MQTT broker internal server.
 *
 *	File:	mqtt_br_server.h
 *  Author:	Fotis Panagiotopoulos
 *  Date:	08/09/2022
 *
 *
 ******************************************************************************/

#ifndef MQTT_BR_SERVER_H_
#define MQTT_BR_SERVER_H_

#include "mqtt_broker.h"
#include <nuttx/config.h>

#ifdef CONFIG_MQTT_BROKER


/*
 *	Initializes the internal server.
 *
 *	Parameters:
 *		broker		MQTT broker handle.
 */
void MQTT_server_init(MQTT_Broker_t * broker);

/*
 *	Ticks the internal server.
 *
 *	Parameters:
 *		broker		MQTT broker handle.
 */
void MQTT_server_tick(MQTT_Broker_t * broker);


#endif

#endif

