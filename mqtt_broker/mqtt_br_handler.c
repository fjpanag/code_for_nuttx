/*******************************************************************************
 *
 *	MQTT broker message handler.
 *
 *	File:	mqtt_br_handler.c
 *  Author:	Fotis Panagiotopoulos
 *  Date:	08/09/2022
 *
 *
 ******************************************************************************/

#include "mqtt_br_handler.h"
#include "mqtt_broker.h"
#include "mqtt_br_session.h"
#include "mqtt_br_subscription.h"
#include "mqtt_br_authentication.h"
#include "mqtt_br_queue.h"
#include "mqtt_br_helpers.h"
#include "mqtt_br_logger.h"
#include "mqtt_br_types.h"
#include <unistd.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>
#include <nuttx/config.h>
#include <sys/types.h>

#ifdef CONFIG_MQTT_BROKER

#define DROP_SESSION(b, s, buf)		do { MQTT_session_drop(b, s); free(buf); return; } while (0)

static int connect_h(MQTT_Broker_t * broker, MQTT_Session_t * session, uint8_t * msg, size_t len);
static int disconnect_h(MQTT_Broker_t * broker, MQTT_Session_t * session, uint8_t * msg, size_t len);
static int publish_h(MQTT_Broker_t * broker, MQTT_Session_t * session, uint8_t * msg, size_t len);
static int puback_h(MQTT_Broker_t * broker, MQTT_Session_t * session, uint8_t * msg, size_t len);
static int pubrec_h(MQTT_Broker_t * broker, MQTT_Session_t * session, uint8_t * msg, size_t len);
static int pubrel_h(MQTT_Broker_t * broker, MQTT_Session_t * session, uint8_t * msg, size_t len);
static int pubcomp_h(MQTT_Broker_t * broker, MQTT_Session_t * session, uint8_t * msg, size_t len);
static int subscribe_h(MQTT_Broker_t * broker, MQTT_Session_t * session, uint8_t * msg, size_t len);
static int unsubscribe_h(MQTT_Broker_t * broker, MQTT_Session_t * session, uint8_t * msg, size_t len);
static int pingreq_h(MQTT_Broker_t * broker, MQTT_Session_t * session, uint8_t * msg, size_t len);

static int send_connack(MQTT_Broker_t * broker, MQTT_Session_t * session, uint8_t connack, int session_present);
static int send_puback(MQTT_Broker_t * broker, MQTT_Session_t * session, int packet_id);
static int send_pubrec(MQTT_Broker_t * broker, MQTT_Session_t * session, int packet_id);
static int send_pubrel(MQTT_Broker_t * broker, MQTT_Session_t * session, int packet_id);
static int send_pubcomp(MQTT_Broker_t * broker, MQTT_Session_t * session, int packet_id);
static int send_suback(MQTT_Broker_t * broker, MQTT_Session_t * session, int packet_id, int count, const uint8_t * g_qos);
static int send_unsuback(MQTT_Broker_t * broker, MQTT_Session_t * session, int packet_id);
static int send_pingresp(MQTT_Broker_t * broker, MQTT_Session_t * session);


