
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

#include "thingy_shared.h"
#include "thingy_aggregator.h"
#include "thingy_helpers.h"

// lock for PV linked list
static pthread_mutex_t g_pv_lock = PTHREAD_MUTEX_INITIALIZER;
// lock for connection object
static pthread_mutex_t g_connlock = PTHREAD_MUTEX_INITIALIZER;

// flag to stop revive thread before cleanup
static int g_stop;
// flag for determining whether first-time setup has occured
static int g_setup = 0;
// LED toggle for all nodes
static int g_led_all;
// bitmap for toggling individual LEDs
static int g_led_nodes[MAX_NODES];
// bitmap for active nodes
static int g_active[MAX_NODES];
// bitmap for nodes currently active and transmitting data
static int g_alive[MAX_NODES];
// bitmap for nodes which are active but not transmitting data
static int g_dead[MAX_NODES];

// thread functions
static void	notification_listener();
static void notif_callback(const uuid_t*, const uint8_t*, size_t, void*);
static void	watchdog();
static void	reconnect();

static void disconnect_handler() {
	printf("WARNING: Connection to aggregator lost.\n");
	set_status(AGGREGATOR_ID, "DISCONNECTED");
	#ifdef USE_CUSTOM_IDS
		for (int i=0; i<MAX_NODES; i++) {
			g_custom_node_ids[i] = -1;
		}
	#endif
	g_broken_conn = 1;
}

// connect, initialize global UUIDs for communication, start threads for monitoring connection
static gatt_connection_t* get_connection() {
	if (gp_connection != 0) {
		return gp_connection;
	}

	pthread_mutex_lock(&g_connlock);
	// connection was made while thread waited for lock
	if (gp_connection != 0)
		return gp_connection;
	printf("Connecting to device %s...\n", g_mac_address);
	gp_connection = gattlib_connect(NULL, g_mac_address, GATTLIB_CONNECTION_OPTIONS_LEGACY_BDADDR_LE_PUBLIC | GATTLIB_CONNECTION_OPTIONS_LEGACY_BT_SEC_LOW);
	if (gp_connection != 0) {
		set_status(AGGREGATOR_ID, "CONNECTED");
		printf("Connected.\n");
	}
	g_recv_uuid = aggregator_UUID(RECV_UUID);
	g_send_uuid = aggregator_UUID(SEND_UUID);
	// register cleanup method
	signal(SIGINT, disconnect);

	// first-time setup
	if (g_setup == 0) {
		// create disconnect handler
		gattlib_register_on_disconnect(gp_connection, disconnect_handler, NULL);
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
		#ifdef USE_CUSTOM_IDS
			// initialize custom ID list as empty
			for (int i=0; i<MAX_NODES; i++)
				g_custom_node_ids[i] = -1;
		#endif
		g_setup = 1;
	}
	pthread_mutex_unlock(&g_connlock);
	return gp_connection;
}


// disconnect & cleanup
void disconnect() {
	// wait for reconnect thread to stop
	g_stop = 1;
	while (g_stop != 0)
		sleep(1);
	PVnode *node = g_first_pv; 
	PVnode *next;
	printf("Stopping notifications...\n");
	gattlib_notification_stop(gp_connection, &g_recv_uuid);
	gattlib_disconnect(gp_connection);
	printf("Disconnected from device.\n");
	exit(1);
}

// thread function to check that active nodes are still connected
static void watchdog() {
	// wait for IOC to start
	while (g_ioc_started == 0)
		sleep(1);
	// scan all PVs in case any were set before IOC started
	PVnode *node = g_first_pv;
	while (node != 0) {
		scanOnce(node->pv);
		node = node->next;
	}

	uint8_t node_id;
	uint8_t custom_id;
	while(1) {
		for (node_id=0; node_id<MAX_NODES; node_id++) {
			// only check nodes that have PVs and are assigned a node ID
			if (g_active[node_id]) {
				#ifdef USE_CUSTOM_IDS
					if (g_custom_node_ids[node_id] != -1) {
				#endif
				if (g_alive[node_id] == 0 && g_dead[node_id] == 0) {
					#ifdef USE_CUSTOM_IDS
						custom_id = g_custom_node_ids[node_id];
						printf("watchdog: Lost connection to node %d\n", custom_id);
					#else
						printf("watchdog: Lost connection to node %d\n", node_id);
					#endif
					disconnect_node(node_id);
					g_dead[node_id] = 1;
					//printf("g_dead[%d] = 1\n", node_id);
				}
				else {
					g_alive[node_id] = 0;
				}
				#ifdef USE_CUSTOM_IDS
				}
				#endif
			}
		// sleep for HEARTBEAT_DELAY ms
		usleep(HEARTBEAT_DELAY * 1000);
		}
	}
}

