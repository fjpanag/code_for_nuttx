/*******************************************************************************
 *
 *	MQTT broker connection session.
 *
 *	File:	mqtt_br_session.h
 *  Author:	Fotis Panagiotopoulos
 *  Date:	08/09/2022
 *
 *
 ******************************************************************************/

#ifndef MQTT_BR_SESSION_H_
#define MQTT_BR_SESSION_H_

#include "mqtt_broker.h"
#include "mqtt_br_types.h"
#include "list.h"
#include <time.h>
#include <stdint.h>
#include <nuttx/config.h>
#include <sys/types.h>

#ifdef CONFIG_MQTT_BROKER

/* Client session. */
typedef struct {
	void * next;

	char * id;
	int active;

	int sd;
	int clean;
	time_t keepalive;
	clock_t timer;

	struct {
		uint16_t inbound[CONFIG_MQTT_BROKER_MAX_INFLIGHT];
#if 0  //Not actually used.
		uint16_t outbound[CONFIG_MQTT_BROKER_MAX_INFLIGHT];
#endif
	} in_flight;

	MQTT_Message_t lwt;

	List_t subscriptions;

} MQTT_Session_t;


/*
 *	Monitors all active sessions.
 *
 *	Parameters:
 *		broker		MQTT broker handle.
 */
void MQTT_sessions_monitor(MQTT_Broker_t * broker);

/*
 *	Terminates all active sessions.
 *
 *	Parameters:
 *		broker		MQTT broker handle.
 */
void MQTT_sessions_reset(MQTT_Broker_t * broker);


/*
 *	Creates a new session.
 *
 *	Parameters:
 *		broker		MQTT broker handle.
 *		sd			The socket descriptor.
 *
 *	Returns the new session, or NULL if it cannot be created.
 */
MQTT_Session_t * MQTT_session_create(MQTT_Broker_t * broker, int sd);

/*
 *	Activates a session.
 *
 *	Parameters:
 *		broker			MQTT broker handle.
 *		session			Session handle.
 *		client_id		The client ID.
 *		keepalive		The keepalive interval, in seconds.
 *		clean			Clean session flag.
 *		lwt				The last will and testament message.
 *		present			Set to 1 if there is a stored session.
 */
void MQTT_session_activate(MQTT_Broker_t * broker, MQTT_Session_t * session, char * client_id, time_t keepalive, int clean, MQTT_Message_t * lwt, int * present);

/*
 *	Pings an active session.
 *
 *	Parameters:
 *		broker			MQTT broker handle.
 *		session			Session handle.
 */
void MQTT_session_ping(MQTT_Broker_t * broker, MQTT_Session_t * session);

/*
 *	Closes a session.
 *	This is a graceful disconnect. The last will and testament
 *	message will not be published.
 *
 *	Parameters:
 *		broker		MQTT broker handle.
 *		session		Session handle.
 */
void MQTT_session_close(MQTT_Broker_t * broker, MQTT_Session_t * session);

/*
 *	Drops a session.
 *	The client is considered disconnected / dead, and the
 *	last will and testament message will be published.
 *
 *	Parameters:
 *		broker		MQTT broker handle.
 *		session		Session handle.
 */
void MQTT_session_drop(MQTT_Broker_t * broker, MQTT_Session_t * session);


#endif

#endif

