#ifndef THINGY_H
#define THINGY_H

#include <aSubRecord.h>
#include "gattlib.h"

#define MAX_NODES 19

// ----------------------- METHOD SIGNATURES -----------------------

void disconnect();
aSubRecord* get_pv(int, int);
int set_pv(aSubRecord*, float);
void disconnect_node(int);

// ----------------------- PERFORMANCE VARIABLES -----------------------

// max attempts for reliable communication
#define MAX_ATTEMPTS 5

// delay (in seconds) in between attempts to reconnect to aggregator
#define RECONNECT_DELAY 3

// delay (in milliseconds) in between checks for connectivity
#define HEARTBEAT_DELAY 90000


// ----------------------- GLOBALS -----------------------

// bluetooth UUID objects for communication with aggregator
uuid_t g_send_uuid;
uuid_t g_recv_uuid;

// connection object
gatt_connection_t *gp_connection;
// flag for broken connection
int g_broken_conn;

// array for mapping hardware node ID to custom node ID
int g_custom_node_ids[MAX_NODES];

// linked list of structures to pair node/sensor IDs to PVs
typedef struct {
	aSubRecord *pv;
	int node_id;
	int pv_id;
	struct PVnode *next;
} PVnode;

PVnode* g_first_pv;

// ----------------------- CONSTANTS -----------------------

// Maximum length for a Thingy's Bluetooth name
#define MAX_NAME_LENGTH 15

// Maximum amount of nodes that can be connected to aggregator
#define AGGREGATOR_ID MAX_NODES + 1

// Bluetooth UUIDs for aggregator characteristics
#define RECV_UUID "3e520003-1368-b682-4440-d7dd234c45bc"
#define SEND_UUID "3e520002-1368-b682-4440-d7dd234c45bc"

// IDs for PVs
#define CONNECTION_ID 0
#define STATUS_ID 1
#define RSSI_ID 2
#define BATTERY_ID 3
#define BUTTON_ID 4
#define TEMPERATURE_ID 5
#define HUMIDITY_ID 6
#define PRESSURE_ID 7
#define GAS_ID 8
#define CO2_ID 9
#define TVOC_ID 10
#define TEMP_INTERVAL_ID 11
#define PRESSURE_INTERVAL_ID 12
#define HUMID_INTERVAL_ID 13
#define GAS_MODE_ID 14
#define QUATERNION_W_ID 15
#define QUATERNION_X_ID 16
#define QUATERNION_Y_ID 17
#define QUATERNION_Z_ID 18
#define ACCEL_X_ID 19
#define ACCEL_Y_ID 20
#define ACCEL_Z_ID 21
#define GYRO_X_ID 22
#define GYRO_Y_ID 23
#define GYRO_Z_ID 24
#define COMPASS_X_ID 25
#define COMPASS_Y_ID 26
#define COMPASS_Z_ID 27
#define ROLL_ID 28
#define PITCH_ID 29
#define YAW_ID 30
#define HEADING_ID 31
#define STEP_INTERVAL_ID 32
#define TEMP_COMP_INTERVAL_ID 33
#define MAG_COMP_INTERVAL_ID 34
#define MOTION_FREQ_ID 35
#define WAKE_ID 36
#define CONN_MIN_INTERVAL_ID 37
#define CONN_MAX_INTERVAL_ID 38
#define CONN_LATENCY_ID 39
#define CONN_TIMEOUT_ID 40
#define QUATERNION_TOGGLE_ID 41
#define RAW_MOTION_TOGGLE_ID 42
#define EULER_TOGGLE_ID 43
#define HEADING_TOGGLE_ID 44

// Connection status
#define CONNECTED 1
#define DISCONNECTED 0

// Opcodes for commands
#define COMMAND_LED_TOGGLE 2
#define COMMAND_ENV_CONFIG_READ 6
#define COMMAND_ENV_CONFIG_WRITE 7
#define COMMAND_MOTION_CONFIG_READ 8
#define COMMAND_MOTION_CONFIG_WRITE 9 
#define COMMAND_SET_SENSOR 10
#define COMMAND_CONN_PARAM_READ 11
#define COMMAND_CONN_PARAM_WRITE 12

// Opcodes for responses
#define OPCODE_CONNECT 1
#define OPCODE_DISCONNECT 2
#define OPCODE_BUTTON 3
#define OPCODE_BATTERY 4
#define OPCODE_RSSI 6
#define OPCODE_TEMPERATURE 7
#define OPCODE_PRESSURE 8
#define OPCODE_HUMIDITY 9
#define OPCODE_GAS 10
#define OPCODE_ENV_CONFIG 11
#define OPCODE_QUATERNIONS 12
#define OPCODE_RAW_MOTION 13
#define OPCODE_EULER 14
#define OPCODE_HEADING 15
#define OPCODE_MOTION_CONFIG 16
#define OPCODE_CONN_PARAM 17

// Indices for every response payload
#define RESP_OPCODE 0
#define RESP_ID 2

// Indices for each response type
#define RESP_CONNECT_NAME 11

#define RESP_BUTTON_STATE 4

#define RESP_BATTERY_LEVEL 3

#define RESP_RSSI_VAL 3

#define RESP_TEMPERATURE_INT 3
#define RESP_TEMPERATURE_DEC 4

#define RESP_PRESSURE_INT 3 // 4 byte int
#define RESP_PRESSURE_DEC 7

#define RESP_HUMIDITY_VAL 3

#define RESP_GAS_CO2 3 // 2 byte uint
#define RESP_GAS_TVOC 5 // 2 byte uint

#define RESP_QUATERNIONS_W 3 // 4 byte int 2Q30 fixed point
#define RESP_QUATERNIONS_X 7
#define RESP_QUATERNIONS_Y 11
#define RESP_QUATERNIONS_Z 15

#define RESP_RAW_ACCEL_X 3 // 2 byte int 6Q10 fixed point
#define RESP_RAW_ACCEL_Y 5
#define RESP_RAW_ACCEL_Z 7
#define RESP_RAW_GYRO_X 9 // 11Q5 fixed point
#define RESP_RAW_GYRO_Y 11
#define RESP_RAW_GYRO_Z 13
#define RESP_RAW_COMPASS_X 15 // 12Q4 fixed point
#define RESP_RAW_COMPASS_Y 17
#define RESP_RAW_COMPASS_Z 19

#define RESP_EULER_ROLL 3 // 4 byte int 16Q16 fixed point
#define RESP_EULER_PITCH 7
#define RESP_EULER_YAW 11

#define RESP_HEADING_VAL 3 // 4 byte int 16Q16 fixed point

#endif