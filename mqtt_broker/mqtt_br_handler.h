/*******************************************************************************
 *
 *	MQTT broker message handler.
 *
 *	File:	mqtt_br_handler.h
 *  Author:	Fotis Panagiotopoulos
 *  Date:	08/09/2022
 *
 *
 ******************************************************************************/

#ifndef MQTT_BR_HANDLER_H_
#define MQTT_BR_HANDLER_H_

#include "mqtt_broker.h"
#include "mqtt_br_session.h"
#include <nuttx/config.h>

#ifdef CONFIG_MQTT_BROKER


/*
 *	Handles an incoming MQTT message.
 *
 *	Parameters:
 *		broker			MQTT broker handle.
 *		sd				The socket descriptor.
 */
void MQTT_br_handler(MQTT_Broker_t * broker, MQTT_Session_t * session);


#endif

#endif

