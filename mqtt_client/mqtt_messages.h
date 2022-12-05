/*******************************************************************************
 *
 *	MQTT messages serializers & deserializers.
 *
 *	File:	mqtt_messages.h
 *  Author:	Fotis Panagiotopoulos
 *  Date:	03/06/2022
 *
 *
 ******************************************************************************/

#ifndef MQTT_MESSAGES_H_
#define MQTT_MESSAGES_H_

#include <stdint.h>
#include <sys/types.h>

/* MQTT Message types. */
typedef enum {
	CONNECT		= 1,
	CONNACK		= 2,
	PUBLISH		= 3,
	PUBACK		= 4,
	PUBREC		= 5,
	PUBREL		= 6,
	PUBCOMP		= 7,
	SUBSCRIBE	= 8,
	SUBACK		= 9,
	UNSUBSCRIBE	= 10,
	UNSUBACK	= 11,
	PINGREQ		= 12,
	PINGRESP	= 13,
	DISCONNECT	= 14
} MQTT_MessageType_t;

/* MQTT packet header. */
typedef union {
	uint8_t byte;                  // The whole byte.

	struct {
		uint8_t retain : 1;        // Retained flag bit.
		uint8_t qos : 2;           // QoS value, 0, 1 or 2.
		uint8_t dup : 1;           // DUP flag bit.
		uint8_t type : 4;          // Message type nibble.
	} bits;
} MQTT_Header_t;

/* Connect options. */
typedef struct {
	char struct_id[4];          //Must be MQTC.
	int struct_version;         //Must be 0.
	int MQTTVersion;            // 3 = 3.1, 4 = 3.1.1

	char * clientID;
	char * username;
	char * password;

	int cleanSession;
	int keepAliveInterval;

	int willFlag;
	struct {
		char struct_id[4];          //Must be MQTW.
		int struct_version;         //Must be 0.

		char * topic;
		int qos;
		int retained;
		size_t size;
		uint8_t * payload;
	} will;
} MQTT_connectOptions_t;


/*
 *	Serializes a CONNECT message.
 *
 *	Parameters:
 *		buffer			Pointer to set to the created message.
 *		options			The CONNECT message options.
 *
 *	Returns the size of the message.
 */
size_t MQTT_connect_serialize(uint8_t ** buffer, MQTT_connectOptions_t * options);

/*
 *	Deserializes a CONNACK message.
 *
 *	Parameters:
 *		buffer			The buffer to read from.
 *		sessionPresent	Whether there is a session present.
 *		connack_rc		The CONNACK return code.
 *
 *	Returns the length of the deserialized data, or 0 in case of error.
 */
size_t MQTT_connack_deserialize(uint8_t * buffer, int * sessionPresent, int * connack_rc);

/*
 *	Serializes a DISCONNECT message.
 *
 *	Parameters:
 *		buffer			Pointer to set to the created message.
 */
size_t MQTT_disconnect_serialize(uint8_t ** buffer);

/*
 *	Serializes a PINGREQ message.
 *
 *	Parameters:
 *		buffer			Pointer to set to the created message.
 */
size_t MQTT_pingreq_serialize(uint8_t ** buffer);

/*
 *	Serializes a PUBLISH message.
 *
 *	Parameters:
 *		buffer			Pointer to set to the created message.
 *		dup				Duplicate flag.
 *		qos				Quality of service.
 *		retained		Retained flag.
 *		packetID		The packet ID.
 *		topic			The topic to publish to.
 *		payload			The message payload.
 *		payloadLed		The size of the payload.
 *
 *	Returns the size of the serialized data, or 0 in case of error.
 */
size_t MQTT_publish_serialize(uint8_t ** buffer, int dup, int qos, int retained, int packetID, const char * topic, uint8_t * payload, size_t payloadLen);

/*
 *	Deserializes a PUBLISH message.
 *
 *	Parameters:
 *		buffer			The buffer to read from.
 *		dup				Duplicate flag.
 *		qos				Quality of service.
 *		retained		Retained flag.
 *		packetID		The packet ID.
 *		topicName		The topic of the publication.
 *		payload			The message payload.
 *		payloadLen		The size of the payload.
 *
 *	Returns the length of the deserialized data, or 0 in case of error.
 */
size_t MQTT_publish_deserialize(uint8_t * buffer, int * dup, int * qos, int * retained, int * packetID, char ** topicName, uint8_t ** payload, size_t * payloadLen);

/*
 *	Serializes a SUBSCRIBE message.
 *
 *	Parameters:
 *		buffer			Pointer to set to the created message.
 *		dup				Duplicate flag.
 *		packetID		The packet ID.
 *		topic			The topic to subscribe to.
 *		QoS				Quality of service of the subscription.
 *
 *	Returns the size of the serialized data, or 0 in case of error.
 */
size_t MQTT_subscribe_serialize(uint8_t ** buffer, int dup, int packetID, const char * topic, int QoS);

/*
 *	Deserializes a SUBACK message.
 *
 *	Parameters:
 *		buffer			The buffer to read from.
 *		packetID		The packet ID.
 *		grantedQoS		Granted quality of service.
 *
 *	Returns the length of the deserialized data, or 0 in case of error.
 */
size_t MQTT_suback_deserialize(uint8_t * buffer, int * packetID, int * grantedQoS);

/*
 *	Serializes an UNSUBSCRIBE message.
 *
 *	Parameters:
 *		buffer			Pointer to set to the created message.
 *		dup				Duplicate flag.
 *		packetID		The packet ID.
 *		topic			The topics to unsubscribe from.
 *
 *	Returns the size of the serialized data, or 0 in case of error.
 */
size_t MQTTS_unsubscribe_serialize(uint8_t ** buffer, int dup, int packetID, const char * topic);

/*
 *	Deserializes an UNSUBSCRIBE message.
 *
 *	Parameters:
 *		buffer			The buffer to read from.
 *		packetID		The packet ID.
 *
 *	Returns the length of the deserialized data, or 0 in case of error.
 */
size_t MQTT_unsuback_deserialize(uint8_t * buffer, int * packetID);

/*
 *	Serializes an ACK message.
 *
 *	Parameters:
 *		buffer			Pointer to set to the created message.
 *		packetType		The type of the ACK message.
 *		dup				Duplicate flag.
 *		packetID		Packet ID.
 *
 *	Returns the size of the serialized data, or 0 in case of error.
 */
size_t MQTT_ack_serialize(uint8_t ** buffer, int packetType, int dup, int packetID);

/*
 *	Deserializes an ACK message.
 *
 *	Parameters:
 *		buffer			The buffer to read from.
 *		packetType		The type of the ACK message.
 *		dup				Duplicate flag.
 *		packetID		Packet ID.
 *
 *	Returns the length of the deserialized data, or 0 in case of error.
 */
size_t MQTT_ack_deserialize(uint8_t * buffer, int * packetType, int * dup, int * packetID);


#endif

