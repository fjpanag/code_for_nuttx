/*******************************************************************************
 *
 *	MQTT broker.
 *
 *	File:	mqtt_broker.h
 *  Author:	Fotis Panagiotopoulos
 *  Date:	06/09/2022
 *
 *
 ******************************************************************************/

#ifndef MQTT_BROKER_H_
#define MQTT_BROKER_H_

#include "list.h"
#include <netinet/in.h>
#include <nuttx/config.h>

#ifdef CONFIG_MQTT_BROKER

/* MQTT broker status. */
typedef struct {
	enum {
		MQTT_BROKER_INHIBIT = -1,
		MQTT_BROKER_DOWN = 0,
		MQTT_BROKER_UP = 1
	} state;

	int clients;
	struct in_addr ip;

} MQTT_Broker_Status_t;

/* MQTT broker structure. */
typedef struct {

	struct {
		int sd;
		int port;
		enum {
			MQTT_SERV_STOPPED,
			MQTT_SERV_RUNNING
		} status;
	} server;

	struct {
		List_t current;
		List_t stored;
	} sessions;

	struct {
		List_t pending;
		List_t retained;
	} queues;

} MQTT_Broker_t;


/*
 *	Starts the MQTT broker.
 */
void MQTT_Broker_start(void);

/*
 *	Gets the status of the MQTT broker.
 *
 *	Parameters:
 *		status		A status struct to be populated
 *					with the current broker data.
 */
void MQTT_Broker_status(MQTT_Broker_Status_t * status);


#endif

#endif

