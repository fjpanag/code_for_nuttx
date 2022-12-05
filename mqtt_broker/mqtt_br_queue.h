/*******************************************************************************
 *
 *	MQTT broker message queues.
 *
 *	File:	mqtt_br_queue.h
 *  Author:	Fotis Panagiotopoulos
 *  Date:	10/09/2022
 *
 *
 ******************************************************************************/

#ifndef MQTT_BR_QUEUE_H_
#define MQTT_BR_QUEUE_H_

#include "mqtt_broker.h"
#include "mqtt_br_session.h"
#include "mqtt_br_types.h"
#include <stdint.h>
#include <nuttx/config.h>

#ifdef CONFIG_MQTT_BROKER

/* Message queue. */
typedef struct {
	void * next;

	struct {
		uint8_t p_qos;
		uint8_t retain;
	} state;

	MQTT_Message_t message;

} MQTT_Queue_t;


/*
 *	Processes all messages in the queue.
 *
 *	Parameters:
 *		broker		MQTT broker handle.
 */
void MQTT_queue_process(MQTT_Broker_t * broker);

/*
 *	Clears all messages pending in the queue.
 *
 *	Parameters:
 *		broker		MQTT broker handle.
 */
void MQTT_queue_clear(MQTT_Broker_t * broker);


/*
 *	Adds a published message to the broker's queue.
 *
 *	Parameters:
 *		broker		MQTT broker handle.
 *		message		The message to enqueue.
 */
int MQTT_queue_add(MQTT_Broker_t * broker, MQTT_Message_t * message);

/*
 *	Publishes to the provided session any retained
 *	messages on the specified topic filter.
 *
 *	Note! It never fails, and it does not drop the session
 *	on any error.
 *
 *	Parameters:
 *		broker		MQTT broker handle.
 *		session		Session handle.
 *		topic		The topic filter to check for retained messages.
 *		g_qos		The QoS to publish the messages.
 */
void MQTT_queue_handleRetained(MQTT_Broker_t * broker, MQTT_Session_t * session, char * topic_filter, int g_qos);


#endif

#endif

