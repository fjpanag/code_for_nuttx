/*******************************************************************************
 *
 *	MQTT broker subscription.
 *
 *	File:	mqtt_br_subscription.h
 *  Author:	Fotis Panagiotopoulos
 *  Date:	09/09/2022
 *
 *
 ******************************************************************************/

#ifndef MQTT_BR_SUBSCRIPTION_H_
#define MQTT_BR_SUBSCRIPTION_H_

#include "mqtt_broker.h"
#include "mqtt_br_session.h"
#include <nuttx/config.h>

#ifdef CONFIG_MQTT_BROKER

/* Topic subscription. */
typedef struct {
	void * next;
	char * topic_filter;
	uint8_t qos;
} MQTT_Subscription_t;


/*
 *	Adds a subscription to the specified session.
 *
 *	Parameters:
 *		broker		MQTT broker handle.
 *		session		Session handle.
 *		topic		The topic filter to subscribe to.
 *		qos			The requested QoS.
 *
 *	Returns the granted QoS, or 0x80 in case of error.
 */
int MQTT_subscriptions_add(MQTT_Broker_t * broker, MQTT_Session_t * session, char * topic_filter, int qos);

/*
 *	Removes a subscription from the specified session.
 *
 *	Parameters:
 *		broker		MQTT broker handle.
 *		session		Session handle.
 *		topic		The topic filter to unsubscribe from.
 */
void MQTT_subscriptions_remove(MQTT_Broker_t * broker, MQTT_Session_t * session, char * topic_filter);

/*
 *	Clears all subscriptions of the specified session.
 *
 *	Parameters:
 *		broker		MQTT broker handle.
 *		session		Session handle.
 */
void MQTT_subscriptions_clear(MQTT_Broker_t * broker, MQTT_Session_t * session);


#endif

#endif

