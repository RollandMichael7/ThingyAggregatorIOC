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

// bitmap for toggling LED
int led_on[MAX_NODES];


// ----------------------- CONSTANTS -----------------------

// Bluetooth UUIDs for interacting with aggregator
#define RECV_UUID "3e520003-1368-b682-4440-d7dd234c45bc"

// IDs for PVs
#define CONNECTION_ID 0
#define STATUS_ID 1
#define RSSI_ID 2
#define BATTERY_ID 3
#define BUTTON_ID 4
#define TEMPERATURE_ID 5
#define HUMIDITY_ID 6
#define PRESSURE_ID 7
#define ACCELX_ID 8
#define ACCELY_ID 9
#define ACCELZ_ID 10

// Connection status
#define CONNECTED 1
#define DISCONNECTED 0

// Indices for every response payload
#define RESP_ID 1
#define RESP_OPCODE 0

// Opcodes for responses
#define OPCODE_HUMIDITY_READING 0x05
#define OPCODE_BUTTON_REPORT 0x03

#define RESP_HUMIDITY_VAL 2

#define RESP_REPORT_BUTTON_STATE 4

// Indices for RSSI response
#define RSSI_DATA 3

