/*******************************************************************************
 *
 *	MQTT broker subscription.
 *
 *	File:	mqtt_br_subscription.c
 *  Author:	Fotis Panagiotopoulos
 *  Date:	09/09/2022
 *
 *
 ******************************************************************************/

#include "mqtt_br_subscription.h"
#include "mqtt_broker.h"
#include "mqtt_br_session.h"
#include "mqtt_br_logger.h"
#include "list.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <nuttx/config.h>
#include <sys/types.h>

#ifdef CONFIG_MQTT_BROKER


int MQTT_subscriptions_add(MQTT_Broker_t * broker, MQTT_Session_t * session, char * topic_filter, int qos)
{
	(void)broker;
	DEBUGASSERT(session);
	DEBUGASSERT(session->active);

	if ((topic_filter == NULL) || (strlen(topic_filter) == 0))
		return 0x80;

	if ((qos != 0) && (qos != 1) && (qos != 2))
		return 0x80;

	int subs = 0;
	MQTT_Subscription_t * it = List_getFirst(&session->subscriptions);
	while (it)
	{
		//Check if the subscription already exists.
		if (strcmp(it->topic_filter, topic_filter) == 0)
		{
			it->qos = qos;
			return qos;
		}

		it = List_getNext(&session->subscriptions, it);
		subs++;
	}

	if (subs >= CONFIG_MQTT_BROKER_MAX_SUBSCRIPTIONS)
	{
		MQTT_log(LOG_DEBUG, "Broker >> Subscriptions limit exceeded for session <%s:%d>.\n", session->id ? session->id : "anonymous", session->sd);
		return 0x80;
	}

	MQTT_log(LOG_DEBUG, "Broker >> Session <%s:%d> subscribed to topic filter [%s].\n", session->id ? session->id : "anonymous", session->sd, topic_filter);

	MQTT_Subscription_t * subscription = calloc(1, sizeof(MQTT_Subscription_t));
	if (subscription == NULL)
	{
		MQTT_log(LOG_DEBUG, "Broker >> Cannot register subscription, memory error.\n");
		return 0x80;
	}

	subscription->topic_filter = topic_filter;
	subscription->qos = qos;

	List_add(&session->subscriptions, subscription);

	return qos;
}

void MQTT_subscriptions_remove(MQTT_Broker_t * broker, MQTT_Session_t * session, char * topic_filter)
{
	(void)broker;
	DEBUGASSERT(session);
	DEBUGASSERT(session->active);

	if ((topic_filter == NULL) || (strlen(topic_filter) == 0))
		return;

	MQTT_Subscription_t * it = List_getFirst(&session->subscriptions);
	while (it)
	{
		//Check if the subscription already exists.
		if (strcmp(it->topic_filter, topic_filter) == 0)
		{
			List_remove(&session->subscriptions, it);

			MQTT_log(LOG_DEBUG, "Broker >> Session <%s:%d> unsubscribed from topic filter [%s].\n", session->id ? session->id : "anonymous", session->sd, topic_filter);

			free(it->topic_filter);
			free(it);

			return;
		}

		it = List_getNext(&session->subscriptions, it);
	}
}

void MQTT_subscriptions_clear(MQTT_Broker_t * broker, MQTT_Session_t * session)
{
	(void)broker;
	DEBUGASSERT(session);

	MQTT_Subscription_t * it = List_getFirst(&session->subscriptions);
	while (it)
	{
		List_remove(&session->subscriptions, it);
		free(it->topic_filter);
		free(it);

		//Since the list is manipulated during the iteration,
		//use always the head.
		it = List_getFirst(&session->subscriptions);
	}
}

#endif

