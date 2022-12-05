/*******************************************************************************
 *
 *	MQTT messages serializers & deserializers.
 *
 *	File:	mqtt_messages.c
 *  Author:	Fotis Panagiotopoulos
 *  Date:	03/06/2022
 *
 *
 ******************************************************************************/

#include "mqtt_messages.h"
#include "mqtt_helpers.h"
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/types.h>

typedef union {
	unsigned char all;                      // All connect flags.

	struct {
		unsigned int : 1;                   // Unused.
		unsigned int cleanSession : 1;      // Clean session flag.
		unsigned int will : 1;              // Will flag.
		unsigned int willQoS : 2;           // Will QoS value.
		unsigned int willRetain : 1;        // Will retain setting.
		unsigned int password : 1;          // 3.1 password.
		unsigned int username : 1;          // 3.1 user name.
	} bits;
} MQTT_connectFlags_t;

typedef union {
	unsigned char all;                      // All connack flags.

	struct {
		unsigned int : 7;                   // Unused.
		unsigned int sessionPresent : 1;    // Session present flag.
	} bits;
} MQTT_connackFlags_t;


size_t MQTT_connect_serialize(uint8_t ** buffer, MQTT_connectOptions_t * options)
{
	size_t len = 5;
	len += (options->MQTTVersion == 4) ? 10 : 12;
	len += strlen(options->clientID) + 2;

	if (options->willFlag)
		len += strlen(options->will.topic) + 2 + options->will.size + 2;

	if (options->username)
		len += strlen(options->username) + 2;

	if (options->password)
		len += strlen(options->password) + 2;


	*buffer = malloc(len);
	if (*buffer == NULL)
		return 0;


	MQTT_Header_t header;
	header.byte = 0;
	header.bits.type = CONNECT;

	unsigned char * ptr = *buffer;

	MQTT_writeChar(&ptr, header.byte);

	ptr += MQTT_encodeSize(ptr, len - 5);

	if (options->MQTTVersion == 4)
	{
		MQTT_writeString(&ptr, "MQTT");
		MQTT_writeChar(&ptr, (char)4);
	}
	else
	{
		MQTT_writeString(&ptr, "MQIsdp");
		MQTT_writeChar(&ptr, (char)3);
	}

	MQTT_connectFlags_t flags;
	flags.all = 0;
	flags.bits.cleanSession = options->cleanSession;
	flags.bits.will = (options->willFlag) ? 1 : 0;
	if (flags.bits.will)
	{
		flags.bits.willQoS = options->will.qos;
		flags.bits.willRetain = options->will.retained;
	}

	if (options->username)
		flags.bits.username = 1;

	if (options->password)
		flags.bits.password = 1;

	MQTT_writeChar(&ptr, flags.all);
	MQTT_writeInt(&ptr, options->keepAliveInterval);
	MQTT_writeString(&ptr, options->clientID);

	if (options->willFlag)
	{
		MQTT_writeString(&ptr, options->will.topic);

		MQTT_writeInt(&ptr, (int)options->will.size);
		memcpy(ptr, options->will.payload, options->will.size);
		ptr += options->will.size;
	}

	if (flags.bits.username)
		MQTT_writeString(&ptr, options->username);

	if (flags.bits.password)
		MQTT_writeString(&ptr, options->password);

	return (ptr - *buffer);
}

size_t MQTT_connack_deserialize(uint8_t * buffer, int * sessionPresent, int * connack_rc)
{
	unsigned char * curdata = buffer;

	MQTT_Header_t header;
	header.byte = MQTT_readChar(&curdata);
	if (header.bits.type != CONNACK)
		return 0;

	size_t len = 0;
	int rc = MQTT_decodeSize(curdata, &len);

	if (len < 2)
		return 0;

	curdata += rc;

	MQTT_connackFlags_t flags;
	flags.all = MQTT_readChar(&curdata);
	*sessionPresent = (int)(!!flags.bits.sessionPresent);
	*connack_rc = MQTT_readChar(&curdata);

	return (1 + rc + len);
}

size_t MQTT_disconnect_serialize(uint8_t ** buffer)
{
	*buffer = malloc(2);
	if (*buffer == NULL)
		return 0;

	MQTT_Header_t header;
	header.byte = 0;
	header.bits.type = DISCONNECT;

	unsigned char * ptr = *buffer;

	MQTT_writeChar(&ptr, header.byte);

	ptr += MQTT_encodeSize(ptr, 0);

	return (ptr - *buffer);
}

size_t MQTT_pingreq_serialize(uint8_t ** buffer)
{
	*buffer = malloc(2);
	if (*buffer == NULL)
		return 0;

	MQTT_Header_t header;
	header.byte = 0;
	header.bits.type = PINGREQ;

	unsigned char * ptr = *buffer;

	MQTT_writeChar(&ptr, header.byte);

	ptr += MQTT_encodeSize(ptr, 0);

	return (ptr - *buffer);
}

