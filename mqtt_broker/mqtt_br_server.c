/*******************************************************************************
 *
 *	MQTT broker internal server.
 *
 *	File:	mqtt_br_server.c
 *  Author:	Fotis Panagiotopoulos
 *  Date:	08/09/2022
 *
 *
 ******************************************************************************/

#include "mqtt_br_server.h"
#include "mqtt_broker.h"
#include "mqtt_br_session.h"
#include "mqtt_br_handler.h"
#include "mqtt_br_logger.h"
#include "network.h"
#include "list.h"
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <nuttx/config.h>
#include <sys/types.h>

#ifdef CONFIG_MQTT_BROKER

//The select timeout, in seconds.
#define MQTT_SERV_SELECT_TIMEOUT		5

//Server fatal error.
#define SERVER_ERROR(s, msg, ...)	do {  \
		(s)->status = MQTT_SERV_STOPPED;  \
		MQTT_log(LOG_ERR, msg);           \
		close((s)->sd);                   \
		(s)->sd = -1;                     \
		return;                           \
} while (0)

static void incoming_connection(MQTT_Broker_t * broker, int sd);
static void incoming_data(MQTT_Broker_t * broker, int sd);


void MQTT_server_init(MQTT_Broker_t * broker)
{
	DEBUGASSERT(broker->server.status == MQTT_SERV_STOPPED);

	MQTT_log(LOG_INFO, "Broker >> Starting...\n");

	//Create the server socket.
	broker->server.sd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (broker->server.sd < 0)
		SERVER_ERROR(&broker->server, "Broker >> Cannot create server socket.\n");

	//Set the address as reusable.
	int reuse = 1;
	if (setsockopt(broker->server.sd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0)
		SERVER_ERROR(&broker->server, "Broker >> Error setting up server socket.\n");

	//Set the socket in non-blocking mode.
	int non_blocking = 1;
	if (ioctl(broker->server.sd, FIONBIO, &non_blocking) < 0)
		SERVER_ERROR(&broker->server, "Broker >> Error setting up server socket.\n");

	//Bind the socket.
	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	addr.sin_port = htons(broker->server.port);
	if (bind(broker->server.sd, (const struct sockaddr*)&addr, sizeof(struct sockaddr_in)) < 0)
		SERVER_ERROR(&broker->server, "Broker >> Error binding server socket.\n");

	//Set the socket in listen mode.
	int backlog = SOMAXCONN;
	if (listen(broker->server.sd, backlog) < 0)
		SERVER_ERROR(&broker->server, "Broker >> Error setting server socket to listen.\n");

	//The server is ready.
	broker->server.status = MQTT_SERV_RUNNING;
}

void MQTT_server_tick(MQTT_Broker_t * broker)
{
	DEBUGASSERT(broker->server.status == MQTT_SERV_RUNNING);

	//Check if the network is available.
	if (!Network_isUp())
		SERVER_ERROR(&broker->server, "Broker >> Network is down!\n");

	//Prepare the working set for the select.
	fd_set working_set;
	FD_ZERO(&working_set);
	FD_SET(broker->server.sd, &working_set);
	int max_sd = broker->server.sd;

	//Find all active sockets, and close the dead ones.
	MQTT_Session_t * it = List_getFirst(&broker->sessions.current);
	while (it)
	{
		void * next = List_getNext(&broker->sessions.current, it);

		if (it->sd < 0)
			goto next;

		if (fcntl(it->sd, F_GETFD) < 0)
		{
			MQTT_log(LOG_DEBUG, "Broker >> Socket is dead. Dropping session...\n", it->sd);
			MQTT_session_drop(broker, it);
			goto next;
		}

		FD_SET(it->sd, &working_set);

		if (it->sd > max_sd)
			max_sd = it->sd;

next:
		it = next;
	}

	//Select any sockets that are ready.
	struct timeval timeout;
	timeout.tv_sec = MQTT_SERV_SELECT_TIMEOUT;
	timeout.tv_usec = 0;

	int available = select(max_sd + 1, &working_set, NULL, NULL, &timeout);

	//Timeout in select.
	if (available == 0)
		return;

	//Error in select.
	if (available < 0)
	{
		//EBUSY means that there are no free TCP sockets. Try again later.
		if (errno == EBUSY)
			MQTT_log(LOG_WARNING, "Broker >> TCP connections have been exhausted!\n");
		else
			SERVER_ERROR(&broker->server, "Broker >> Socket error in select.\n");
	}

	//Check all sockets that are ready.
	for (int sd = 0; (sd <= max_sd) && (available > 0); sd++)
	{
		if (!FD_ISSET(sd, &working_set))
			continue;

		available--;

		if (sd == broker->server.sd)
		{
			int new_sd;
			do {
				//Accept the new connection.
				new_sd = accept(broker->server.sd, NULL, NULL);

				if (new_sd >= 0)
				{
					//Accept the connection.
					incoming_connection(broker, new_sd);
				}
				else
				{
					//EWOULDBLOCK means that there are no
					//more connections to accept.
					if (errno != EWOULDBLOCK)
					{
						SERVER_ERROR(&broker->server, "Broker >> Error accepting new connection.\n");
					}
				}
			} while (new_sd != -1);
		}
		else
		{
			//Handle the incoming message.
			incoming_data(broker, sd);
		}
	}
}


void incoming_connection(MQTT_Broker_t * broker, int sd)
{
	//Set the socket in blocking mode.
	int non_blocking = 0;
	ioctl(sd, FIONBIO, &non_blocking);

	//Set a small timeout.
	//When select provides a socket, it should
	//already have data to process.
	struct timeval tv;
	tv.tv_sec  = 0;
	tv.tv_usec = (100 * 1000);
	setsockopt(sd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(struct timeval));

	//Enable TCP keepalive to this socket.
	//The client may disable the MQTT keepalive,
	//so the server needs a way to terminate dead
	//connections.
	int keepalive = 1;
	setsockopt(sd, SOL_SOCKET, SO_KEEPALIVE, &keepalive, sizeof(keepalive));

	MQTT_Session_t * session = MQTT_session_create(broker, sd);

	if (session == NULL)
	{
		//Cannot create a new session.
		//Close the connection.
		close(sd);
	}
}

void incoming_data(MQTT_Broker_t * broker, int sd)
{
	MQTT_Session_t * session = List_getFirst(&broker->sessions.current);
	while (session)
	{
		if (session->sd == sd)
		{
			MQTT_br_handler(broker, session);
			return;
		}

		session = List_getNext(&broker->sessions.current, session);
	}

	//No session was found for this socket.
	MQTT_log(LOG_DEBUG, "Broker >> No client session exists for the received data.\n");
	close(sd);
}

#endif

