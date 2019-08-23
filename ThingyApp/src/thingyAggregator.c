
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <ctype.h>
#include <math.h>
#include <pthread.h>
#include <signal.h>

#include <dbAccess.h>
#include <dbDefs.h>
#include <dbFldTypes.h>
#include <dbScan.h>

#include <registryFunction.h>
#include <aSubRecord.h>
#include <waveformRecord.h>
#include <epicsExport.h>
#include <epicsTime.h>
#include <callback.h>

#include <glib.h>
#include "gattlib.h"

#include "thingyAggregator.h"
#include "thingy_helpers.h"


// lock for PV linked list
pthread_mutex_t pv_lock = PTHREAD_MUTEX_INITIALIZER;
// lock for connection object
pthread_mutex_t connlock = PTHREAD_MUTEX_INITIALIZER;

// flag to stop revive thread before cleanup
int stop;
// flag for determining whether monitoring threads have started
int monitoring = 0;
// LED toggle
int led_on;
// bitmap for active nodes
int active[MAX_NODES];
// bitmap for nodes currently active and transmitting data
int alive[MAX_NODES];
// bitmap for nodes which are active but not transmitting data
int dead[MAX_NODES];

static long register_pv(aSubRecord*);

// thread functions
static void	notification_listener();
static void notif_callback(const uuid_t*, const uint8_t*, size_t, void*);
static void	watchdog();
static void	reconnect();

// linked list of structures to pair node/sensor IDs to PVs
typedef struct {
	aSubRecord *pv;
	int nodeID;
	int pvID;
	struct PVnode *next;
} PVnode;

PVnode* firstPV = 0;

static void disconnect_handler() {
	printf("WARNING: Connection to aggregator lost.\n");
	set_status(AGGREGATOR_ID, "DISCONNECTED");
	broken_conn = 1;
}

// connect, initialize global UUIDs for communication, start threads for monitoring connection
static gatt_connection_t* get_connection() {
	if (connection != 0) {
		return connection;
	}

	pthread_mutex_lock(&connlock);
	// connection was made while thread waited for lock
	if (connection != 0)
		return connection;
	printf("Connecting to device %s...\n", mac_address);
	connection = gattlib_connect(NULL, mac_address, GATTLIB_CONNECTION_OPTIONS_LEGACY_BDADDR_LE_PUBLIC | GATTLIB_CONNECTION_OPTIONS_LEGACY_BT_SEC_LOW);
	if (connection != 0) {
		set_status(AGGREGATOR_ID, "CONNECTED");
		printf("Connected.\n");
	}
	recv_uuid = aggregatorUUID(RECV_UUID);
	send_uuid = aggregatorUUID(SEND_UUID);
	// register cleanup method
	signal(SIGINT, disconnect);

	// turn on monitoring threads if necessary
	if (monitoring == 0) {
		// create disconnect handler
		gattlib_register_on_disconnect(connection, disconnect_handler, NULL);
		// start notification listener thread
		printf("Starting notification listener thread...\n");
		pthread_t listener;
		pthread_create(&listener, NULL, &notification_listener, NULL);
		// start watchdog thread
		printf("Starting watchdog thread...\n");
		pthread_t watchdog_pid;
		pthread_create(&watchdog_pid, NULL, &watchdog, NULL);
		// start reconnect thread
		printf("Starting reconnection thread...\n");
		pthread_t necromancer;
		pthread_create(&necromancer, NULL, &reconnect, NULL);
		monitoring = 1;
	}
	pthread_mutex_unlock(&connlock);
	return connection;
}


// disconnect & cleanup
void disconnect() {
	// wait for reconnect thread to stop
	stop = 1;
	while (stop != 0)
		sleep(1);
	PVnode *node = firstPV; 
	PVnode *next;
	printf("Stopping notifications...\n");
	gattlib_notification_stop(connection, &recv_uuid);
	gattlib_disconnect(connection);
	printf("Disconnected from device.\n");
	exit(1);
}