void MQTT_br_handler(MQTT_Broker_t * broker, MQTT_Session_t * session)
{
	int sd = session->sd;
	DEBUGASSERT(sd >= 0);

	//1. Read the header byte.
	uint8_t header_byte;
	if (recv(sd, &header_byte, 1, 0) != 1)
		DROP_SESSION(broker, session, NULL);


	//2. Read and parse the remaining packet length.
	char size[4] = { 0 };
	int remainingSize = 0;
	int multiplier = 1;
	int size_len = 0;
	do {
		//Remaining length field can be up to 4 bytes long.
		if (size_len >= 4)
			DROP_SESSION(broker, session, NULL);

		if (recv(sd, &size[size_len], 1, 0) != 1)
			DROP_SESSION(broker, session, NULL);

		remainingSize += (size[size_len] & 0x7F) * multiplier;
		multiplier <<= 7;
	} while ((size[size_len++] >> 7) & 0x01);  //Checks the continuation bit.


	//3. Allocate a new buffer and copy the message.
	uint8_t * msg = malloc(1 + size_len + remainingSize);
	if (msg == NULL)
	{
		MQTT_log(LOG_ERR, "Broker >> Cannot read packet, memory error.\n");
		DROP_SESSION(broker, session, NULL);
	}

	msg[0] = header_byte;
	for (int i = 0; i < size_len; i++)
		msg[1 + i] = size[i];


	//4. Read the rest of the packet.
	if (remainingSize > 0)
	{
		if (recv(sd, msg + 1 + size_len, remainingSize, MSG_WAITALL) != remainingSize)
			DROP_SESSION(broker, session, msg);
	}


	//5. Handle the message according to its type.
	MQTT_Header_t header;
	header.byte = header_byte;
	size_t len = (1 + size_len + remainingSize);

	MQTT_log(LOG_DEBUG, "Broker >> MQTT <%s:%d> -> %d\n", session->id ? session->id : "anonymous", session->sd, header.bits.type);

	if (header.bits.type == MQTT_MSG_TYPE_CONNECT)
	{
		if (session->active)
			DROP_SESSION(broker, session, msg);

		if (connect_h(broker, session, msg, len) == 0)
		{
			//If connect fails, the session is in a
			//half-activated state.
			//Special handling is needed.
			List_remove(&broker->sessions.current, session);

			free(session->id);
			MQTT_message_free(&session->lwt);
			MQTT_subscriptions_clear(broker, session);

			close(session->sd);

			free(session);
			free(msg);

			return;
		}
	}
	else
	{
		if (!session->active)
			DROP_SESSION(broker, session, msg);

		MQTT_session_ping(broker, session);

		int success = 0;

		switch (header.bits.type)
		{
			case MQTT_MSG_TYPE_DISCONNECT:
				success = disconnect_h(broker, session, msg, len);
				break;

			case MQTT_MSG_TYPE_PUBLISH:
				success = publish_h(broker, session, msg, len);
				break;

			case MQTT_MSG_TYPE_PUBACK:
				success = puback_h(broker, session, msg, len);
				break;

			case MQTT_MSG_TYPE_PUBREC:
				success = pubrec_h(broker, session, msg, len);
				break;

			case MQTT_MSG_TYPE_PUBREL:
				success = pubrel_h(broker, session, msg, len);
				break;

			case MQTT_MSG_TYPE_PUBCOMP:
				success = pubcomp_h(broker, session, msg, len);
				break;

			case MQTT_MSG_TYPE_SUBSCRIBE:
				success = subscribe_h(broker, session, msg, len);
				break;

			case MQTT_MSG_TYPE_UNSUBSCRIBE:
				success = unsubscribe_h(broker, session, msg, len);
				break;

			case MQTT_MSG_TYPE_PINGREQ:
				success = pingreq_h(broker, session, msg, len);
				break;

			case MQTT_MSG_TYPE_CONNACK:
			case MQTT_MSG_TYPE_SUBACK:
			case MQTT_MSG_TYPE_UNSUBACK:
			case MQTT_MSG_TYPE_PINGRESP:
			default:
				//Invalid or illegal message.
				success = 0;
				break;
		}

		if (!success)
			DROP_SESSION(broker, session, msg);
	}

	free(msg);
}


