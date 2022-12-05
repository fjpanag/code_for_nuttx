/*******************************************************************************
 *
 *	MQTT broker connection session.
 *
 *	File:	mqtt_br_session.c
 *  Author:	Fotis Panagiotopoulos
 *  Date:	08/09/2022
 *
 *
 ******************************************************************************/

#include "mqtt_br_session.h"
#include "mqtt_broker.h"
#include "mqtt_br_subscription.h"
#include "mqtt_br_queue.h"
#include "mqtt_br_logger.h"
#include "mqtt_br_types.h"
#include "list.h"
#include <unistd.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <nuttx/config.h>
#include <sys/types.h>

#ifdef CONFIG_MQTT_BROKER

static void session_store(MQTT_Broker_t * broker, MQTT_Session_t * session);
static int session_retrieve(MQTT_Broker_t * broker, MQTT_Session_t * session);
static void session_free(MQTT_Broker_t * broker, MQTT_Session_t * session);


void MQTT_sessions_monitor(MQTT_Broker_t * broker)
{
	MQTT_Session_t * session = List_getFirst(&broker->sessions.current);
	while (session)
	{
		void * next = List_getNext(&broker->sessions.current, session);

		//Check for inactive and expired sessions.
		time_t timeout;
		if (session->active)
			timeout = (session->keepalive * 2);
		else
			timeout = CONFIG_MQTT_BROKER_INACTIVE_TIMEOUT;

		if (timeout != 0)
		{
			if ((clock() - session->timer) > (timeout * CLOCKS_PER_SEC))
			{
				MQTT_log(LOG_INFO, "Broker >> Session <%s:%d> timeout.\n", session->id ? session->id : "anonymous", session->sd);
				MQTT_session_drop(broker, session);
			}
		}

		session = next;
	}
}

void MQTT_sessions_reset(MQTT_Broker_t * broker)
{
	MQTT_log(LOG_DEBUG, "Broker >> Dropping all sessions...\n");

	MQTT_Session_t * session = List_getFirst(&broker->sessions.current);
	while (session)
	{
		MQTT_session_drop(broker, session);

		//Since the list is manipulated during the iteration,
		//use always the head.
		session = List_getFirst(&broker->sessions.current);
	}
}


MQTT_Session_t * MQTT_session_create(MQTT_Broker_t * broker, int sd)
{
	DEBUGASSERT(sd >= 0);

	MQTT_log(LOG_DEBUG, "Broker >> Creating new session, sd: %d\n", sd);

	size_t sessions = List_size(&broker->sessions.current);
	if (sessions >= CONFIG_MQTT_BROKER_MAX_SESSIONS)
	{
		MQTT_log(LOG_DEBUG, "Broker >> Cannot create session, sessions limit exceeded!\n");
		return NULL;
	}

	MQTT_Session_t * session = calloc(1, sizeof(MQTT_Session_t));
	if (session == NULL)
	{
		MQTT_log(LOG_DEBUG, "Broker >> Cannot create session, memory error.\n");
		return NULL;
	}

	session->id = NULL;
	session->active = 0;

	session->sd = sd;
	session->clean = 0;
	session->keepalive = 0;
	session->timer = clock();

	memset(&session->in_flight, 0, sizeof(session->in_flight));

	memset(&session->lwt, 0, sizeof(MQTT_Message_t));

	List_init(&session->subscriptions);

	List_add(&broker->sessions.current, session);

	return session;
}

void MQTT_session_activate(MQTT_Broker_t * broker, MQTT_Session_t * session, char * client_id, time_t keepalive, int clean, MQTT_Message_t * lwt, int * present)
{
	DEBUGASSERT(session->id == NULL);
	DEBUGASSERT(session->active == 0);
	DEBUGASSERT(session->sd >= 0);

	MQTT_log(LOG_INFO, "Broker >> New client connected: <%s:%d>\n", client_id ? client_id : "anonymous", session->sd);

	if (client_id && strlen(client_id))
		session->id = client_id;

	session->clean = clean;
	session->keepalive = keepalive;

	session->active = 1;
	session->timer = clock();

	if ((lwt != NULL) && (lwt->topic != NULL))
	{
		DEBUGASSERT(strlen(lwt->topic));
		memcpy(&session->lwt, lwt, sizeof(MQTT_Message_t));
	}

	*present = session_retrieve(broker, session);

	if (session->clean)
	{
		*present = 0;
		memset(&session->in_flight, 0, sizeof(session->in_flight));
		MQTT_subscriptions_clear(broker, session);
	}
}

void MQTT_session_ping(MQTT_Broker_t * broker, MQTT_Session_t * session)
{
	(void)broker;
	DEBUGASSERT(session->active);

	session->timer = clock();
}

void MQTT_session_close(MQTT_Broker_t * broker, MQTT_Session_t * session)
{
	DEBUGASSERT(session->active);
	DEBUGASSERT(session->sd >= 0);

	List_remove(&broker->sessions.current, session);

	MQTT_log(LOG_INFO, "Broker >> Closing session <%s:%d>.\n", session->id ? session->id : "anonymous", session->sd);

	session->active = 0;
	session->keepalive = 0;
	session->timer = 0;

	close(session->sd);
	session->sd = -1;

	MQTT_message_free(&session->lwt);
	memset(&session->lwt, 0, sizeof(MQTT_Message_t));

#ifdef CONFIG_MQTT_BROKER_STORE_SESSIONS
	if (session->id && !session->clean)
	{
		DEBUGASSERT(strlen(session->id));
		session_store(broker, session);
	}
	else
#endif
	{
		session_free(broker, session);
	}
}

