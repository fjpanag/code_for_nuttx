/*******************************************************************************
 *
 *	MQTT types.
 *
 *	File:	mqtt_br_types.h
 *  Author:	Fotis Panagiotopoulos
 *  Date:	07/09/2022
 *
 *
 ******************************************************************************/

#ifndef MQTT_BR_TYPES_H_
#define MQTT_BR_TYPES_H_

#include <stdlib.h>
#include <stdint.h>
#include <nuttx/config.h>
#include <sys/types.h>

#ifdef CONFIG_MQTT_BROKER

/* Message types. */
typedef enum {
	MQTT_MSG_TYPE_CONNECT		= 1,
	MQTT_MSG_TYPE_CONNACK		= 2,
	MQTT_MSG_TYPE_PUBLISH		= 3,
	MQTT_MSG_TYPE_PUBACK		= 4,
	MQTT_MSG_TYPE_PUBREC		= 5,
	MQTT_MSG_TYPE_PUBREL		= 6,
	MQTT_MSG_TYPE_PUBCOMP		= 7,
	MQTT_MSG_TYPE_SUBSCRIBE		= 8,
	MQTT_MSG_TYPE_SUBACK		= 9,
	MQTT_MSG_TYPE_UNSUBSCRIBE	= 10,
	MQTT_MSG_TYPE_UNSUBACK		= 11,
	MQTT_MSG_TYPE_PINGREQ		= 12,
	MQTT_MSG_TYPE_PINGRESP		= 13,
	MQTT_MSG_TYPE_DISCONNECT	= 14
} MQTT_Msg_Type_t;

/* Message header. */
typedef union {
	unsigned char byte;                     // The whole byte.

	struct {
		unsigned int retain : 1;            // Retained flag bit.
		unsigned int qos : 2;               // QoS value, 0, 1 or 2.
		unsigned int dup : 1;               // DUP flag bit.
		unsigned int type : 4;              // Message type nibble.
	} bits;
} MQTT_Header_t;

/* Connect message flags. */
typedef union {
	unsigned char all;                      // All connect flags.

	struct {
		unsigned int reserved : 1;          // Unused.
		unsigned int cleanSession : 1;      // Clean session flag.
		unsigned int will : 1;              // Will flag.
		unsigned int willQoS : 2;           // Will QoS value.
		unsigned int willRetain : 1;        // Will retain setting.
		unsigned int password : 1;          // 3.1 password.
		unsigned int username : 1;          // 3.1 user name.
	} bits;
} MQTT_connectFlags_t;

/* Connect return codes. */
typedef enum {
	MQTT_CONNACK_OK				= 0,
	MQTT_CONNACK_REFUSE_PROTO	= 1,
	MQTT_CONNACK_REFUSE_ID		= 2,
	MQTT_CONNACK_UNAVAILABLE	= 3,
	MQTT_CONNACK_BAD_USER_PASS	= 4,
	MQTT_CONNACK_UNAUTHORIZED	= 5
} MQTT_Connack_t;

/* MQTT message. */
typedef struct {
	char * topic;

	uint16_t id;

	union {
		uint8_t byte;

		struct {
			uint8_t retain : 1;
			uint8_t qos : 2;
			uint8_t dup : 1;
		};
	} flags;

	struct {
		uint8_t * data;
		size_t size;
	} payload;

} MQTT_Message_t;

/*
 *	Frees all dynamically allocated members of
 *	an MQTT message.
 *
 *	Parameters:
 *		msg			The message to free.
 */
#define MQTT_message_free(msg)		{ free((msg)->topic); free((msg)->payload.data); }


#endif

#endif