int connect_h(MQTT_Broker_t * broker, MQTT_Session_t * session, uint8_t * msg, size_t len)
{
	DEBUGASSERT(session);
	DEBUGASSERT(!session->active);
	DEBUGASSERT(msg && len);

	uint8_t * p = msg;
	uint8_t * end = (msg + len);
	int connack = 0xFF;

	char * magic = NULL;
	char * client_id = NULL;
	char * username = NULL;
	uint8_t * password = NULL;
	MQTT_Message_t lwt = { 0 };

	int version = 0;
	MQTT_connectFlags_t flags = { 0 };
	time_t keepalive = 0;
	int session_present = 0;
	size_t pass_len = 0;

	//Check for the minimum message size.
	if (len < 14)
		return 0;

	//Read and validate the header.
	MQTT_Header_t header;
	header.byte = MQTT_br_readChar(&p);
	if (header.bits.type != MQTT_MSG_TYPE_CONNECT)
		return 0;

	if (header.bits.dup || header.bits.qos || header.bits.retain)
		return 0;

	//Read and validate the remaining size.
	int s;
	p += MQTT_br_decodeSize(p, &s);
	if ((size_t)((p - msg) + s) != len)
		return 0;

	//Get the protocol version.
	size_t m_len = MQTT_br_readString(&magic, &p, end);
	if ((m_len == 4) && (memcmp(magic, "MQTT", 4) == 0))
	{
		int v = MQTT_br_readChar(&p);
		if (v == 4)
			version = 4; //MQTT v3.1.1
	}
	else if ((m_len == 6) && (memcmp(magic, "MQIsdp", 6) == 0))
	{
		int v = MQTT_br_readChar(&p);
		if (v == 3)
			version = 3; //MQTT v3.1.0
	}

	//Check the protocol version.
	if (version == 0)
	{
		//Invalid header or unsupported version.
		connack = MQTT_CONNACK_REFUSE_PROTO;
		goto end;
	}

	//Get the connection flags.
	flags.all = MQTT_br_readChar(&p);
	if (flags.bits.reserved != 0)
	{
		connack = 0xFF;
		goto end;
	}

	//Get the keepalive interval.
	keepalive = MQTT_br_readInt(&p);

	//Get the client ID.
	MQTT_br_readString(&client_id, &p, end);
	if (client_id == NULL)
	{
		if (version == 3)
		{
			//Version 3.1.0 requires an ID.
			connack = MQTT_CONNACK_REFUSE_ID;
			goto end;
		}
		else if ((version == 4) && !flags.bits.cleanSession)
		{
			//Version 3.1.1 requires an ID only if
			//the session is not clean.
			connack = MQTT_CONNACK_REFUSE_ID;
			goto end;
		}
	}

	//Last will and testament.
	if (flags.bits.will)
	{
		MQTT_br_readString(&lwt.topic, &p, end);
		if ((lwt.topic == NULL) || (strlen(lwt.topic) == 0))
		{
			connack = 0xFF;
			goto end;
		}

		if (strchr(lwt.topic, '#') || strchr(lwt.topic, '+') || (lwt.topic[0] == '$'))
		{
			connack = 0xFF;
			goto end;
		}

		if ((flags.bits.willQoS != 0) && (flags.bits.willQoS != 1) && (flags.bits.willQoS != 2))
		{
			connack = 0xFF;
			goto end;
		}

		lwt.flags.qos = flags.bits.willQoS;
		lwt.flags.retain = flags.bits.willRetain;
		lwt.flags.dup = 0;

		lwt.payload.size = MQTT_br_readInt(&p);

		if (lwt.payload.size)
		{
			if (&p[lwt.payload.size] > end)
			{
				connack = 0xFF;
				goto end;
			}

			lwt.payload.data = malloc(lwt.payload.size);

			if (lwt.payload.data != NULL)
			{
				memcpy(lwt.payload.data, p, lwt.payload.size);
				p += lwt.payload.size;
			}
			else
			{
				connack = MQTT_CONNACK_UNAVAILABLE;
				goto end;
			}
		}
	}
	else
	{
		if ((flags.bits.willQoS != 0) || (flags.bits.willRetain != 0))
		{
			connack = 0xFF;
			goto end;
		}
	}

	//Get the username & password.
	if (flags.bits.username)
	{
		MQTT_br_readString(&username, &p, end);
		if (username == NULL)
		{
			connack = MQTT_CONNACK_BAD_USER_PASS;
			goto end;
		}
	}

	if (flags.bits.password)
	{
		if (!flags.bits.username)
		{
			connack = 0xFF;
			goto end;
		}

		pass_len = MQTT_br_readInt(&p);

		if (pass_len > 0)
		{
			if (&p[pass_len] > end)
			{
				connack = 0xFF;
				goto end;
			}

			password = malloc(pass_len);
			if (password == NULL)
			{
				connack = 0xFF;
				goto end;
			}

			memcpy(password, p, pass_len);
			p += pass_len;
		}
	}

	//Authenticate the client.
	if (!MQTT_authenticate(broker, client_id, username, password, pass_len))
	{
		connack = MQTT_CONNACK_UNAUTHORIZED;
		goto end;
	}


	//Activate the session.
	MQTT_session_activate(broker, session, client_id, keepalive, flags.bits.cleanSession, &lwt, &session_present);

	connack = MQTT_CONNACK_OK;


end:

	//If the session was never activated, clean-up everything.
	if (!session->active)
	{
		free(client_id);
		MQTT_message_free(&lwt);
	}

	free(magic);
	free(username);
	free(password);

	if (connack != 0xFF)
	{
		if (!send_connack(broker, session, connack, session_present))
			return 0;

		//If there is a stored session, send all retained messages.
		if ((connack == MQTT_CONNACK_OK) && session_present)
		{
			MQTT_Subscription_t * subscription = List_getFirst(&session->subscriptions);
			while (subscription)
			{
				MQTT_queue_handleRetained(broker, session, subscription->topic_filter, subscription->qos);
				subscription = List_getNext(&session->subscriptions, subscription);
			}
		}
	}

	return (connack == MQTT_CONNACK_OK);
}

