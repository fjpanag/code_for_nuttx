/*******************************************************************************
 *
 *	MQTT broker.
 *
 *	File:	mqtt_broker.c
 *  Author:	Fotis Panagiotopoulos
 *  Date:	06/09/2022
 *
 *
 ******************************************************************************/

#include "mqtt_broker.h"
#include "mqtt_br_server.h"
#include "mqtt_br_session.h"
#include "mqtt_br_queue.h"
#include "mqtt_br_logger.h"
#include "list.h"
#include "network.h"
#include "netlib.h"
#include "settings.h"
#include <sched.h>
#include <unistd.h>
#include <netinet/in.h>
#include <syslog.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <nuttx/config.h>
#include <sys/types.h>

#ifdef CONFIG_MQTT_BROKER

static int broker_th(int argc, char ** argv);

static MQTT_Broker_Status_t broker_status;


void MQTT_Broker_start()
{
	memset(&broker_status, 0, sizeof(MQTT_Broker_Status_t));

	broker_status.state = MQTT_BROKER_DOWN;

	Settings_create("mqtt.broker", SETTING_BOOL, 0);
	Settings_create("mqtt.broker.port", SETTING_INT, CONFIG_MQTT_BROKER_PORT);

	/*
	 * Note! For performance reasons, the settings
	 * are only read during the system initialization.
	 * To reload the settings, a full system reboot
	 * is required.
	 */

	int enabled = 0;
	Settings_get("mqtt.broker", SETTING_BOOL, &enabled);
	if (!enabled)
	{
		broker_status.state = MQTT_BROKER_INHIBIT;
		return;
	}

	int pid = task_create("mqtt_broker", CONFIG_MQTT_BROKER_PRIORITY, CONFIG_MQTT_BROKER_STACKSIZE, broker_th, 0);
	if (pid < 0)
	{
		syslog(LOG_CRIT, "Error starting MQTT broker task!\n");
		DEBUGVERIFY(pid);
	}
}

void MQTT_Broker_status(MQTT_Broker_Status_t * status)
{
	memcpy(status, &broker_status, sizeof(MQTT_Broker_Status_t));
}


int broker_th(int argc, char ** argv)
{
	(void)argc;
	(void)argv;

	syslog(LOG_INFO, "Initializing MQTT broker...\n");

	//Create a new broker instance.
	MQTT_Broker_t * broker = calloc(1, sizeof(MQTT_Broker_t));
	if (broker == NULL)
	{
		DEBUGASSERT(0);
		return 0;
	}

	//Start the logger.
	MQTT_logger_init();

	//Get the configured broker port.
	Settings_get("mqtt.broker.port", SETTING_INT, &broker->server.port);
	syslog(LOG_INFO, "MQTT broker port: %d\n", broker->server.port);

	//Initialize the broker lists.
	List_init(&broker->sessions.current);
	List_init(&broker->sessions.stored);
	List_init(&broker->queues.pending);
	List_init(&broker->queues.retained);

	while (1)
	{
		if (!Network_isUp())
			goto retry;


		MQTT_server_init(broker);

		broker_status.state = MQTT_BROKER_UP;
		netlib_get_ipv4addr(CONFIG_NETIF_DEV_NAME, &broker_status.ip);

		while (broker->server.status == MQTT_SERV_RUNNING)
		{
			MQTT_server_tick(broker);

			MQTT_sessions_monitor(broker);

			MQTT_queue_process(broker);

			broker_status.clients = (int)List_size(&broker->sessions.current);
		}

		MQTT_log(LOG_WARNING, "Broker >> The broker has stopped. Resetting...\n");

		broker_status.state = MQTT_BROKER_DOWN;
		broker_status.clients = 0;
		memset(&broker_status.ip, 0, sizeof(struct in_addr));

		MQTT_sessions_reset(broker);

		MQTT_queue_clear(broker);


retry:
		//Wait a bit before restarting.
		sleep(2);
	}

	free(broker);

	//The broker should never exit.
	DEBUGASSERT(0);

	return 0;
}

#endif

