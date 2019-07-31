#include <aSubRecord.h>
#include "gattlib.h"

#define MAX_NODES 19
#define AGGREGATOR_ID MAX_NODES + 1

// max attempts for reliable communication
#define MAX_ATTEMPTS 5

// delay (in seconds) in between attempts to reconnect nodes
#define RECONNECT_DELAY 3

// delay (in milliseconds) in between checks for connectivity
#define HEARTBEAT_DELAY 250

void disconnect();
aSubRecord* get_pv(int, int);

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