int disconnect_h(MQTT_Broker_t * broker, MQTT_Session_t * session, uint8_t * msg, size_t len)
{
	DEBUGASSERT(session);
	DEBUGASSERT(session->active);
	DEBUGASSERT(msg && len);

	uint8_t * p = msg;

	//Check message size.
	if (len != 2)
		return 0;

	//Read and validate the header.
	MQTT_Header_t header;
	header.byte = MQTT_br_readChar(&p);
	if (header.bits.type != MQTT_MSG_TYPE_DISCONNECT)
		return 0;

	if (header.bits.dup || header.bits.qos || header.bits.retain)
		return 0;

	//Read and validate the remaining size.
	int s;
	p += MQTT_br_decodeSize(p, &s);
	if (s != 0)
		return 0;

	//Close the session.
	MQTT_session_close(broker, session);

	return 1;
}

int publish_h(MQTT_Broker_t * broker, MQTT_Session_t * session, uint8_t * msg, size_t len)
{
	DEBUGASSERT(session);
	DEBUGASSERT(session->active);
	DEBUGASSERT(msg && len);

	uint8_t * p = msg;
	uint8_t * end = (msg + len);

	char * topic = NULL;

	int packet_id = 0;
	MQTT_Message_t message = { 0 };

	//Check message size.
	if (len < 7)
		return 0;

	//Read and validate the header.
	MQTT_Header_t header;
	header.byte = MQTT_br_readChar(&p);
	if (header.bits.type != MQTT_MSG_TYPE_PUBLISH)
		return 0;

	if ((header.bits.qos != 0) && (header.bits.qos != 1) && (header.bits.qos != 2))
		return 0;

	if ((header.bits.qos == 0) && header.bits.dup)
		return 0;

	//Read and validate the remaining size.
	int s;
	p += MQTT_br_decodeSize(p, &s);
	if (s < 5)
		return 0;

	//Read the message topic.
	MQTT_br_readString(&topic, &p, end);
	if ((topic == NULL) || (strlen(topic) == 0))
		goto error;

	//Check the topic validity.
	if (strchr(topic, '#') || strchr(topic, '+') || (topic[0] == '$'))
		goto error;

	//Get the packet ID.
	if ((header.bits.qos == 1) || (header.bits.qos == 2))
	{
		packet_id = MQTT_br_readInt(&p);

		if (packet_id == 0)
			goto error;
	}

	//Get the payload.
	uint8_t * payload = p;
	size_t p_size = (end - p);

	//Create the message.
	message.topic = topic;
	message.id = packet_id;
	message.flags.retain = header.bits.retain;
	message.flags.qos = header.bits.qos;
	message.flags.dup = header.bits.dup;

	if (p_size)
	{
		message.payload.data = malloc(p_size);
		if (message.payload.data == NULL)
			goto error;

		memcpy(message.payload.data, payload, p_size);
		message.payload.size = p_size;
	}

	//Add message to the queue.
	if (header.bits.qos == 0)
	{
		if (!MQTT_queue_add(broker, &message))
			goto error;

		return 1;
	}
	else if (header.bits.qos == 1)
	{
		if (!MQTT_queue_add(broker, &message))
			goto error;

		if (!send_puback(broker, session, packet_id))
			return 0;

		return 1;
	}
	else if (header.bits.qos == 2)
	{
		//Check for duplicate messages.
		int idx = 0;
		while ((idx < CONFIG_MQTT_BROKER_MAX_INFLIGHT) && (session->in_flight.inbound[idx] != packet_id))
			idx++;

		if (idx >= CONFIG_MQTT_BROKER_MAX_INFLIGHT)
		{
			//If no duplicate is found, check for a free one.
			idx = 0;
			while ((idx < CONFIG_MQTT_BROKER_MAX_INFLIGHT) && (session->in_flight.inbound[idx] != 0))
				idx++;
		}

		if (idx >= CONFIG_MQTT_BROKER_MAX_INFLIGHT)
			goto error;

		if (session->in_flight.inbound[idx] == packet_id)
		{
			if (message.flags.dup == 0)
				goto error;

			//This is a duplicate. The message will not be used.
			MQTT_message_free(&message);
		}
		else
		{
			if (!MQTT_queue_add(broker, &message))
				goto error;

			session->in_flight.inbound[idx] = packet_id;
		}

		if (!send_pubrec(broker, session, packet_id))
			return 0;

		return 1;
	}

	/*
	 * The code should never reach this point.
	 * The message should have been resolved above.
	 */
	DEBUGASSERT(0);

error:

	free(topic);

	//Topic is already free'd above.
	message.topic = NULL;
	MQTT_message_free(&message);

	return 0;
}

