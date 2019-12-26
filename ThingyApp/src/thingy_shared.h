// shared between all src files

#define MAX_NODES 19

// Pointer for mac address given by thingyConfig()
char g_mac_address[100];

// Flag set when the IOC has started and PVs can be scanned
int g_ioc_started;

// Bluetooth name to use when setting a custom node ID.
// A device with a custom node ID N would have the name (CUSTOM_NODE_NAME + N)
// eg. if CUSTOM_NODE_NAME = "Node" and N = 3, the device would have name "Node3"
#define CUSTOM_NODE_NAME "Node"
// comment out this line to disable custom node IDs
//#define USE_CUSTOM_IDS


#ifdef USE_CUSTOM_IDS
	// array for mapping hardware node ID to custom node ID
	int g_custom_node_ids[MAX_NODES];
#endif

#ifdef __cplusplus
	extern "C" void disconnect();
#else
	void disconnect();
#endif