// thread function to attempt to reconnect to aggregator
static void reconnect() {
	while(g_ioc_started == 0)
		sleep(1);
	int i;
	while(1) {
		if (g_broken_conn) {
			printf("reconnect: Attempting reconnection to aggregator...\n");
			gp_connection = 0;
			get_connection();
			if (gp_connection != 0)
				g_broken_conn = 0;
		}
		if (g_stop) {
			printf("Reconnect thread stopped\n");
			g_stop = 0;
			return;
		}
		sleep(RECONNECT_DELAY);
	}
}

// thread function to begin listening for UUID notifications from aggregator
static void notification_listener() {
	gattlib_register_notification(gp_connection, notif_callback, NULL);
	gattlib_notification_start(gp_connection, &g_recv_uuid);
	// run forever waiting for notifications
	GMainLoop *loop = g_main_loop_new(NULL, 0);
	g_main_loop_run(loop);
}

// parse notification and save to PV(s)
static void notif_callback(const uuid_t *uuidObject, const uint8_t *resp, size_t len, void *user_data) {
	uint8_t node_id = resp[RESP_ID];
	#ifdef USE_CUSTOM_IDS
		uint8_t custom_id = g_custom_node_ids[node_id];
	#endif

	g_alive[node_id] = 1;
	if (g_dead[node_id] == 1) {
		#ifdef USE_CUSTOM_IDS
			printf("Node %d successfully reconnected.\n", custom_id);
		#else
			printf("Node %d successfully reconnected.\n", node_id);
		#endif
		set_status(node_id, "CONNECTED");
		set_connection(node_id, CONNECTED);
		g_dead[node_id] = 0;
	}

	parse_resp(resp, len);
}

// PV startup function 
// adds PV to global linked list
static long register_pv(aSubRecord *pv) {
	// initialize globals
	get_connection();
	int node_id, pv_id;
	memcpy(&node_id, pv->a, sizeof(int));
	if (node_id > (MAX_NODES-1) && node_id != AGGREGATOR_ID) {
		printf("MAX_NODES exceeded. Ignoring PVs for node %d\n", node_id);
		return;
	}
	memcpy(&pv_id, pv->b, sizeof(int));

	// add PV to list
	pthread_mutex_lock(&g_pv_lock);
	PVnode *pvnode = malloc(sizeof(PVnode));
	pvnode->node_id = node_id;
	pvnode->pv_id = pv_id;
	pvnode->pv = pv;
	pvnode->next = 0;
	if (g_first_pv == 0) {
		g_first_pv = pvnode;
	}
	else {
		PVnode *curr = g_first_pv;
		while (curr->next != 0) {
			curr = curr->next;
		}
		curr->next = pvnode;
	}
	pthread_mutex_unlock(&g_pv_lock);

	//printf("Registered %s\n", pv->name);
	if (pv_id == STATUS_ID)
		set_status(node_id, "CONNECTED");
	else if (pv_id == CONNECTION_ID) 
		set_connection(node_id, CONNECTED);
	g_active[node_id] = 1;
	return 0;
}

// LED toggle triggered by writing to LED PV
static long toggle_led(aSubRecord *pv) {
	int val;
	memcpy(&val, pv->b, sizeof(int));
	if (val != 0) {
		int node_id;
		memcpy(&node_id, pv->a, sizeof(int));
		uint8_t command[5];
		command[0] = COMMAND_LED_TOGGLE;
		if (node_id == AGGREGATOR_ID) {
			g_led_all ^= 1;
			command[1] = g_led_all;
			command[2] = 0xFF;
			command[3] = 0xFF;
			command[4] = 0xFF;
		}
		else {
			#ifdef USE_CUSTOM_IDS
				node_id = get_actual_node_id(node_id);
			#endif
			int offset = node_id % 8;
			int byte = 2 + (node_id / 8);
			g_led_nodes[node_id] ^= 1;
			command[1] = g_led_nodes[node_id];
			command[byte] = 1 << offset;
		}
		gattlib_write_char_by_uuid(gp_connection, &g_send_uuid, command, sizeof(command));
		set_pv(pv, 0);
	}
	return 0;
}