int puback_h(MQTT_Broker_t * broker, MQTT_Session_t * session, uint8_t * msg, size_t len)
{
	(void)broker;
	DEBUGASSERT(session);
	DEBUGASSERT(session->active);
	DEBUGASSERT(msg && len);

	uint8_t * p = msg;

	//Check message size.
	if (len != 4)
		return 0;

	//Read and validate the header.
	MQTT_Header_t header;
	header.byte = MQTT_br_readChar(&p);
	if (header.bits.type != MQTT_MSG_TYPE_PUBACK)
		return 0;

	if (header.bits.dup || header.bits.qos || header.bits.retain)
		return 0;

	//Read and validate the remaining size.
	int s;
	p += MQTT_br_decodeSize(p, &s);
	if (s != 2)
		return 0;

	//Get the packet ID.
	int packet_id = MQTT_br_readInt(&p);

	if (packet_id == 0)
		return 0;

	return 1;
}

int pubrec_h(MQTT_Broker_t * broker, MQTT_Session_t * session, uint8_t * msg, size_t len)
{
	DEBUGASSERT(session);
	DEBUGASSERT(session->active);
	DEBUGASSERT(msg && len);

	uint8_t * p = msg;

	//Check message size.
	if (len != 4)
		return 0;

	//Read and validate the header.
	MQTT_Header_t header;
	header.byte = MQTT_br_readChar(&p);
	if (header.bits.type != MQTT_MSG_TYPE_PUBREC)
		return 0;

	if (header.bits.dup || header.bits.qos || header.bits.retain)
		return 0;

	//Read and validate the remaining size.
	int s;
	p += MQTT_br_decodeSize(p, &s);
	if (s != 2)
		return 0;

	//Get the packet ID.
	int packet_id = MQTT_br_readInt(&p);

	if (packet_id == 0)
		return 0;

	//Find the acknowledged message.
#if 0  //Not used, see explanation bellow.
	for (int i = 0; i < CONFIG_MQTT_BROKER_MAX_INFLIGHT; i++)
	{
		if (session->in_flight.outbound[i] == packet_id)
			session->in_flight.outbound[i] = 0;
	}
#endif

	/*
	 * Note! A valid response is always sent.
	 * This is because it is normal to receive a PUBREC for a message that
	 * that is unknown to the broker. If this PUBREC is a duplicate of a
	 * previous one that was received, but PUBREL was lost, the client
	 * will retry PUBREC on an ID that the server has cleared. This
	 * handling takes care of all these.
	 *
	 * Of course, this creates a loophole. If the client sends a PUBREC
	 * on a random/invalid ID, then the server will still acknowledge it,
	 * instead of dropping the session. But still, this is a client's
	 * problem, its implementation is broken.
	 */

	//Send the response.
	if (!send_pubrel(broker, session, packet_id))
		return 0;

	return 1;
}

int pubrel_h(MQTT_Broker_t * broker, MQTT_Session_t * session, uint8_t * msg, size_t len)
{
	DEBUGASSERT(session);
	DEBUGASSERT(session->active);
	DEBUGASSERT(msg && len);

	uint8_t * p = msg;

	//Check message size.
	if (len != 4)
		return 0;

	//Read and validate the header.
	MQTT_Header_t header;
	header.byte = MQTT_br_readChar(&p);
	if (header.bits.type != MQTT_MSG_TYPE_PUBREL)
		return 0;

	if (header.bits.dup || (header.bits.qos != 1) || header.bits.retain)
		return 0;

	//Read and validate the remaining size.
	int s;
	p += MQTT_br_decodeSize(p, &s);
	if (s != 2)
		return 0;

	//Get the packet ID.
	int packet_id = MQTT_br_readInt(&p);

	if (packet_id == 0)
		return 0;

	//Find the acknowledged message.
	for (int i = 0; i < CONFIG_MQTT_BROKER_MAX_INFLIGHT; i++)
	{
		if (session->in_flight.inbound[i] == packet_id)
			session->in_flight.inbound[i] = 0;
	}

	/*
	 * Note! A valid response is always sent.
	 * This is because it is normal to receive a PUBREL for a message that
	 * that is unknown to the broker. If this PUBREL is a duplicate of a
	 * previous one that was received, but PUBCOMP was lost, the client
	 * will retry PUBREL on an ID that the server has cleared. This
	 * handling takes care of all these.
	 *
	 * Of course, this creates a loophole. If the client sends a PUBREL
	 * on a random/invalid ID, then the server will still acknowledge it,
	 * instead of dropping the session. But still, this is a client's
	 * problem, its implementation is broken.
	 */

	//Send the response.
	if (!send_pubcomp(broker, session, packet_id))
		return 0;

	return 1;
}

