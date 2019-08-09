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

/*
 *	 helper functions to parse response from node according to sensor ID and save to corresponding PVs
 */

static void parse_button(uint8_t *resp, size_t len) {
	int nodeID = resp[RESP_ID];
	aSubRecord *buttonPV = get_pv(nodeID, BUTTON_ID);

	if (buttonPV != 0)
		set_pv(buttonPV, resp[RESP_BUTTON_STATE]);
}

static void parse_battery(uint8_t *resp, size_t len) {
	int nodeID = resp[RESP_ID];
	aSubRecord *batteryPV = get_pv(nodeID, BATTERY_ID);

	if (batteryPV != 0)
		set_pv(batteryPV, resp[RESP_BATTERY_LEVEL]);
}

static void parse_rssi(uint8_t *resp, size_t len) {
	int nodeID = resp[RESP_ID];
	aSubRecord *rssiPV = get_pv(nodeID, RSSI_ID);
	int8_t rssi = resp[RESP_RSSI_VAL];

	if (rssiPV != 0)
		set_pv(rssiPV, rssi);
}

static void parse_temperature(uint8_t *resp, size_t len) {
	int nodeID = resp[RESP_ID];
	aSubRecord *tempPV = get_pv(nodeID, TEMPERATURE_ID);

	if (tempPV != 0) {
		int8_t integer = resp[RESP_TEMPERATURE_INT];
		uint8_t decimal = resp[RESP_TEMPERATURE_DEC];
		float temperature = integer + (float)(decimal / 100.0);
		set_pv(tempPV, temperature);
	}
}

static void parse_pressure(uint8_t *resp, size_t len) {
	int nodeID = resp[RESP_ID];
	aSubRecord *pressurePV = get_pv(nodeID, PRESSURE_ID);

	if (pressurePV != 0) {
		int i = RESP_PRESSURE_INT;
		int32_t integer = (resp[i]) | (resp[i+1] << 8) | (resp[i+2] << 16) | (resp[i+3] << 24);
		uint8_t decimal = resp[RESP_PRESSURE_DEC];
		float pressure = integer + (float)(decimal / 100.0);
		set_pv(pressurePV, pressure);
	}
}

static void parse_humidity(uint8_t *resp, size_t len) {
	int nodeID = resp[RESP_ID];
	aSubRecord *humidPV = get_pv(nodeID, HUMIDITY_ID);

	if (humidPV != 0)
		set_pv(humidPV, resp[RESP_HUMIDITY_VAL]);
}

static void parse_gas(uint8_t *resp, size_t len) {
	int nodeID = resp[RESP_ID];
	aSubRecord *gasPV = get_pv(nodeID, GAS_ID);
	aSubRecord *coPV = get_pv(nodeID, CO2_ID);
	aSubRecord *tvocPV = get_pv(nodeID, TVOC_ID);

	if (gasPV != 0 && ioc_started) {
		int i = RESP_GAS_CO2;
		uint16_t co2 = (resp[i]) | (resp[i+1] << 8);
		if (coPV != 0)
			set_pv(coPV, co2);
		i = RESP_GAS_TVOC;
		uint16_t tvoc = (resp[i]) | (resp[i+1] << 8);
		if (tvocPV != 0)
			set_pv(tvocPV, tvoc);
		char buf[40];
		memset(buf, 0, sizeof(buf));
		snprintf(buf, sizeof(buf), "%u eCO2 ppm\n%u TVOC ppb", (unsigned int)co2, (unsigned int)tvoc);
		strncpy(gasPV->vala, buf, sizeof(buf));
		scanOnce(gasPV);
	}
}

static void parse_quaternions(uint8_t *resp, size_t len) {
	int nodeID = resp[RESP_ID];
	aSubRecord *pvs[4];
	pvs[0] = get_pv(nodeID, QUATERNION_W_ID);
	pvs[1] = get_pv(nodeID, QUATERNION_X_ID);
	pvs[2] = get_pv(nodeID, QUATERNION_Y_ID);
	pvs[3] = get_pv(nodeID, QUATERNION_Z_ID);

	for (int i=0; i<4; i++) {
		if (pvs[i] != 0) {
			int j = RESP_QUATERNIONS_W + (i * 4);
			int32_t raw = (resp[j]) | (resp[j+1] << 8) | (resp[j+2] << 16) | (resp[j+3] << 24);
			float x = ((float)(raw) / (float)(1 << 30)); // 2Q30 fixed point
			set_pv(pvs[i], x);
		}
	}
}

