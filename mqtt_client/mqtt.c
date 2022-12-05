/*******************************************************************************
 *
 *	MQTT client.
 *
 *	File:	mqtt.c
 *  Author:	Fotis Panagiotopoulos
 *  Date:	30/01/2022
 *
 *
 ******************************************************************************/

#include "mqtt.h"
#include "mqtt_messages.h"
#include "mqtt_helpers.h"
#include "network.h"
#include <unistd.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <time.h>
#include <sys/time.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <nuttx/config.h>
#include <sys/types.h>

#ifdef CONFIG_MQTT_VER_3_1_1
#define MQTT_VERSION	4
#else
#define MQTT_VERSION	3
#endif

#define MAX_PACKET_ID 65535

static int connection(MQTT_Client_t * client);
static int process(MQTT_Client_t * client, int * packet_type, int * packet_id);
static void deliverMessage(MQTT_Client_t * client, MQTT_Message_t * message);
static int waitfor(MQTT_Client_t * client, int packet_type, int packet_id);
static void keepalive(MQTT_Client_t * client);
static int sendPacket(MQTT_Client_t * client, size_t length);
static int readPacket(MQTT_Client_t * client);
static int getNextId(MQTT_Client_t * client);
static char isTopicMatched(char * topicFilter, char * topicName);

typedef struct {
	void * next;
	char * topic;
	MQTT_Subscriber_t subscriber;
} MQTT_Subscription_t;


void MQTT_init(MQTT_Client_t * client, const char * host, uint16_t port)
{
	memset(client, 0, sizeof(MQTT_Client_t));

	client->broker.address = strdup(host);
	client->broker.port = port;

	DEBUGASSERT(client->broker.address);

	client->nextId = 1;

	client->connection.sockfd = -1;
}

void MQTT_tick(MQTT_Client_t * client)
{
	if (MQTT_isConnected(client))
	{
		int type;
		int id;
		process(client, &type, &id);

		keepalive(client);
	}
	else if (client->connection.enabled)
	{
		if (!Network_isUp())
		{
			client->connection.timer = clock();
			return;
		}

		if ((clock() - client->connection.timer) > (CONFIG_MQTT_RECONNECT_INTERVAL * CLOCKS_PER_SEC))
		{
			client->connection.timer = clock();
			connection(client);
		}
	}
}

int MQTT_connect(MQTT_Client_t * client, const char * id, const char * username, const char * password, int cleanSession, MQTT_Message_t * lastWill)
{
	if (client->broker.address == NULL)
		return 0;

	if (client->connection.enabled)
		return 0;

	client->session.clientID = strdup(id);
	if (client->session.clientID == NULL)
		goto mem_error;

	if (username && strlen(username))
	{
		client->session.username = strdup(username);
		if (client->session.username == NULL)
			goto mem_error;

		if (password && strlen(password))
		{
			client->session.password = strdup(password);
			if (client->session.password == NULL)
				goto mem_error;
		}
	}

	client->session.clean = cleanSession;

	if (lastWill && lastWill->topic && strlen(lastWill->topic))
	{
		client->session.lastWill.topic = strdup(lastWill->topic);
		if (client->session.lastWill.topic == NULL)
			goto mem_error;

		client->session.lastWill.qos = lastWill->qos;
		client->session.lastWill.retained = lastWill->retained;

		if (lastWill->size)
		{
			client->session.lastWill.payload = malloc(lastWill->size);
			if (client->session.lastWill.payload == NULL)
				goto mem_error;

			memcpy(client->session.lastWill.payload, lastWill->payload, lastWill->size);
			client->session.lastWill.size = lastWill->size;
		}
	}


	client->connection.enabled = 1;
	client->connection.timer = clock();

	client->keepalive.timer = 0;
	client->keepalive.pending = 0;

	return 1;


mem_error:
	DEBUGASSERT(0);

	free(client->session.clientID);
	free(client->session.username);
	free(client->session.password);
	free(client->session.lastWill.topic);
	free(client->session.lastWill.payload);
	memset(&client->session, 0, sizeof(client->session));

	return 0;
}