int pubcomp_h(MQTT_Broker_t * broker, MQTT_Session_t * session, uint8_t * msg, size_t len)
{
	(void)broker;
	DEBUGASSERT(session);
	DEBUGASSERT(session->active);
	DEBUGASSERT(msg && len);

	uint8_t * p = msg;

	//Check message size.
	if (len != 4)
		return 0;

	//Read and validate the header.
	MQTT_Header_t header;
	header.byte = MQTT_br_readChar(&p);
	if (header.bits.type != MQTT_MSG_TYPE_PUBCOMP)
		return 0;

	if (header.bits.dup || header.bits.qos || header.bits.retain)
		return 0;

	//Read and validate the remaining size.
	int s;
	p += MQTT_br_decodeSize(p, &s);
	if (s != 2)
		return 0;

	//Get the packet ID.
	int packet_id = MQTT_br_readInt(&p);

	if (packet_id == 0)
		return 0;

	return 1;
}

int subscribe_h(MQTT_Broker_t * broker, MQTT_Session_t * session, uint8_t * msg, size_t len)
{
	DEBUGASSERT(session);
	DEBUGASSERT(session->active);
	DEBUGASSERT(msg && len);

	uint8_t * p = msg;
	uint8_t * end = (msg + len);

	//Check message size.
	if (len < 8)
		return 0;

	//Read and validate the header.
	MQTT_Header_t header;
	header.byte = MQTT_br_readChar(&p);
	if (header.bits.type != MQTT_MSG_TYPE_SUBSCRIBE)
		return 0;

	if (header.bits.dup || (header.bits.qos != 1) || header.bits.retain)
		return 0;

	//Read and validate the remaining size.
	int s;
	p += MQTT_br_decodeSize(p, &s);
	if (s < 6)
		return 0;

	//Get the packet ID.
	int packet_id = MQTT_br_readInt(&p);

	if (packet_id == 0)
		return 0;

	//Read the subscriptions.
	int idx = 0;
	uint8_t g_qos[CONFIG_MQTT_BROKER_MAX_SUBSCRIPTIONS];
	while (p <= (end - 4))
	{
		if (idx >= CONFIG_MQTT_BROKER_MAX_SUBSCRIPTIONS)
			return 0;

		char * topic_filter = NULL;
		MQTT_br_readString(&topic_filter, &p, end);
		if ((topic_filter == NULL) || (strlen(topic_filter) == 0))
			goto topic_error;

		char * m_wild = strchr(topic_filter, '#');
		if (m_wild != NULL)
		{
			if (m_wild != &topic_filter[strlen(topic_filter) - 1])
				goto topic_error;

			if ((m_wild != topic_filter) && (*(m_wild - 1) != '/'))
				goto topic_error;
		}

		char * s_wild = strchr(topic_filter, '+');
		if (s_wild != NULL)
		{
			if ((s_wild != topic_filter) && (*(s_wild - 1) != '/'))
				goto topic_error;

			if ((s_wild != &topic_filter[strlen(topic_filter) - 1]) && (*(s_wild + 1) != '/'))
				goto topic_error;
		}

		int qos = MQTT_br_readChar(&p);
		if ((qos < 0) || (qos > 2))
			goto topic_error;

		g_qos[idx] = MQTT_subscriptions_add(broker, session, topic_filter, qos);

		if (g_qos[idx] != 0x80)
			MQTT_queue_handleRetained(broker, session, topic_filter, g_qos[idx]);
		else
			free(topic_filter);

		idx++;

		continue;


topic_error:
		free(topic_filter);
		return 0;
	}

	//At least one subscription is required.
	if (idx == 0)
		return 0;

	//Send the response.
	if (!send_suback(broker, session, packet_id, idx, g_qos))
		return 0;

	return 1;
}