static void parse_raw_motion(uint8_t *resp, size_t len) {
	int nodeID = resp[RESP_ID];
	aSubRecord *pvs[9];
	pvs[0] = get_pv(nodeID, ACCEL_X_ID); pvs[1] = get_pv(nodeID, ACCEL_Y_ID); pvs[2] = get_pv(nodeID, ACCEL_Z_ID);
	pvs[3] = get_pv(nodeID, GYRO_X_ID); pvs[4] = get_pv(nodeID, GYRO_Y_ID); pvs[5] = get_pv(nodeID, GYRO_Z_ID);
	pvs[6] = get_pv(nodeID, COMPASS_X_ID); pvs[7] = get_pv(nodeID, COMPASS_Y_ID); pvs[8] = get_pv(nodeID, COMPASS_Z_ID);
	for (int i=0; i<9; i++) {
		if (pvs[i] != 0) {
			int j = RESP_RAW_ACCEL_X + (i * 2);
			int16_t raw = (resp[j]) | (resp[j+1] << 8);
			float x;
			if (i <= 2) // acceleration
				x = ((float)(raw) / (float)(1 << 10)); // 6Q10 fixed point
			else if (i <= 5) // gyroscope
				x = ((float)(raw) / (float)(1 << 5)); // 11Q5 fixed point
			else // compass
				x = ((float)(raw) / (float)(1 << 4)); // 12Q4 fixed point
			set_pv(pvs[i], x);
		}
	}
}

static void parse_euler(uint8_t *resp, size_t len) {
	int nodeID = resp[RESP_ID];
	aSubRecord *pvs[3];
	pvs[0] = get_pv(nodeID, ROLL_ID);
	pvs[1] = get_pv(nodeID, PITCH_ID);
	pvs[2] = get_pv(nodeID, YAW_ID);
	for (int i=0; i<3; i++) {
		if (pvs[i] != 0) {
			int j = RESP_EULER_ROLL + (i * 4);
			int32_t raw = (resp[j]) | (resp[j+1] << 8) | (resp[j+2] << 16) | (resp[j+3] << 24);
			float x = ((float)(raw) / (float)(1 << 16)); // 16Q16 fixed point
			set_pv(pvs[i], x);
		}
	}
}

static void parse_heading(uint8_t *resp, size_t len) {
	int nodeID = resp[RESP_ID];
	aSubRecord *headingPV = get_pv(nodeID, HEADING_ID);
	if (headingPV != 0) {
		int i = RESP_HEADING_VAL;
		int32_t raw = (resp[i]) | (resp[i+2] << 8) | (resp[i+3] << 16) | (resp[i+4] << 24);
		float x = ((float)(raw) / (float)(1 << 16)); // 16Q16 fixed point
		set_pv(headingPV, x);
	}
}

// Parse response
void parse_resp(uint8_t *resp, size_t len) {
	//print_resp(resp, len);
	uint8_t op = resp[RESP_OPCODE];
	if (op == OPCODE_BUTTON)
		parse_button(resp, len);
	else if (op == OPCODE_BATTERY)
		parse_battery(resp, len);
	else if (op == OPCODE_RSSI)
		parse_rssi(resp, len);
	else if (op == OPCODE_TEMPERATURE)
		parse_temperature(resp, len);
	else if (op == OPCODE_PRESSURE)
		parse_pressure(resp, len);
	else if (op == OPCODE_HUMIDITY)
		parse_humidity(resp, len);
	else if (op == OPCODE_GAS)
		parse_gas(resp, len);
	else if (op == OPCODE_QUATERNIONS)
		parse_quaternions(resp, len);
	else if (op == OPCODE_RAW_MOTION)
		parse_raw_motion(resp, len);
	else if (op == OPCODE_EULER)
		parse_euler(resp, len);
	else if (op == OPCODE_HEADING)
		parse_heading(resp, len);
	//else
	//	printf("unknown opcode: %d\n", op);
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
