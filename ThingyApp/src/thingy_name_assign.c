#include <stdio.h>
#include <stdlib.h>
#include "gattlib.h"

#define THINGY_NAME_UUID "EF680101-9B35-4933-9B10-52FFA9740042"

// taken from gattlib; convert string to 128 bit uint
static uint128_t str_to_128t(const char *string) {
	uint32_t data0, data4;
	uint16_t data1, data2, data3, data5;
	uint128_t u128;
	uint8_t *val = (uint8_t *) &u128;

	if(sscanf(string, "%08x-%04hx-%04hx-%04hx-%08x%04hx",
				&data0, &data1, &data2,
				&data3, &data4, &data5) != 6) {
		printf("Parse of UUID %s failed\n", string);
		memset(&u128, 0, sizeof(uint128_t));
		return u128;
	}

	data0 = htonl(data0);
	data1 = htons(data1);
	data2 = htons(data2);
	data3 = htons(data3);
	data4 = htonl(data4);
	data5 = htons(data5);

	memcpy(&val[0], &data0, 4);
	memcpy(&val[4], &data1, 2);
	memcpy(&val[6], &data2, 2);
	memcpy(&val[8], &data3, 2);
	memcpy(&val[10], &data4, 4);
	memcpy(&val[14], &data5, 2);

	return u128;
}

// construct a 128 bit UUID object from string
uuid_t string_to_uuid(const char *str) {
	uint128_t uuid_val = str_to_128t(str);
	uuid_t uuid = {.type=SDP_UUID128, .value.uuid128=uuid_val};
	return uuid;
}

int main(int argc, char const *argv[])
{
	if (argc != 3) {
		printf("%s [bluetooth address] [name]\n", argv[0]);
		exit(1);
	}
	printf("Connecting to device %s...\n", argv[1]);
	gatt_connection_t *connection = gattlib_connect(NULL, argv[1], GATTLIB_CONNECTION_OPTIONS_LEGACY_BDADDR_LE_PUBLIC | GATTLIB_CONNECTION_OPTIONS_LEGACY_BT_SEC_LOW);
	if (!connection) {
		printf("ERROR: Could not connect.\n");
		exit(1);
	}
	printf("Done.\n");

	printf("Writing name %.10s...\n", argv[2]);

	uint8_t name[10];
	memcpy(name, argv[2], 10);

	uuid_t name_uuid = string_to_uuid(THINGY_NAME_UUID);
	gattlib_write_char_by_uuid(connection, &name_uuid, name, sizeof(name));
	printf("Done.\n");

	printf("Disconnecting...\n");
	gattlib_disconnect(connection);
	printf("Done.\n");
	return 0;
}