int unsubscribe_h(MQTT_Broker_t * broker, MQTT_Session_t * session, uint8_t * msg, size_t len)
{
	DEBUGASSERT(session);
	DEBUGASSERT(session->active);
	DEBUGASSERT(msg && len);

	uint8_t * p = msg;
	uint8_t * end = (msg + len);

	//Check message size.
	if (len < 7)
		return 0;

	//Read and validate the header.
	MQTT_Header_t header;
	header.byte = MQTT_br_readChar(&p);
	if (header.bits.type != MQTT_MSG_TYPE_UNSUBSCRIBE)
		return 0;

	if (header.bits.dup || (header.bits.qos != 1) || header.bits.retain)
		return 0;

	//Read and validate the remaining size.
	int s;
	p += MQTT_br_decodeSize(p, &s);
	if (s < 5)
		return 0;

	//Get the packet ID.
	int packet_id = MQTT_br_readInt(&p);

	if (packet_id == 0)
		return 0;

	//Read the unsubscriptions.
	int idx = 0;
	while (p <= (end - 3))
	{
		char * topic_filter = NULL;
		MQTT_br_readString(&topic_filter, &p, end);
		if ((topic_filter == NULL) || (strlen(topic_filter) == 0))
			goto topic_error;

		char * m_wild = strchr(topic_filter, '#');
		if (m_wild != NULL)
		{
			if (m_wild != &topic_filter[strlen(topic_filter) - 1])
				goto topic_error;

			if ((m_wild != topic_filter) && (*(m_wild - 1) != '/'))
				goto topic_error;
		}

		char * s_wild = strchr(topic_filter, '+');
		if (s_wild != NULL)
		{
			if ((s_wild != topic_filter) && (*(s_wild - 1) != '/'))
				goto topic_error;

			if ((s_wild != &topic_filter[strlen(topic_filter) - 1]) && (*(s_wild + 1) != '/'))
				goto topic_error;
		}

		MQTT_subscriptions_remove(broker, session, topic_filter);

		free(topic_filter);

		idx++;

		continue;


topic_error:
		free(topic_filter);
		return 0;
	}

	//At least on unsubscription is required.
	if (idx == 0)
		return 0;

	//Send the response.
	if (!send_unsuback(broker, session, packet_id))
		return 0;

	return 1;
}

int pingreq_h(MQTT_Broker_t * broker, MQTT_Session_t * session, uint8_t * msg, size_t len)
{
	DEBUGASSERT(session);
	DEBUGASSERT(session->active);
	DEBUGASSERT(msg && len);

	uint8_t * p = msg;

	//Check message size.
	if (len != 2)
		return 0;

	//Read and validate the header.
	MQTT_Header_t header;
	header.byte = MQTT_br_readChar(&p);
	if (header.bits.type != MQTT_MSG_TYPE_PINGREQ)
		return 0;

	if (header.bits.dup || header.bits.qos || header.bits.retain)
		return 0;

	//Read and validate the remaining size.
	int s;
	p += MQTT_br_decodeSize(p, &s);
	if (s != 0)
		return 0;

	//Send the response.
	if (!send_pingresp(broker, session))
		return 0;

	return 1;
}


int send_connack(MQTT_Broker_t * broker, MQTT_Session_t * session, uint8_t connack, int session_present)
{
	(void)broker;

	MQTT_Header_t header;
	header.byte = 0;
	header.bits.type = MQTT_MSG_TYPE_CONNACK;

	uint8_t msg[4];
	msg[0] = header.byte;
	msg[1] = 2;  //Remaining length.
	msg[2] = ((connack == MQTT_CONNACK_OK) && session_present) ? 1 : 0;
	msg[3] = connack;

	ssize_t s = send(session->sd, msg, 4, 0);

	return (s == 4);
}

int send_puback(MQTT_Broker_t * broker, MQTT_Session_t * session, int packet_id)
{
	(void)broker;
	DEBUGASSERT(packet_id > 0);

	MQTT_Header_t header;
	header.byte = 0;
	header.bits.type = MQTT_MSG_TYPE_PUBACK;

	uint8_t msg[4];
	msg[0] = header.byte;
	msg[1] = 2;  //Remaining length.

	uint8_t * p = &msg[2];
	MQTT_br_writeInt(&p, packet_id);

	ssize_t s = send(session->sd, msg, 4, 0);

	return (s == 4);
}

