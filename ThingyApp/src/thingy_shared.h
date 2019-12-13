// shared between all src files

// Pointer for mac address given by thingyConfig()
char g_mac_address[100];

// Flag set when the IOC has started and PVs can be scanned
int g_ioc_started;

// Bluetooth name to use when setting a custom node ID.
// A device with a custom node ID N would have the name (CUSTOM_NODE_NAME + N)
// eg. if CUSTOM_NODE_NAME = "Node" and N = 3, the device would have name "Node3"
#define CUSTOM_NODE_NAME "Node"