// thread function to check that active nodes are still connected
static void watchdog() {
	// wait for IOC to start
	while (ioc_started == 0)
		sleep(1);
	// scan all PVs in case any were set before IOC started
	PVnode *node = firstPV;
	while (node != 0) {
		scanOnce(node->pv);
		node = node->next;
	}

	int i;
	while(1) {
		for (i=0; i<MAX_NODES; i++) {
			if (active[i]) {
				if (alive[i] == 0 && dead[i] == 0) {
					printf("watchdog: Lost connection to node %d\n", i);
					// mark PVs null
					nullify_node(i);
					set_status(i, "DISCONNECTED");
					set_connection(i, DISCONNECTED);
					dead[i] = 1;
				}
				else {
					alive[i] = 0;
				}
			}
		// sleep for HEARTBEAT_DELAY ms
		usleep(HEARTBEAT_DELAY * 1000);
		}
	}
}

// thread function to attempt to reconnect to aggregator
static void reconnect() {
	while(ioc_started == 0)
		sleep(1);
	int i;
	while(1) {
		if (broken_conn) {
			printf("reconnect: Attempting reconnection to aggregator...\n");
			connection = 0;
			get_connection();
			if (connection != 0)
				broken_conn = 0;
		}
		if (stop) {
			printf("Reconnect thread stopped\n");
			stop = 0;
			return;
		}
		sleep(RECONNECT_DELAY);
	}
}

// thread function to begin listening for UUID notifications from aggregator
static void notification_listener() {
	gattlib_register_notification(connection, notif_callback, NULL);
	gattlib_notification_start(connection, &recv_uuid);
	// run forever waiting for notifications
	GMainLoop *loop = g_main_loop_new(NULL, 0);
	g_main_loop_run(loop);
}

// parse notification and save to PV(s)
static void notif_callback(const uuid_t *uuidObject, const uint8_t *resp, size_t len, void *user_data) {
	uint8_t nodeID = resp[RESP_ID];
	alive[nodeID] = 1;
	if (dead[nodeID] == 1) {
		printf("Node %d successfully reconnected.\n", nodeID);
		set_status(nodeID, "CONNECTED");
		set_connection(nodeID, CONNECTED);
		dead[nodeID] = 0;
	}

	parse_resp(resp, len);
}

// PV startup function 
// adds PV to global linked list
long register_pv(aSubRecord *pv) {
	// initialize globals
	get_connection();
	int nodeID, pvID;
	memcpy(&nodeID, pv->a, sizeof(int));
	if (nodeID > (MAX_NODES-1) && nodeID != AGGREGATOR_ID) {
		printf("MAX_NODES exceeded. Ignoring node %d\n", nodeID);
		return;
	}
	memcpy(&pvID, pv->b, sizeof(int));

	// add PV to list
	pthread_mutex_lock(&pv_lock);
	PVnode *pvnode = malloc(sizeof(PVnode));
	pvnode->nodeID = nodeID;
	pvnode->pvID = pvID;
	pvnode->pv = pv;
	pvnode->next = 0;
	if (firstPV == 0) {
		firstPV = pvnode;
	}
	else {
		PVnode *curr = firstPV;
		while (curr->next != 0) {
			curr = curr->next;
		}
		curr->next = pvnode;
	}
	pthread_mutex_unlock(&pv_lock);

	printf("Registered %s\n", pv->name);
	if (pvID == STATUS_ID)
		set_status(nodeID, "CONNECTED");
	else if (pvID == CONNECTION_ID) 
		set_connection(nodeID, CONNECTED);
	active[nodeID] = 1;
	return 0;
}

// LED toggle triggered by writing to LED PV
long toggle_led(aSubRecord *pv) {
	int val;
	memcpy(&val, pv->b, sizeof(int));
	if (val != 0) {
		uint8_t command[5];
		led_on ^= 1;
		command[0] = COMMAND_LED_TOGGLE;
		command[1] = led_on;
		command[2] = 0xFF;
		command[3] = 0xFF;
		command[4] = 0xFF;
		gattlib_write_char_by_uuid(connection, &send_uuid, command, sizeof(command));
		set_pv(pv, 0);
	}
	return 0;
}

