/*******************************************************************************
 *
 *	MQTT client.
 *
 *	File:	mqtt.h
 *  Author:	Fotis Panagiotopoulos
 *  Date:	30/01/2022
 *
 *
 ******************************************************************************/

#ifndef MQTT_H_
#define MQTT_H_

#include <time.h>
#include <stdint.h>
#include <sys/types.h>

/* Quality-of-service definitions. */
typedef enum {
	MQTT_QOS_0,
	MQTT_QOS_1,
	MQTT_QOS_2
} MQTT_QOS_t;

/* MQTT Client. */
typedef struct {
	struct {
		char * address;
		uint16_t port;
	} broker;

	int nextId;

	struct {
		char * clientID;
		char * username;
		char * password;

		int clean;

		struct {
			char * topic;
			int qos;
			int retained;
			size_t size;
			uint8_t * payload;
		} lastWill;
	} session;

	struct {
		int sockfd;
		int enabled;
		int active;
		clock_t timer;
		int (*cb)(int session);
	} connection;

	struct {
		uint8_t * tx;
		uint8_t * rx;
	} buffers;

	void * subscriptions;

	struct {
		clock_t timer;
		clock_t pending;
	} keepalive;

} MQTT_Client_t;

/* MQTT message. */
typedef struct {
	char * topic;

	int id;
	int qos;
	int retained;
	int dup;

	void * payload;
	size_t size;
} MQTT_Message_t;

/* MQTT message constructor. */
#define MQTT_Message_create(msg, t, q, r, p, s)	{ (msg)->topic = t; (msg)->id = 0; (msg)->qos = q; (msg)->retained = r; (msg)->dup = 0; (msg)->payload = p; (msg)->size = s; }

/* MQTT connect callback. */
typedef int (*MQTT_ConnectCB_t)(int sessionPresent);

/* MQTT subscriber callback. */
typedef void (*MQTT_Subscriber_t)(MQTT_Message_t * msg);


/*
 *	Initializes the MQTT client.
 *
 *	Parameters:
 *		client			The MQTT client handle.
 *		host			The broker address.
 *		port			The port that the broker listens to.
 */
void MQTT_init(MQTT_Client_t * client, const char * host, uint16_t port);

/*
 *	Ticks the MQTT client.
 *
 *	Parameters:
 *		client			The MQTT client handle.
 */
void MQTT_tick(MQTT_Client_t * client);

/*
 *	Connects the client to the broker.
 *
 *	Parameters:
 *		client			The MQTT client handle.
 *		id				The client ID to use.
 *		username		The connection username.
 *		password		The connection password.
 *		cleanSession	Whether to request for a clean session.
 *		lastWill		The last will & testament message.
 *
 *	Returns 1 if the connection configuration is correct, 0 otherwise.
 */
int MQTT_connect(MQTT_Client_t * client, const char * id, const char * username, const char * password, int cleanSession, MQTT_Message_t * lastWill);

/*
 *	Sets a connect callback function.
 *
 *	Parameters:
 *		client			The MQTT client handle.
 *		callback		The callback function.
 */
void MQTT_connectCallback(MQTT_Client_t * client, MQTT_ConnectCB_t callback);

/*
 *	Checks whether the client is connected to the broker.
 *
 *	Parameters:
 *		client			The MQTT client handle.
 *
 *	Returns the connection uptime if connected (in seconds)
 *		or 0 if there is no active connection.
 */
time_t MQTT_isConnected(MQTT_Client_t * client);

/*
 *	Disconnects the client from the broker.
 *
 *	This is a graceful disconnect, so Last Will & Testament
 *	message will not be sent.
 *
 *	All subscriptions are preserved for any future connections.
 *
 *	Parameters:
 *		client			The MQTT client handle.
 */
void MQTT_disconnect(MQTT_Client_t * client);

/*
 *	Publishes a message.
 *
 *	Parameters:
 *		client			The MQTT client handle.
 *		topic			The topic to publish to.
 *		qos				Quality of service.
 *		retained		Retained flag.
 *		data			The message payload (it can be NULL).
 *		length			The size of the payload.
 *
 *	Returns 1 if succeeds, 0 otherwise.
 */
int MQTT_publish(MQTT_Client_t * client, const char * topic, MQTT_QOS_t qos, int retained, void * data, size_t length);

/*
 *	Subscribes to a topic.
 *	Topic patterns and wildcards are supported.
 *
 *	Note that due to the lightweight nature of this client,
 *	QoS 2 cannot be guaranteed. Use the duplicate flag and
 *	the packet ID if you really need this QoS level.
 *
 *	Parameters:
 *		client			The MQTT client handle.
 *		topic			The topic to subscribe to.
 *		qos				Quality of service.
 *		subscriber		The subscriber handler.
 *
 *	Returns 1 if succeeds, 0 otherwise.
 */
int MQTT_subscribe(MQTT_Client_t * client, const char * topic, MQTT_QOS_t qos, MQTT_Subscriber_t subscriber);

/*
 *	Unsubscribes from the given topic.
 *
 *	Parameters:
 *		client			The MQTT client handle.
 *		topic			The topic to unsubscribe from.
 *
 *	Returns 1 if succeeds, 0 otherwise.
 */
int MQTT_unsubscribe(MQTT_Client_t * client, const char * topic);


#endif