size_t MQTT_publish_serialize(uint8_t ** buffer, int dup, int qos, int retained, int packetID, const char * topic, uint8_t * payload, size_t payloadLen)
{
	size_t pub_len = (2 + strlen(topic) + ((qos > 0) ? 2 : 0) + payloadLen);

	*buffer = malloc(5 + pub_len);
	if (*buffer == NULL)
		return 0;

	MQTT_Header_t header;
	header.byte = 0;
	header.bits.type = PUBLISH;
	header.bits.dup = dup;
	header.bits.qos = qos;
	header.bits.retain = retained;

	unsigned char * ptr = *buffer;

	MQTT_writeChar(&ptr, header.byte);

	ptr += MQTT_encodeSize(ptr, pub_len);

	MQTT_writeString(&ptr, topic);

	if (qos > 0)
		MQTT_writeInt(&ptr, packetID);

	memcpy(ptr, payload, payloadLen);
	ptr += payloadLen;

	return ptr - *buffer;
}

size_t MQTT_publish_deserialize(uint8_t * buffer, int * dup, int * qos, int * retained, int * packetID, char ** topicName, uint8_t ** payload, size_t * payloadLen)
{
	unsigned char * curdata = buffer;
	unsigned char * enddata = NULL;

	MQTT_Header_t header;
	header.byte = MQTT_readChar(&curdata);
	if (header.bits.type != PUBLISH)
		return 0;

	*dup = header.bits.dup;
	*qos = header.bits.qos;
	*retained = header.bits.retain;

	size_t len = 0;
	int rc = MQTT_decodeSize(curdata, &len);

	if (len < 2)
		return 0;

	curdata += rc;
	enddata = curdata + len;

	if (!MQTT_readString(topicName, &curdata, enddata))
		return 0;

	if (*qos > 0)
		*packetID = MQTT_readInt(&curdata);
	else
		*packetID = 0;

	*payloadLen = (enddata - curdata);
	*payload = curdata;

	return (1 + rc + len);
}

size_t MQTT_subscribe_serialize(uint8_t ** buffer, int dup, int packetID, const char * topic, int QoS)
{
	size_t sub_len = 2 + 2 + strlen(topic) + 1;

	*buffer = malloc(5 + sub_len);
	if (*buffer == NULL)
		return 0;

	MQTT_Header_t header;
	header.byte = 0;
	header.bits.type = SUBSCRIBE;
	header.bits.dup = dup;
	header.bits.qos = 1;

	unsigned char * ptr = *buffer;

	MQTT_writeChar(&ptr, header.byte);

	ptr += MQTT_encodeSize(ptr, sub_len);

	MQTT_writeInt(&ptr, packetID);

	MQTT_writeString(&ptr, topic);
	MQTT_writeChar(&ptr, QoS);

	return (ptr - *buffer);
}

size_t MQTT_suback_deserialize(uint8_t * buffer, int * packetID, int * grantedQoS)
{
	unsigned char * curdata = buffer;

	MQTT_Header_t header;
	header.byte = MQTT_readChar(&curdata);
	if (header.bits.type != SUBACK)
		return 0;

	size_t len = 0;
	int rc = MQTT_decodeSize(curdata, &len);

	if (len < 2)
		return 0;

	curdata += rc;

	*packetID = MQTT_readInt(&curdata);

	if (len > 2)
		*grantedQoS = MQTT_readChar(&curdata);

	return (1 + rc + len);
}

size_t MQTTS_unsubscribe_serialize(uint8_t ** buffer, int dup, int packetID, const char * topic)
{
	size_t unsub_len = 2 + 2 + strlen(topic);

	*buffer = malloc(5 + unsub_len);
	if (*buffer == NULL)
		return 0;

	MQTT_Header_t header;
	header.byte = 0;
	header.bits.type = UNSUBSCRIBE;
	header.bits.dup = dup;
	header.bits.qos = 1;

	unsigned char *ptr = *buffer;

	MQTT_writeChar(&ptr, header.byte);

	ptr += MQTT_encodeSize(ptr, unsub_len);

	MQTT_writeInt(&ptr, packetID);

	MQTT_writeString(&ptr, topic);

	return (ptr - *buffer);
}

size_t MQTT_unsuback_deserialize(uint8_t * buffer, int * packetID)
{
	int type = 0;
	int dup = 0;
	size_t rc = MQTT_ack_deserialize(buffer, &type, &dup, packetID);
	if (type != UNSUBACK)
		return 0;

	return rc;
}

size_t MQTT_ack_serialize(uint8_t ** buffer, int packetType, int dup, int packetID)
{
	*buffer = malloc(4);
	if (*buffer == NULL)
		return 0;

	MQTT_Header_t header;
	header.byte = 0;
	header.bits.type = packetType;
	header.bits.dup = dup;
	header.bits.qos = (packetType == PUBREL) ? 1 : 0;

	unsigned char * ptr = *buffer;

	MQTT_writeChar(&ptr, header.byte);

	ptr += MQTT_encodeSize(ptr, 2);
	MQTT_writeInt(&ptr, packetID);

	return (ptr - *buffer);
}

size_t MQTT_ack_deserialize(uint8_t * buffer, int * packetType, int * dup, int * packetID)
{
	unsigned char * curdata = buffer;

	MQTT_Header_t header;
	header.byte = MQTT_readChar(&curdata);
	*dup = header.bits.dup;
	*packetType = header.bits.type;

	size_t len = 0;
	int rc = MQTT_decodeSize(curdata, &len);

	if (len < 2)
		return 0;

	curdata += rc;

	*packetID = MQTT_readInt(&curdata);

	return (1 + rc + 2);
}

