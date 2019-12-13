#include "thingy_shared.h"
#include "gattlib.h"
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/queue.h>

#define BLE_SCAN_TIMEOUT   4

typedef void (*ble_discovered_device_t)(const char* addr, const char* name);

LIST_HEAD(listhead, connection_t) g_ble_connections;
struct connection_t {
	pthread_t thread;
	char* addr;
	LIST_ENTRY(connection_t) entries;
};

static void ble_discovered_device(void *adapter, const char* addr, const char* name, void *user_data) {
	struct connection_t *connection;
	int ret;

	if (name) {
		printf("Discovered %s - '%s'\n", addr, name);
	} else {
		printf("Discovered %s\n", addr);
	}
}

int main(int argc, const char *argv[]) {
	printf("The current custom node prefix is '%s'\n\n", CUSTOM_NODE_NAME);

	const char* adapter_name;
	void* adapter;
	int ret;

	if (argc == 1) {
		adapter_name = NULL;
	} else if (argc == 2) {
		adapter_name = argv[1];
	} else {
		fprintf(stderr, "%s [<bluetooth-adapter>]\n", argv[0]);
		return 1;
	}

	LIST_INIT(&g_ble_connections);

	ret = gattlib_adapter_open(adapter_name, &adapter);
	if (ret) {
		fprintf(stderr, "ERROR: Failed to open adapter.\n");
		return 1;
	}

	ret = gattlib_adapter_scan_enable(adapter, ble_discovered_device, BLE_SCAN_TIMEOUT, NULL /* user_data */);
	if (ret) {
		fprintf(stderr, "ERROR: Failed to scan.\n");
		goto EXIT;
	}

	gattlib_adapter_scan_disable(adapter);

	puts("Scan completed");

EXIT:
	gattlib_adapter_close(adapter);
	return ret;
}
