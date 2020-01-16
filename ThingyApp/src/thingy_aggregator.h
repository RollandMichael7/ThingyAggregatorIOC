#ifndef THINGY_H
#define THINGY_H

#include <aSubRecord.h>
#include "gattlib.h"

// ----------------------- METHOD SIGNATURES -----------------------

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

// bitmap for nodes currently active and transmitting data
int g_alive[MAX_NODES];
// bitmap for nodes which are active but not transmitting data
int g_dead[MAX_NODES];

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

// Node ID of aggregator
#define AGGREGATOR_ID MAX_NODES + 1

// Bluetooth UUIDs for aggregator characteristics
#define UUID_RECV "3e520003-1368-b682-4440-d7dd234c45bc"
#define UUID_SEND "3e520002-1368-b682-4440-d7dd234c45bc"

// Connection status
#define CONNECTED 1
#define DISCONNECTED 0

// IDs for PVs
#define ID_CONNECTION 0
#define ID_STATUS 1
#define ID_RSSI 2
#define ID_BATTERY 3
#define ID_BUTTON 4
// environment sensors
#define ID_TEMPERATURE 5
#define ID_HUMIDITY 6
#define ID_PRESSURE 7
#define ID_GAS 8
#define ID_CO2 9
#define ID_TVOC 10
// environment config
#define ID_TEMP_INTERVAL 11
#define ID_PRESSURE_INTERVAL 12
#define ID_HUMID_INTERVAL 13
#define ID_GAS_MODE 14
// motion sensors
#define ID_QUATERNION_W 15
#define ID_QUATERNION_X 16
#define ID_QUATERNION_Y 17
#define ID_QUATERNION_Z 18
#define ID_ACCEL_X 19
#define ID_ACCEL_Y 20
#define ID_ACCEL_Z 21
#define ID_GYRO_X 22
#define ID_GYRO_Y 23
#define ID_GYRO_Z 24
#define ID_COMPASS_X 25
#define ID_COMPASS_Y 26
#define ID_COMPASS_Z 27
#define ID_ROLL 28
#define ID_PITCH 29
#define ID_YAW 30
// motion config
#define ID_HEADING 31
#define ID_STEP_INTERVAL 32
#define ID_TEMP_COMP_INTERVAL 33
#define ID_MAG_COMP_INTERVAL 34
#define ID_MOTION_FREQ 35
#define ID_WAKE 36
// connection config
#define ID_CONN_MIN_INTERVAL 37
#define ID_CONN_MAX_INTERVAL 38
#define ID_CONN_LATENCY 39
#define ID_CONN_TIMEOUT 40
// motion toggles
#define ID_QUATERNION_TOGGLE 41
#define ID_RAW_MOTION_TOGGLE 42
#define ID_EULER_TOGGLE 43
#define ID_HEADING_TOGGLE 44
// external pins
#define ID_EXT0 45
#define ID_EXT1 46
#define ID_EXT2 47
#define ID_EXT3 48

// Opcodes for commands
#define COMMAND_LED_TOGGLE 2
#define COMMAND_ENV_CONFIG_READ 6
#define COMMAND_ENV_CONFIG_WRITE 7
#define COMMAND_MOTION_CONFIG_READ 8
#define COMMAND_MOTION_CONFIG_WRITE 9 
#define COMMAND_SET_SENSOR 10
#define COMMAND_CONN_PARAM_READ 11
#define COMMAND_CONN_PARAM_WRITE 12
#define COMMAND_IO_READ 13
#define COMMAND_IO_WRITE 14

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
#define OPCODE_EXTIO 18

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
