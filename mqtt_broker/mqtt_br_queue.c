/*******************************************************************************
 *
 *	MQTT broker message queues.
 *
 *	File:	mqtt_br_queue.c
 *  Author:	Fotis Panagiotopoulos
 *  Date:	10/09/2022
 *
 *
 ******************************************************************************/

#include "mqtt_br_queue.h"
#include "mqtt_broker.h"
#include "mqtt_br_session.h"
#include "mqtt_br_subscription.h"
#include "mqtt_br_helpers.h"
#include "mqtt_br_logger.h"
#include "mqtt_br_types.h"
#include "list.h"
#include <sys/socket.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>
#include <nuttx/config.h>
#include <sys/types.h>

#ifdef CONFIG_MQTT_BROKER

static uint16_t next_id(void);
static int process_sessions(MQTT_Broker_t * broker, MQTT_Queue_t * queue);
static int process_subscriptions(MQTT_Broker_t * broker, MQTT_Session_t * session, MQTT_Queue_t * queue);
static int publish_message(MQTT_Broker_t * broker, MQTT_Session_t * session, MQTT_Message_t * message);
static char isTopicMatched(char * topicFilter, char * topicName);


void MQTT_queue_process(MQTT_Broker_t * broker)
{
	MQTT_Queue_t * queue = List_getFirst(&broker->queues.pending);
	while (queue)
	{
		DEBUGASSERT(!(strchr(queue->message.topic, '#') || strchr(queue->message.topic, '+') || (queue->message.topic[0] == '$')));

		queue->message.id = next_id();
		queue->message.flags.retain = 0;

		process_sessions(broker, queue);

		List_remove(&broker->queues.pending, queue);

		if (queue->state.retain)
		{
			MQTT_Queue_t * ret_msg = List_getFirst(&broker->queues.retained);
			while (ret_msg)
			{
				if (strcmp(queue->message.topic, ret_msg->message.topic) == 0)
				{
					List_remove(&broker->queues.retained, ret_msg);
					MQTT_message_free(&ret_msg->message);
					free(ret_msg);
					break;
				}

				ret_msg = List_getNext(&broker->queues.retained, ret_msg);
			}

			if (queue->message.payload.size > 0)
			{
				if (List_size(&broker->queues.retained) >= CONFIG_MQTT_BROKER_MAX_RETAINED)
				{
					MQTT_log(LOG_DEBUG, "Broker >> Retained queue limit exceeded, discarding oldest message.\n");
					MQTT_Queue_t * del = List_getFirst(&broker->queues.retained);
					List_remove(&broker->queues.retained, del);
					MQTT_message_free(&del->message);
					free(del);
				}

				MQTT_log(LOG_DEBUG, "Broker >> Storing retained message on topic [%s].\n", queue->message.topic);

				List_add(&broker->queues.retained, queue);
			}
			else
			{
				MQTT_message_free(&queue->message);
				free(queue);
			}
		}
		else
		{
			MQTT_message_free(&queue->message);
			free(queue);
		}

		//Always get the head as each message is removed
		//from the list when used.
		queue = List_getFirst(&broker->queues.pending);
	}
}

void MQTT_queue_clear(MQTT_Broker_t * broker)
{
	MQTT_log(LOG_DEBUG, "Broker >> Dropping all messages in queue...\n");

	MQTT_Queue_t * queue = List_getFirst(&broker->queues.pending);
	while (queue)
	{
		List_remove(&broker->queues.pending, queue);

		MQTT_message_free(&queue->message);
		free(queue);

		//Since the list is manipulated during the iteration,
		//use always the head.
		queue = List_getFirst(&broker->queues.pending);
	}
}


int MQTT_queue_add(MQTT_Broker_t * broker, MQTT_Message_t * message)
{
	MQTT_log(LOG_DEBUG, "Broker >> Queuing new message on [%s].\n", message->topic);

	size_t pending = List_size(&broker->queues.pending);
	if (pending >= CONFIG_MQTT_BROKER_QUEUE_SIZE)
	{
		MQTT_log(LOG_DEBUG, "Broker >> Cannot enqueue message, queue limit exceeded.\n");
		return 0;
	}

	MQTT_Queue_t * q = calloc(1, sizeof(MQTT_Queue_t));
	if (q == NULL)
	{
		MQTT_log(LOG_ERR, "Broker >> Cannot enqueue message, memory error.\n");
		return 0;
	}

	q->next = NULL;
	memcpy(&q->message, message, sizeof(MQTT_Message_t));
	q->state.p_qos = message->flags.qos;
	q->state.retain = message->flags.retain;

	q->message.flags.dup = 0;

	List_add(&broker->queues.pending, q);

	return 1;
}

void MQTT_queue_handleRetained(MQTT_Broker_t * broker, MQTT_Session_t * session, char * topic_filter, int g_qos)
{
	MQTT_Queue_t * retained = List_getFirst(&broker->queues.retained);
	while (retained)
	{
		if (isTopicMatched(topic_filter, retained->message.topic))
		{
			int qos = retained->state.p_qos;
			if (qos > g_qos)
				qos = g_qos;

			retained->message.id = next_id();
			retained->message.flags.retain = 1;
			retained->message.flags.qos = qos;

			DEBUGASSERT(retained->message.payload.data);
			DEBUGASSERT(retained->message.payload.size > 0);

#if 0
			if (qos == 2)
			{
				int idx = 0;
				while ((idx < CONFIG_MQTT_BROKER_MAX_INFLIGHT) && (session->in_flight.outbound[idx] != 0))
				{
					DEBUGASSERT(session->in_flight.outbound[idx] != retained->message.id);
					idx++;
				}

				if (idx >= CONFIG_MQTT_BROKER_MAX_INFLIGHT)
					goto next;

				session->in_flight.outbound[idx] = retained->message.id;
			}
#endif

			if (!publish_message(broker, session, &retained->message))
				return;

			goto next;
		}

next:
		retained = List_getNext(&broker->queues.retained, retained);
	}
}


