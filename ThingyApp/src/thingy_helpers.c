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

#include "thingyAggregator.h"
#include "thingy_helpers.h"

static void print_resp(uint8_t*, size_t);

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
uuid_t aggregatorUUID(const char *str) {
	uint128_t uuid_val = str_to_128t(str);
	uuid_t uuid = {.type=SDP_UUID128, .value.uuid128=uuid_val};
	return uuid;
}

static void parse_button_report(uint8_t *resp, size_t len) {
	int nodeID = resp[RESP_ID];
	aSubRecord *buttonPV = get_pv(nodeID, BUTTON_ID);

	if (buttonPV != 0)
		set_pv(buttonPV, resp[RESP_REPORT_BUTTON_STATE]);
}

static void parse_humidity(uint8_t *resp, size_t len) {
	int nodeID = resp[RESP_ID];
	aSubRecord *humidPV = get_pv(nodeID, HUMIDITY_ID);

	if (humidPV != 0)
		set_pv(humidPV, resp[RESP_HUMIDITY_VAL]);
}

static void parse_rssi(uint8_t *resp, size_t len) {
	int nodeID = resp[RESP_ID];
	aSubRecord *rssiPV = get_pv(nodeID, RSSI_ID);
	int8_t rssi = resp[RESP_RSSI_VAL];

	if (rssiPV != 0)
		set_pv(rssiPV, rssi);
}

// Parse response
void parse_resp(uint8_t *resp, size_t len) {
	uint8_t op = resp[RESP_OPCODE];
	if (op == OPCODE_BUTTON_REPORT)
		parse_button_report(resp, len);
	else if (op == OPCODE_HUMIDITY)
		parse_humidity(resp, len);
	else if (op == OPCODE_RSSI)
		parse_rssi(resp, len);
}

// set PV value and scan it
int set_pv(aSubRecord *pv, float val) {
	if (pv == 0)
		return 1;
	memcpy(pv->vala, &val, sizeof(float));
	if (ioc_started) {
		scanOnce(pv);
	}
	return 0;
}

// set status PV 
int set_status(int nodeID, char* status) {
	//printf("status = %s for node %d\n", status, nodeID);
	aSubRecord *pv = get_pv(nodeID, STATUS_ID);
	if (pv == 0)
		return 1;
	strncpy(pv->vala, status, 40);
	if (ioc_started) {
		scanOnce(pv);
	}
	return 0;
}

// set connection PV
int set_connection(int nodeID, float status) {
	aSubRecord *pv = get_pv(nodeID, CONNECTION_ID);
	if (pv == 0)
		return 1;
	//printf("set connection %d for node %d\n", status, nodeID);
	set_pv(pv, status);
	return 0;
}

// print response
static void print_resp(uint8_t* resp, size_t len) {
	printf("resp: ");
	for (int i=0; i<len; i++)
		printf("%d ", resp[i]);
	printf("\n");
}
