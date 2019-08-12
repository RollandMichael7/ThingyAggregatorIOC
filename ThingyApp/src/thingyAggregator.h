#include <aSubRecord.h>
#include "gattlib.h"

// ----------------------- METHOD SIGNATURES -----------------------

void disconnect();
aSubRecord* get_pv(int, int);

// ----------------------- PERFORMANCE VARIABLES -----------------------

// max attempts for reliable communication
#define MAX_ATTEMPTS 5

// delay (in seconds) in between attempts to reconnect to aggregator
#define RECONNECT_DELAY 3

// delay (in milliseconds) in between checks for connectivity
#define HEARTBEAT_DELAY 90000


// ----------------------- SHARED GLOBALS -----------------------

// Pointer for mac address given by thingyConfig()
char mac_address[100];

// Flag set when the IOC has started and PVs can be scanned
int ioc_started;

// bluetooth UUIDs for communication with bridge
uuid_t send_uuid;
uuid_t recv_uuid;

// connection object
gatt_connection_t *connection;
// flag for broken connection
int broken_conn;


// ----------------------- CONSTANTS -----------------------

// Maximum amount of nodes that can be connected to aggregator
#define MAX_NODES 19
#define AGGREGATOR_ID MAX_NODES + 1

// Bluetooth UUIDs for interacting with aggregator
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
#define QUATERNION_W_ID 11
#define QUATERNION_X_ID 12
#define QUATERNION_Y_ID 13
#define QUATERNION_Z_ID 14
#define ACCEL_X_ID 15
#define ACCEL_Y_ID 16
#define ACCEL_Z_ID 17
#define GYRO_X_ID 18
#define GYRO_Y_ID 19
#define GYRO_Z_ID 20
#define COMPASS_X_ID 21
#define COMPASS_Y_ID 22
#define COMPASS_Z_ID 23
#define ROLL_ID 24
#define PITCH_ID 25
#define YAW_ID 26
#define HEADING_ID 27

// Connection status
#define CONNECTED 1
#define DISCONNECTED 0

// Indices for every response payload
#define RESP_OPCODE 0
#define RESP_ID 2

// Opcodes for responses
#define OPCODE_BUTTON 3
#define OPCODE_BATTERY 4
#define OPCODE_RSSI 6
#define OPCODE_TEMPERATURE 7
#define OPCODE_PRESSURE 8
#define OPCODE_HUMIDITY 9
#define OPCODE_GAS 10
#define OPCODE_QUATERNIONS 11
#define OPCODE_RAW_MOTION 12
#define OPCODE_EULER 13
#define OPCODE_HEADING 14

// Indices for each response type
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