// Sensor toggle triggered by writing to SensorWrite PV
static long toggle_sensor(aSubRecord *pv) {
	int val, nodeID, sensorID;
	memcpy(&val, pv->b, sizeof(int));
	if (val != 0) {
		memcpy(&nodeID, pv->a, sizeof(int));
		memcpy(&sensorID, pv->b, sizeof(int));
		aSubRecord *sensorPV = get_pv(nodeID, sensorID);
		if (sensorPV == 0)
			return 0;
		float x;
		memcpy(&x, sensorPV->vala, sizeof(float));

		uint8_t command[4];
		command[0] = COMMAND_ENV_SET_SENSOR;
		command[1] = nodeID;
		command[2] = sensorID;
		command[3] = x ? 0 : 1;
		gattlib_write_char_by_uuid(connection, &send_uuid, command, sizeof(command));
		if (x != 0) {
			set_pv(sensorPV, 0);
			if (sensorID == GAS_ID) {
				sensorPV = get_pv(nodeID, CO2_ID);
				set_pv(sensorPV, 0);
				sensorPV = get_pv(nodeID, TVOC_ID);
				set_pv(sensorPV, 0);
			}
		}
		set_pv(pv, 0);
	}
	return 0;
}

// Start environment sensor config read; triggered by writing to EnvConfigRead PV
long read_env_config(aSubRecord *pv) {
	int val;
	memcpy(&val, pv->b, sizeof(int));
	if (val != 0) {
		int nodeID;
		memcpy(&nodeID, pv->a, sizeof(int));
		uint8_t command[2];
		command[0] = COMMAND_ENV_CONFIG_READ;
		command[1] = nodeID;
		gattlib_write_char_by_uuid(connection, &send_uuid, command, sizeof(command));
		set_pv(pv, 0);
	}
	return 0;
}

// Environment sensor write triggered by writing to EnvConfigWrite PV
long write_env_config(aSubRecord *pv) {
	int val;
	memcpy(&val, pv->b, sizeof(int));
	if (val != 0) {
		int nodeID;
		memcpy(&nodeID, pv->a, sizeof(int));
		write_env_config_helper(nodeID);
		set_pv(pv, 0);
	}
}

// Start connection param read; triggered by writing to ConnParamRead PV
long read_conn_param(aSubRecord *pv) {
	int val;
	memcpy(&val, pv->b, sizeof(int));
	if (val != 0) {
		int nodeID;
		memcpy(&nodeID, pv->a, sizeof(int));
		uint8_t command[2];
		command[0] = COMMAND_CONN_PARAM_READ;
		command[1] = nodeID;
		gattlib_write_char_by_uuid(connection, &send_uuid, command, sizeof(command));
		set_pv(pv, 0);
	}
	return 0;
}

// ---------------------------- Helper functions ----------------------------

// fetch PV from linked list given node/PV IDs
aSubRecord* get_pv(int nodeID, int pvID) {
	PVnode *node = firstPV;
	while (node != 0) {
		if (node->nodeID == nodeID && node->pvID == pvID) {
			return node->pv;
		}
		node = node->next;
	}
	printf("WARNING: No PV for node %d sensor %d\n", nodeID, pvID);
	return 0;
}

// mark dead nodes through PV values
void nullify_node(int id) {
	float null = -1;
	int pvID;
	PVnode *node = firstPV;
	aSubRecord *pv;
	while (node != 0) {
		pvID = node->pvID;
		if (node->nodeID == id && pvID != CONNECTION_ID && pvID != STATUS_ID) {
			if (pvID == BUTTON_ID)
				set_pv(node->pv, 0);
			else
				set_pv(node->pv, null);
		}
		node = node->next;
	}
}

/* Register these symbols for use by IOC code: */
epicsRegisterFunction(register_pv);
epicsRegisterFunction(toggle_led);
epicsRegisterFunction(toggle_sensor);
epicsRegisterFunction(read_env_config);
epicsRegisterFunction(write_env_config);
epicsRegisterFunction(read_conn_param);