void MQTT_connectCallback(MQTT_Client_t * client, MQTT_ConnectCB_t callback)
{
	client->connection.cb = callback;
}

time_t MQTT_isConnected(MQTT_Client_t * client)
{
	if ((clock() - client->keepalive.timer) > ((CONFIG_MQTT_KEEPALIVE_INTERVAL * 3) * CLOCKS_PER_SEC))
		client->connection.active = 0;

	if (client->connection.sockfd <= 0)
		client->connection.active = 0;

	if (!client->connection.active)
		return 0;

	time_t uptime = ((unsigned)((clock() - client->connection.timer)) / CLOCKS_PER_SEC);
	uptime += !uptime ? 1 : 0;

	return uptime;
}

void MQTT_disconnect(MQTT_Client_t * client)
{
	size_t len = MQTT_disconnect_serialize(&client->buffers.tx);
	if (len > 0)
		sendPacket(client, len);

	free(client->buffers.tx);
	client->buffers.tx = NULL;

	close(client->connection.sockfd);
	client->connection.sockfd = -1;

	client->connection.enabled = 0;
	client->connection.active = 0;
	client->connection.timer = 0;

	client->keepalive.timer = 0;
	client->keepalive.pending = 0;

	free(client->session.clientID);
	free(client->session.username);
	free(client->session.password);
	free(client->session.lastWill.topic);
	free(client->session.lastWill.payload);
	memset(&client->session, 0, sizeof(client->session));
}

int MQTT_publish(MQTT_Client_t * client, const char * topic, MQTT_QOS_t qos, int retained, void * data, size_t length)
{
	int id = 0;
	if ((qos == MQTT_QOS_1) || (qos == MQTT_QOS_2))
		id = getNextId(client);

	size_t len = MQTT_publish_serialize(&client->buffers.tx, 0, qos, retained, id, topic, (uint8_t*)data, length);
	if (len == 0)
		return 0;

	int send_err = (sendPacket(client, len) == 0);

	free(client->buffers.tx);
	client->buffers.tx = NULL;

	if (send_err)
		return 0;

	if (qos == MQTT_QOS_1)
	{
		if (!waitfor(client, PUBACK, id))
			return 0;
	}
	else if (qos == MQTT_QOS_2)
	{
		if (!waitfor(client, PUBCOMP, id))
			return 0;
	}

	return 1;
}

int MQTT_subscribe(MQTT_Client_t * client, const char * topic, MQTT_QOS_t qos, MQTT_Subscriber_t subscriber)
{
	MQTT_Subscription_t * sub = client->subscriptions;
	MQTT_Subscription_t * tail = NULL;
	while (sub)
	{
		if (strcmp(sub->topic, topic) == 0)
		{
			sub->subscriber = subscriber;
			return 1;
		}

		tail = sub;
		sub = sub->next;
	}

	int id = getNextId(client);

	size_t len = MQTT_subscribe_serialize(&client->buffers.tx, 0, id, topic, qos);
	if (len == 0)
		return 0;

	int send_err = (sendPacket(client, len) == 0);

	free(client->buffers.tx);
	client->buffers.tx = NULL;

	if (send_err)
		return 0;

	if (!waitfor(client, SUBACK, id))
		return 0;

	MQTT_Subscription_t * subscription = malloc(sizeof(MQTT_Subscription_t));
	if (subscription == NULL)
		return 0;

	subscription->next = NULL;
	subscription->topic = strdup(topic);
	subscription->subscriber = subscriber;

	if (!subscription->topic)
	{
		free(subscription);
		return 0;
	}

	if (tail)
		tail->next = subscription;
	else
		client->subscriptions = subscription;

	return 1;
}