uint16_t next_id()
{
	static uint16_t next = 1;

	uint16_t id = next++;

	if (next == 0)
		next++;

	return id;
}

int process_sessions(MQTT_Broker_t * broker, MQTT_Queue_t * queue)
{
	/*
	 * Note! Messages should be processed for stored sessions too.
	 * That is, if there is a stored session with a matching
	 * subscription the message should be stored and re-sent when
	 * the client reconnects. Due to the constrained nature of
	 * this broker, this is not possible. Messages are neither
	 * stored, nor retransmitted. Rather, they are directly
	 * forwarded to clients in a best-effort fashion.
	 */

	MQTT_Session_t * session = List_getFirst(&broker->sessions.current);
	while (session)
	{
		//Keep a reference to the next session, as the current one
		//may be dropped in case of an error.
		void * next = List_getNext(&broker->sessions.current, session);

		if (!process_subscriptions(broker, session, queue))
			MQTT_session_drop(broker, session);

		session = next;
	}

	return 1;
}

int process_subscriptions(MQTT_Broker_t * broker, MQTT_Session_t * session, MQTT_Queue_t * queue)
{
	MQTT_Subscription_t * subscription = List_getFirst(&session->subscriptions);
	while (subscription)
	{
		if (isTopicMatched(subscription->topic_filter, queue->message.topic))
		{
			int qos = queue->state.p_qos;
			if (qos > subscription->qos)
				qos = subscription->qos;

			queue->message.flags.qos = qos;

#if 0
			if (qos == 2)
			{
				int idx = 0;
				while ((idx < CONFIG_MQTT_BROKER_MAX_INFLIGHT) && (session->in_flight.outbound[idx] != 0))
				{
					DEBUGASSERT(session->in_flight.outbound[idx] != queue->message.id);
					idx++;
				}

				if (idx >= CONFIG_MQTT_BROKER_MAX_INFLIGHT)
					return 0;

				session->in_flight.outbound[idx] = queue->message.id;
			}
#endif

			if (!publish_message(broker, session, &queue->message))
				return 0;
		}

		subscription = List_getNext(&session->subscriptions, subscription);
	}

	return 1;
}

int publish_message(MQTT_Broker_t * broker, MQTT_Session_t * session, MQTT_Message_t * message)
{
	(void)broker;

	DEBUGASSERT((message->flags.qos == 0) || (message->flags.qos == 1) || (message->flags.qos == 2));

	MQTT_log(LOG_DEBUG, "Broker >> Publishing message to <%s:%d> on [%s].\n", session->id ? session->id : "anonymous", session->sd, message->topic);

	MQTT_Header_t header;
	header.byte = 0;
	header.bits.type = MQTT_MSG_TYPE_PUBLISH;
	header.bits.dup = 0;
	header.bits.qos = message->flags.qos;
	header.bits.retain = message->flags.retain;

	size_t remaining_length = 2 + strlen(message->topic);

	if ((message->flags.qos == 1) || (message->flags.qos == 2))
		remaining_length += 2;

	if (message->payload.size)
	{
		DEBUGASSERT(message->payload.data);
		remaining_length += message->payload.size;
	}
	else
	{
		DEBUGASSERT(message->payload.data == NULL);
	}

	uint8_t * msg = malloc(5 + remaining_length);
	if (msg == NULL)
	{
		MQTT_log(LOG_DEBUG, "Broker >> Cannot publish message, memory error.\n");
		return 0;
	}

	msg[0] = header.byte;
	int off = MQTT_br_encodeSize(&msg[1], (int)remaining_length);

	uint8_t * p = &msg[1 + off];
	MQTT_br_writeString(&p, message->topic);

	if ((message->flags.qos == 1) || (message->flags.qos == 2))
		MQTT_br_writeInt(&p, message->id);

	if (message->payload.size)
	{
		memcpy(p, message->payload.data, message->payload.size);
	}

	size_t len = (1 + off + remaining_length);
	DEBUGASSERT((msg + len) == (p + message->payload.size));

	ssize_t s = send(session->sd, msg, len, 0);

	free(msg);

	return (s == (ssize_t)len);
}

char isTopicMatched(char * topicFilter, char * topicName)
{
	char * curf = topicFilter;
	char * curn = topicName;
	char * curn_end = curn + strlen(topicName);

	if ((*topicFilter == '#') && (*topicName == '$'))
		return 0;

	while (*curf && (curn < curn_end))
	{
		if ((*curn == '/') && (*curf != '/'))
			break;

		if ((*curf != '+') && (*curf != '#') && (*curf != *curn))
			break;

		if (*curf == '+')
		{
			//Skip until the next separator, or the end of the string.
			char * nextpos = curn + 1;
			while ((nextpos < curn_end) && (*nextpos != '/'))
				nextpos = ++curn + 1;
		}
		else if (*curf == '#')
		{
			//Skip till the end of the string.
			curn = curn_end - 1;
		}

		curf++;
		curn++;
	}

	return ((curn == curn_end) && (*curf == '\0'));
}

#endif