void MQTT_session_drop(MQTT_Broker_t * broker, MQTT_Session_t * session)
{
	DEBUGASSERT(session->sd >= 0);

	List_remove(&broker->sessions.current, session);

	MQTT_log(LOG_INFO, "Broker >> Dropping session <%s:%d>.\n", session->id ? session->id : "anonymous", session->sd);

	session->active = 0;
	session->keepalive = 0;
	session->timer = 0;

	close(session->sd);
	session->sd = -1;

	if (session->lwt.topic)
	{
		DEBUGASSERT(strlen(session->lwt.topic));

		MQTT_log(LOG_DEBUG, "Broker >> Publishing LWT for <%s:%d> on [%s].\n", session->id ? session->id : "anonymous", session->sd, session->lwt.topic);

		if (!MQTT_queue_add(broker, &session->lwt))
			MQTT_message_free(&session->lwt);

		memset(&session->lwt, 0, sizeof(MQTT_Message_t));
	}

#ifdef CONFIG_MQTT_BROKER_STORE_SESSIONS
	if (session->id && !session->clean)
	{
		DEBUGASSERT(strlen(session->id));
		session_store(broker, session);
	}
	else
#endif
	{
		session_free(broker, session);
	}
}


void session_store(MQTT_Broker_t * broker, MQTT_Session_t * session)
{
	DEBUGASSERT(!session->active);
	DEBUGASSERT(session->id && strlen(session->id));
	DEBUGASSERT(!session->clean);

	size_t stored = List_size(&broker->sessions.stored);
	if (stored >= CONFIG_MQTT_BROKER_MAX_STORED_SESSIONS)
	{
		MQTT_Session_t * del = List_getFirst(&broker->sessions.stored);
		List_remove(&broker->sessions.stored, del);

		MQTT_log(LOG_DEBUG, "Broker >> Deleting old stored session: <%s:%d>\n", del->id ? del->id : "anonymous", del->sd);

		session_free(broker, del);
	}

	MQTT_log(LOG_DEBUG, "Broker >> Storing inactive session: <%s:%d>\n", session->id ? session->id : "anonymous", session->sd);

	List_add(&broker->sessions.stored, session);
}

int session_retrieve(MQTT_Broker_t * broker, MQTT_Session_t * session)
{
	if (session->id == NULL)
		return 0;

	DEBUGASSERT(strlen(session->id));

	MQTT_Session_t * it = NULL;

	//First check if this session substitutes another
	//active one that has not been dropped yet.
	it = List_getFirst(&broker->sessions.current);
	while (it)
	{
		if (session != it)
		{
			if (it->id && (strcmp(session->id, it->id) == 0))
			{
				DEBUGASSERT(it->active);
				DEBUGASSERT(it->sd >= 0);

				List_remove(&broker->sessions.current, it);

				MQTT_log(LOG_DEBUG, "Broker >> Session <%s:%d> substitutes existing session <%s:%d>.\n",
						 session->id ? session->id : "anonymous", session->sd,
						 it->id ? it->id : "anonymous", it->sd);

				memcpy(&session->in_flight, &it->in_flight, sizeof(session->in_flight));

				memcpy(&session->subscriptions, &it->subscriptions, sizeof(List_t));
				List_clear(&it->subscriptions);

				it->active = 0;
				it->keepalive = 0;
				it->timer = 0;

				close(it->sd);
				it->sd = -1;

				session_free(broker, it);

				return 1;
			}
		}

		it = List_getNext(&broker->sessions.current, it);
	}


	//Then check the stored session.
	it = List_getFirst(&broker->sessions.stored);
	while (it)
	{
		DEBUGASSERT(!it->active);
		DEBUGASSERT(it->id && strlen(it->id));
		DEBUGASSERT(it->sd == -1);

		if (strcmp(session->id, it->id) == 0)
		{
			List_remove(&broker->sessions.stored, it);

			MQTT_log(LOG_DEBUG, "Restoring state for session <%s:%d>.", session->id ? session->id : "anonymous", session->sd);

			memcpy(&session->in_flight, &it->in_flight, sizeof(session->in_flight));

			memcpy(&session->subscriptions, &it->subscriptions, sizeof(List_t));
			List_clear(&it->subscriptions);

			session_free(broker, it);

			return 1;
		}

		it = List_getNext(&broker->sessions.stored, it);
	}

	return 0;
}

void session_free(MQTT_Broker_t * broker, MQTT_Session_t * session)
{
	(void)broker;
	DEBUGASSERT(!session->active);
	DEBUGASSERT(session->sd == -1);
	DEBUGASSERT(session->next == NULL);

	free(session->id);
	MQTT_message_free(&session->lwt);
	MQTT_subscriptions_clear(broker, session);

	free(session);
}

#endif