int MQTT_unsubscribe(MQTT_Client_t * client, const char * topic)
{
	MQTT_Subscription_t * sub = client->subscriptions;
	MQTT_Subscription_t * last = NULL;
	while (sub)
	{
		if (strcmp(sub->topic, topic) == 0)
		{
			if (last)
				last->next = sub->next;
			else
				client->subscriptions = sub->next;

			free(sub->topic);
			free(sub);
			break;
		}

		last = sub;
		sub = sub->next;
	}

	int id = getNextId(client);

	size_t len = MQTTS_unsubscribe_serialize(&client->buffers.tx, 0, id, topic);
	if (len == 0)
		return 0;

	int send_err = (sendPacket(client, len) == 0);

	free(client->buffers.tx);
	client->buffers.tx = NULL;

	if (send_err)
		return 0;

	if (!waitfor(client, UNSUBACK, id))
		return 0;

	return 1;
}


int connection(MQTT_Client_t * client)
{
	/* Close any existing sockets. */

	client->connection.active = 0;

	client->keepalive.timer = 0;
	client->keepalive.pending = 0;

	if (client->connection.sockfd > 0)
	{
		close(client->connection.sockfd);
		client->connection.sockfd = -1;
	}


	/* Open a new connection. */

	struct sockaddr_in server;

	char serv[16];
	itoa(client->broker.port, serv, 10);

	struct addrinfo hints;
	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;

	struct addrinfo * info;

	if (getaddrinfo(client->broker.address, serv, &hints, &info) != 0)
		return 0;

	memcpy(&server, info->ai_addr, info->ai_addrlen);

	freeaddrinfo(info);

	if (server.sin_family != AF_INET)
		return 0;

	client->connection.sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (client->connection.sockfd < 0)
		return 0;

	struct timeval tv;
	tv.tv_sec  = CONFIG_MQTT_TIMEOUT;
	tv.tv_usec = 0;
	setsockopt(client->connection.sockfd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(struct timeval));
	setsockopt(client->connection.sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(struct timeval));

	if (connect(client->connection.sockfd, (struct sockaddr*)&server, sizeof(struct sockaddr_in)) < 0)
		goto conn_error;


	/* Create the connection data. */

	MQTT_connectOptions_t options;
	memset(&options, 0, sizeof(MQTT_connectOptions_t));
	memcpy(options.struct_id, "MQTC", 4);  //Must be MQTC!
	options.struct_version = 0;  //Must be 0!
	options.MQTTVersion = MQTT_VERSION;

	options.clientID = client->session.clientID;

	options.username = client->session.username;
	options.password = client->session.password;

	options.keepAliveInterval = CONFIG_MQTT_KEEPALIVE_INTERVAL;
	options.cleanSession = client->session.clean;

	memcpy(options.will.struct_id, "MQTW", 4);  //Must be MQTW!
	options.will.struct_version = 0;  //Must be 0!

	if (client->session.lastWill.topic)
	{
		options.willFlag = 1;
		options.will.topic = client->session.lastWill.topic;
		options.will.qos = client->session.lastWill.qos;
		options.will.retained = client->session.lastWill.retained;
		options.will.size = client->session.lastWill.size;
		options.will.payload = client->session.lastWill.payload;
	}


	/* Send the connect message. */

	size_t len = MQTT_connect_serialize(&client->buffers.tx, &options);
	if (len == 0)
		goto conn_error;

	int send_err = (sendPacket(client, len) == 0);

	free(client->buffers.tx);
	client->buffers.tx = NULL;

	if (send_err)
		goto conn_error;

	if (!waitfor(client, CONNACK, 0))
		goto conn_error;

	return 1;


conn_error:
	client->connection.active = 0;
	close(client->connection.sockfd);
	client->connection.sockfd = -1;
	return 0;
}