// Sensor toggle triggered by writing to SensorWrite PV
static long toggle_sensor(aSubRecord *pv) {
	int val, node_id, sensor_id;
	memcpy(&val, pv->b, sizeof(int));
	if (val != 0) {
		memcpy(&node_id, pv->a, sizeof(int));
		memcpy(&sensor_id, pv->b, sizeof(int));
		#ifdef USE_CUSTOM_IDS
			node_id = get_actual_node_id(node_id);
		#endif
		aSubRecord *sensorPV = get_pv(node_id, sensor_id);
		if (sensorPV == 0)
			return 0;
		float curVal;
		memcpy(&curVal, sensorPV->vala, sizeof(float));

		uint8_t command[4];
		command[0] = COMMAND_SET_SENSOR;
		command[1] = node_id;
		command[2] = sensor_id;
		command[3] = curVal ? 0 : 1;
		gattlib_write_char_by_uuid(gp_connection, &g_send_uuid, command, sizeof(command));
		if (sensor_id == QUATERNION_TOGGLE_ID || sensor_id == RAW_MOTION_TOGGLE_ID || sensor_id == EULER_TOGGLE_ID || sensor_id == HEADING_TOGGLE_ID)
			set_pv(sensorPV, 1);
		if (curVal != 0) {
			set_pv(sensorPV, 0);
			if (sensor_id == GAS_ID) {
				set_pv(get_pv(node_id, CO2_ID), 0);
				set_pv(get_pv(node_id, TVOC_ID), 0);
			}
			else if (sensor_id == QUATERNION_TOGGLE_ID) {
				set_pv(get_pv(node_id, QUATERNION_W_ID), 0);
				set_pv(get_pv(node_id, QUATERNION_X_ID), 0);
				set_pv(get_pv(node_id, QUATERNION_Y_ID), 0);
				set_pv(get_pv(node_id, QUATERNION_Z_ID), 0);
			}
			else if (sensor_id == RAW_MOTION_TOGGLE_ID) {
				set_pv(get_pv(node_id, ACCEL_X_ID), 0);
				set_pv(get_pv(node_id, ACCEL_Y_ID), 0);
				set_pv(get_pv(node_id, ACCEL_Z_ID), 0);
				set_pv(get_pv(node_id, GYRO_X_ID), 0);
				set_pv(get_pv(node_id, GYRO_Y_ID), 0);
				set_pv(get_pv(node_id, GYRO_Z_ID), 0);
				set_pv(get_pv(node_id, COMPASS_X_ID), 0);
				set_pv(get_pv(node_id, COMPASS_Y_ID), 0);
				set_pv(get_pv(node_id, COMPASS_Z_ID), 0);
			}
			else if (sensor_id == EULER_TOGGLE_ID) {
				set_pv(get_pv(node_id, ROLL_ID), 0);
				set_pv(get_pv(node_id, PITCH_ID), 0);
				set_pv(get_pv(node_id, YAW_ID), 0);
			}
			else if (sensor_id == HEADING_TOGGLE_ID) {
				set_pv(get_pv(node_id, HEADING_ID), 0);
			}
		}
		set_pv(pv, 0);
	}
	return 0;
}

// Environment sensor config read triggered by writing to EnvConfigRead PV
static long read_env_config(aSubRecord *pv) {
	return poll_command_pv(pv, COMMAND_ENV_CONFIG_READ);
}

// Motion sensor config read triggered by writing to MotionConfigRead PV
static long read_motion_config(aSubRecord *pv) {
	return poll_command_pv(pv, COMMAND_MOTION_CONFIG_READ);
}

// Connection param read triggered by writing to ConnParamRead PV
static long read_conn_param(aSubRecord *pv) {
	return poll_command_pv(pv, COMMAND_CONN_PARAM_READ);
}

// Environment sensor config write triggered by writing to EnvConfigWrite PV
static long write_env_config(aSubRecord *pv) {
	int val;
	memcpy(&val, pv->b, sizeof(int));
	if (val != 0) {
		int node_id;
		memcpy(&node_id, pv->a, sizeof(int));
		write_env_config_helper(node_id);
		set_pv(pv, 0);
	}
	return 0;
}

// Motion sensor config write triggered by writing to MotionConfigWrite PV
static long write_motion_config(aSubRecord *pv) {
	int val;
	memcpy(&val, pv->b, sizeof(int));
	if (val != 0) {
		int node_id;
		memcpy(&node_id, pv->a, sizeof(int));
		write_motion_config_helper(node_id);
		set_pv(pv, 0);
	}
	return 0;
}

// Connection param write triggered by writing to ConnParamWrite PV
static long write_conn_param(aSubRecord *pv) {
	int val;
	memcpy(&val, pv->b, sizeof(int));
	if (val != 0) {
		int node_id;
		memcpy(&node_id, pv->a, sizeof(int));
		write_conn_param_helper(node_id);
		set_pv(pv, 0);
	}
	return 0;
}

/* Register these symbols for use by IOC code: */
epicsRegisterFunction(register_pv);
epicsRegisterFunction(toggle_led);
epicsRegisterFunction(toggle_sensor);
epicsRegisterFunction(read_env_config);
epicsRegisterFunction(write_env_config);
epicsRegisterFunction(read_motion_config);
epicsRegisterFunction(write_motion_config);
epicsRegisterFunction(read_conn_param);
epicsRegisterFunction(write_conn_param);