int send_pubrec(MQTT_Broker_t * broker, MQTT_Session_t * session, int packet_id)
{
	(void)broker;
	DEBUGASSERT(packet_id > 0);

	MQTT_Header_t header;
	header.byte = 0;
	header.bits.type = MQTT_MSG_TYPE_PUBREC;

	uint8_t msg[4];
	msg[0] = header.byte;
	msg[1] = 2;  //Remaining length.

	uint8_t * p = &msg[2];
	MQTT_br_writeInt(&p, packet_id);

	ssize_t s = send(session->sd, msg, 4, 0);

	return (s == 4);
}

int send_pubrel(MQTT_Broker_t * broker, MQTT_Session_t * session, int packet_id)
{
	(void)broker;
	DEBUGASSERT(packet_id > 0);

	MQTT_Header_t header;
	header.byte = 0;
	header.bits.type = MQTT_MSG_TYPE_PUBREL;
	header.bits.qos = 1;

	uint8_t msg[4];
	msg[0] = header.byte;
	msg[1] = 2;  //Remaining length.

	uint8_t * p = &msg[2];
	MQTT_br_writeInt(&p, packet_id);

	ssize_t s = send(session->sd, msg, 4, 0);

	return (s == 4);
}

int send_pubcomp(MQTT_Broker_t * broker, MQTT_Session_t * session, int packet_id)
{
	(void)broker;
	DEBUGASSERT(packet_id > 0);

	MQTT_Header_t header;
	header.byte = 0;
	header.bits.type = MQTT_MSG_TYPE_PUBCOMP;

	uint8_t msg[4];
	msg[0] = header.byte;
	msg[1] = 2;  //Remaining length.

	uint8_t * p = &msg[2];
	MQTT_br_writeInt(&p, packet_id);

	ssize_t s = send(session->sd, msg, 4, 0);

	return (s == 4);
}

int send_suback(MQTT_Broker_t * broker, MQTT_Session_t * session, int packet_id, int count, const uint8_t * g_qos)
{
	(void)broker;
	DEBUGASSERT(packet_id > 0);
	DEBUGASSERT(count <= CONFIG_MQTT_BROKER_MAX_SUBSCRIPTIONS);

	MQTT_Header_t header;
	header.byte = 0;
	header.bits.type = MQTT_MSG_TYPE_SUBACK;

	int remaining_length = 2 + count;

	uint8_t * msg = malloc(5 + remaining_length);
	if (msg == NULL)
		return 0;

	msg[0] = header.byte;
	int off = MQTT_br_encodeSize(&msg[1], remaining_length);

	uint8_t * p = &msg[1 + off];
	MQTT_br_writeInt(&p, packet_id);

	for (int i = 0; i < count; i++)
	{
		uint8_t qos = g_qos[i];
		DEBUGASSERT((qos == 0) || (qos == 1) || (qos == 2) || (qos == 0x80));
		MQTT_br_writeChar(&p, qos);
	}

	size_t len = (1 + off + 2 + count);
	DEBUGASSERT((msg + len) == p);

	ssize_t s = send(session->sd, msg, len, 0);

	free(msg);

	return (s == (ssize_t)len);
}

int send_unsuback(MQTT_Broker_t * broker, MQTT_Session_t * session, int packet_id)
{
	(void)broker;
	DEBUGASSERT(packet_id > 0);

	MQTT_Header_t header;
	header.byte = 0;
	header.bits.type = MQTT_MSG_TYPE_UNSUBACK;

	uint8_t msg[4];
	msg[0] = header.byte;
	msg[1] = 2;  //Remaining length.

	uint8_t * p = &msg[2];
	MQTT_br_writeInt(&p, packet_id);

	ssize_t s = send(session->sd, msg, 4, 0);

	return (s == 4);
}

int send_pingresp(MQTT_Broker_t * broker, MQTT_Session_t * session)
{
	(void)broker;

	MQTT_Header_t header;
	header.byte = 0;
	header.bits.type = MQTT_MSG_TYPE_PINGRESP;

	uint8_t msg[2];
	msg[0] = header.byte;
	msg[1] = 0;  //Remaining length.

	ssize_t s = send(session->sd, msg, 2, 0);

	return (s == 2);
}

#endif