int process(MQTT_Client_t * client, int * packet_type, int * packet_id)
{
	int res = 0;
	*packet_type = readPacket(client);

	switch (*packet_type)
	{
		case CONNACK:
		{
			int connack = 0xFF;
			int sessionPresent = 0;
			if (!MQTT_connack_deserialize(client->buffers.rx, &sessionPresent, &connack))
				break;

			*packet_id = 0;

			if (connack != 0)
				break;

			client->connection.active = 1;

			if (!sessionPresent)
			{
				MQTT_Subscription_t * sub = client->subscriptions;
				while (sub)
				{
					MQTT_Subscription_t * next = sub->next;

					free(sub->topic);
					free(sub);

					sub = next;
				}

				client->subscriptions = NULL;
			}

			if (client->connection.cb)
			{
				if (client->connection.cb(sessionPresent) == 0)
				{
					client->connection.active = 0;
					break;
				}
			}

			res = 1;
			break;
		}

		case PUBACK:
		{
			int duplicate;
			int type;
			if (!MQTT_ack_deserialize(client->buffers.rx, &type, &duplicate, packet_id))
				break;

			res = 1;
			break;
		}

		case SUBACK:
		{
			int QoS = 0x80;
			if (!MQTT_suback_deserialize(client->buffers.rx, packet_id, &QoS))
				break;

			if (QoS == 0x80)
				break;

			res = 1;
			break;
		}

		case UNSUBACK:
		{
			if (!MQTT_unsuback_deserialize(client->buffers.rx, packet_id))
				break;

			res = 1;
			break;
		}

		case PUBLISH:
		{
			MQTT_Message_t msg;
			if (!MQTT_publish_deserialize(client->buffers.rx, &msg.dup, &msg.qos, &msg.retained, &msg.id, &msg.topic, (uint8_t**)&msg.payload, &msg.size))
				break;

			*packet_id = msg.id;

			deliverMessage(client, &msg);

			free(msg.topic);

			if (msg.qos != MQTT_QOS_0)
			{
				size_t len = 0;
				if (msg.qos == MQTT_QOS_1)
					len = MQTT_ack_serialize(&client->buffers.tx, PUBACK, 0, msg.id);
				else if (msg.qos == MQTT_QOS_2)
					len = MQTT_ack_serialize(&client->buffers.tx, PUBREC, 0, msg.id);

				if (len == 0)
					break;

				int send_err = (sendPacket(client, len) == 0);

				free(client->buffers.tx);
				client->buffers.tx = NULL;

				if (send_err)
					break;
			}

			res = 1;
			break;
		}

		case PUBREC:
		{
			int duplicate;
			int type;
			if (!MQTT_ack_deserialize(client->buffers.rx, &type, &duplicate, packet_id))
				break;

			size_t len = MQTT_ack_serialize(&client->buffers.tx, PUBREL, 0, *packet_id);
			if (len == 0)
				break;

			int send_err = (sendPacket(client, len) == 0);

			free(client->buffers.tx);
			client->buffers.tx = NULL;

			if (send_err)
				break;

			res = 1;
			break;
		}

		case PUBCOMP:
		{
			int duplicate;
			int type;
			if (!MQTT_ack_deserialize(client->buffers.rx, &type, &duplicate, packet_id))
				break;

			res = 1;
			break;
		}

		case PINGRESP:
		{
			client->keepalive.pending = 0;
			res = 1;
			break;
		}

		default:
		{
			*packet_type = 0;
			*packet_id = 0;
			break;
		}
	}

	free(client->buffers.rx);
	client->buffers.rx = NULL;

	return res;
}

void deliverMessage(MQTT_Client_t * client, MQTT_Message_t * message)
{
	MQTT_Subscription_t * sub = client->subscriptions;
	while (sub)
	{
		if ((strcmp(sub->topic, message->topic) == 0) || isTopicMatched(sub->topic, message->topic))
		{
			if (sub->subscriber != NULL)
				sub->subscriber(message);
		}

		sub = sub->next;
	}
}

int waitfor(MQTT_Client_t * client, int packet_type, int packet_id)
{
	clock_t start = clock();
	while ((clock() - start) < (CONFIG_MQTT_TIMEOUT * CLOCKS_PER_SEC))
	{
		int type = 0;
		int id = 0;
		int res = process(client, &type, &id);

		if (packet_type == type)
		{
			if ((packet_id == 0) || (packet_id == id))
				return res;
		}
	}

	return 0;
}

void keepalive(MQTT_Client_t * client)
{
	if (!client->keepalive.pending)
	{
		if ((clock() - client->keepalive.timer) > (CONFIG_MQTT_KEEPALIVE_INTERVAL * CLOCKS_PER_SEC))
		{
			size_t len = MQTT_pingreq_serialize(&client->buffers.tx);
			if (len > 0)
			{
				sendPacket(client, len);
				client->keepalive.pending = clock();

				free(client->buffers.tx);
				client->buffers.tx = NULL;
			}
		}
	}
	else if ((clock() - client->keepalive.pending) > (CONFIG_MQTT_TIMEOUT * CLOCKS_PER_SEC))
	{
		client->keepalive.pending = 0;

		client->connection.active = 0;
		close(client->connection.sockfd);
		client->connection.sockfd = -1;
	}
}

int sendPacket(MQTT_Client_t * client, size_t length)
{
	if (client->connection.sockfd < 0)
		return 0;

	if (send(client->connection.sockfd, client->buffers.tx, length, 0) != (ssize_t)length)
	{
		client->connection.active = 0;
		close(client->connection.sockfd);
		client->connection.sockfd = -1;
		return 0;
	}

	client->keepalive.timer = clock();

	return 1;
}

int readPacket(MQTT_Client_t * client)
{
	if (client->connection.sockfd <= 0)
		return 0;


	struct timeval tv;

	//1. Read the header byte.
	uint8_t header_byte;

	tv.tv_sec  = 0;
	tv.tv_usec = 10 * 1000;
	setsockopt(client->connection.sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(struct timeval));

	if (recv(client->connection.sockfd, &header_byte, 1, 0) != 1)
		return 0;


	//2. Read and parse the remaining packet length.
	char size[4];

	tv.tv_sec  = CONFIG_MQTT_TIMEOUT;
	tv.tv_usec = 0;
	setsockopt(client->connection.sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(struct timeval));

	int remainingSize = 0;
	int multiplier = 1;
	int size_len = 0;
	do {
		//Remaining length field can be up to 4 bytes long.
		if (size_len >= 4)
			goto rx_error;

		if (recv(client->connection.sockfd, &size[size_len], 1, 0) != 1)
			goto rx_error;

		remainingSize += (size[size_len] & 0x7F) * multiplier;

		multiplier <<= 7;
	} while ((size[size_len++] >> 7) & 0x01);  //Checks the continuation bit.


	//3. Allocate a new buffer and copy the message.
	client->buffers.rx = malloc(1 + size_len + remainingSize);
	if (!client->buffers.rx)
		goto rx_error;

	client->buffers.rx[0] = header_byte;
	for (int i = 0; i < size_len; i++)
		client->buffers.rx[1 + i] = size[i];


	//4. Read the rest of the packet.
	if (remainingSize > 0)
	{
		if (recv(client->connection.sockfd, client->buffers.rx + 1 + size_len, remainingSize, MSG_WAITALL) != remainingSize)
			goto rx_error;
	}

	MQTT_Header_t header;
	header.byte = client->buffers.rx[0];
	return header.bits.type;


rx_error:
	client->connection.active = 0;
	close(client->connection.sockfd);
	client->connection.sockfd = -1;

	free(client->buffers.rx);
	client->buffers.rx = NULL;

	return 0;
}

int getNextId(MQTT_Client_t * client)
{
	int id = client->nextId;

	client->nextId++;
	client->nextId %= MAX_PACKET_ID;

	if (client->nextId == 0)
		client->nextId++;

	return id;
}

char isTopicMatched(char * topicFilter, char * topicName)
{
	char * curf = topicFilter;
	char * curn = topicName;
	char * curn_end = curn + strlen(topicName);

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

