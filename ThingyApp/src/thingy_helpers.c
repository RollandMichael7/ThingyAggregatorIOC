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

#include "thingy_shared.h"
#include "thingy_aggregator.h"
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
uuid_t aggregator_UUID(const char *str) {
	uint128_t uuid_val = str_to_128t(str);
	uuid_t uuid = {.type=SDP_UUID128, .value.uuid128=uuid_val};
	return uuid;
}

// get value from read/write PVs 
// these PVs store value in INPC field
static float get_writer_pv_value(int node_id, int pv_id) {
	aSubRecord *pv = get_pv(node_id, pv_id);
	if (pv != 0) {
		scanOnce(pv);
		// wait for scan to complete
		usleep(500);
		float c;
		memcpy(&c, pv->c, sizeof(float));
		//printf("%.2f\n", c);
		return c;
	}
	else
		return -1;
}

// toggle digital pin for node
// toggled_pins = logical OR of pins to toggle
void toggle_io_helper(int node_id, int toggled_pins) {
	#ifdef USE_CUSTOM_IDS
		node_id = get_actual_node_id(node_id);
	#endif
	uint8_t command[6];
	command[0] = COMMAND_IO_WRITE;
	command[1] = node_id;

	aSubRecord *pv;
	int val;
	int bit;
	for (int i=0; i < 4; i++) {
		val = get_writer_pv_value(node_id, ID_EXT0 + i);
		if (val == -1)
			return;
		bit = 1 << i;
		//printf("%d\n", val);
		if (bit & toggled_pins)
			command[2 + i] = (val == 0) ? 255 : 0;
		else
			command[2 + i] = (val == 0) ? 0 : 255;
	}
	gattlib_write_char_by_uuid(gp_connection, &g_send_uuid, command, sizeof(command));
	// wait for write to complete
	usleep(500);
	// read pins to confirm write
	command[0] = COMMAND_IO_READ;
	gattlib_write_char_by_uuid(gp_connection, &g_send_uuid, command, sizeof(command));
}

// write environment config values to node
void write_env_config_helper(int node_id) {
	#ifdef USE_CUSTOM_IDS
		node_id = get_actual_node_id(node_id);
	#endif
	uint16_t tempInterval = get_writer_pv_value(node_id, ID_TEMP_INTERVAL);
	uint16_t pressureInterval = get_writer_pv_value(node_id, ID_PRESSURE_INTERVAL);
	uint16_t humidInterval = get_writer_pv_value(node_id, ID_HUMID_INTERVAL);
	uint16_t colorInterval = 60000;
	uint8_t gasMode = get_writer_pv_value(node_id, ID_GAS_MODE);
	//printf("write env config: %d %d %d %d\n", tempInterval, pressureInterval, humidInterval, gasMode);
	uint8_t command[14];
	command[0] = COMMAND_ENV_CONFIG_WRITE;
	command[1] = node_id;
	command[2] = tempInterval & 0xFF;
	command[3] = tempInterval >> 8;
	command[4] = pressureInterval & 0xFF;
	command[5] = pressureInterval >> 8;
	command[6] = humidInterval & 0xFF;
	command[7] = humidInterval >> 8;
	command[8] = colorInterval & 0xFF;
	command[9] = colorInterval >> 8;
	command[10] = gasMode;
	// light sensor LED RGB color; currently unused
	command[11] = 0;
	command[12] = 0;
	command[13] = 0;
	gattlib_write_char_by_uuid(gp_connection, &g_send_uuid, command, sizeof(command));
	// wait for write to complete
	usleep(500);
	// read values again to confirm write
	command[0] = COMMAND_ENV_CONFIG_READ;
	gattlib_write_char_by_uuid(gp_connection, &g_send_uuid, command, sizeof(command));
}

// write motion config values to node
void write_motion_config_helper(int node_id) {
	#ifdef USE_CUSTOM_IDS
		node_id = get_actual_node_id(node_id);
	#endif
	uint16_t steps = get_writer_pv_value(node_id, ID_STEP_INTERVAL);
	uint16_t tempComp = get_writer_pv_value(node_id, ID_TEMP_COMP_INTERVAL);
	uint16_t magComp = get_writer_pv_value(node_id, ID_MAG_COMP_INTERVAL);
	uint16_t freq = get_writer_pv_value(node_id, ID_MOTION_FREQ);
	uint8_t wake = get_writer_pv_value(node_id, ID_WAKE);
	//printf("write motion config: %d %d %d %d %d\n", steps, tempComp, magComp, freq, wake);
	uint8_t command[11];
	command[0] = COMMAND_MOTION_CONFIG_WRITE;
	command[1] = node_id;
	command[2] = steps & 0xFF;
	command[3] = steps >> 8;
	command[4] = tempComp & 0xFF;
	command[5] = tempComp >> 8;
	command[6] = magComp & 0xFF;
	command[7] = magComp >> 8;
	command[8] = freq & 0xFF;
	command[9] = freq >> 8;
	command[10] = wake;
	gattlib_write_char_by_uuid(gp_connection, &g_send_uuid, command, sizeof(command));
	// wait for write to complete
	usleep(500);
	// read values again to confirm write
	command[0] = COMMAND_MOTION_CONFIG_READ;
	gattlib_write_char_by_uuid(gp_connection, &g_send_uuid, command, sizeof(command));
}

// write conn param values to node
void write_conn_param_helper(int node_id) {
	#ifdef USE_CUSTOM_IDS
		node_id = get_actual_node_id(node_id);
	#endif
	uint16_t min = (uint16_t) (get_writer_pv_value(node_id, ID_CONN_MIN_INTERVAL) / 1.25);
	uint16_t max = (uint16_t) (get_writer_pv_value(node_id, ID_CONN_MAX_INTERVAL) / 1.25);
	uint16_t latency = get_writer_pv_value(node_id, ID_CONN_LATENCY);
	uint16_t timeout = (uint16_t) (get_writer_pv_value(node_id, ID_CONN_TIMEOUT) / 10);
	uint8_t command[10];
	command[0] = COMMAND_CONN_PARAM_WRITE;
	command[1] = node_id;
	command[2] = min & 0xFF;
	command[3] = min >> 8;
	command[4] = max & 0xFF;
	command[5] = max >> 8;
	command[6] = latency & 0xFF;
	command[7] = latency >> 8;
	command[8] = timeout & 0xFF;
	command[9] = timeout >> 8;
	gattlib_write_char_by_uuid(gp_connection, &g_send_uuid, command, sizeof(command));
	usleep(500);
	command[0] = COMMAND_CONN_PARAM_READ;
	gattlib_write_char_by_uuid(gp_connection, &g_send_uuid, command, sizeof(command));
}	

/*
 *	helper functions to parse response from node according to opcode and save to corresponding PVs
 */

static void parse_connect(uint8_t *resp, size_t len) {
	int curr_id = resp[RESP_ID];
	int valid = 1;

	#ifdef USE_CUSTOM_IDS
		// get Bluetooth name of node
		char name[MAX_NAME_LENGTH];
		memset(name, 0, MAX_NAME_LENGTH);
		int name_length = 0;
		for (name_length; name_length < MAX_NAME_LENGTH; name_length++)
			if (resp[RESP_CONNECT_NAME + name_length] == 32)
				break;
		memcpy(name, &(resp[RESP_CONNECT_NAME]), name_length);

		// check if name matches custom node name
		if (strncmp(name, CUSTOM_NODE_NAME, strlen(CUSTOM_NODE_NAME)) == 0) {
			char custom_id_buf[5];
			memset(custom_id_buf, 0, sizeof(custom_id_buf));
			memcpy(custom_id_buf, &(name[strlen(CUSTOM_NODE_NAME)]), name_length - strlen(CUSTOM_NODE_NAME));
			int custom_id = strtol(custom_id_buf, NULL, 10);
			if (g_custom_node_ids[curr_id] == -1) {
				printf("Assigned custom node ID %d to device %s (actual ID %d)\n", custom_id, name, curr_id);
				g_custom_node_ids[curr_id] = custom_id;
			}
			else {
				printf("WARNING: Can not assign node ID %d to device %s: Already in use\n", custom_id, name);
				valid = 0;
			}
		}
		else {
			if (g_custom_node_ids[curr_id] == -1) {
				printf("Assigned node ID %d to device %s\n", curr_id, name);
				g_custom_node_ids[curr_id] = curr_id;
			}
			else {
				printf("WARNING: Can not assign node ID %d to device %s: Already in use\n", curr_id, name);
				valid = 0;
			}
		}
	#endif
	if (valid) {
		set_connection(curr_id, CONNECTED);
		#ifndef USE_CUSTOM_IDS
			printf("Connected node %d\n", curr_id);
		#endif
	}
}

static void parse_disconnect(uint8_t *resp, size_t len) {
	int node_id = resp[RESP_ID];
	printf("Node %d disconnected\n", node_id);
	disconnect_node(node_id);
	#ifdef USE_CUSTOM_IDS
		g_custom_node_ids[node_id] = -1;
	#endif
}

static void parse_button(uint8_t *resp, size_t len) {
	int node_id = resp[RESP_ID];
	aSubRecord *button_pv = get_pv(node_id, ID_BUTTON);

	if (button_pv != 0)
		set_pv(button_pv, resp[RESP_BUTTON_STATE]);
}

static void parse_battery(uint8_t *resp, size_t len) {
	int node_id = resp[RESP_ID];
	aSubRecord *battery_pv = get_pv(node_id, ID_BATTERY);

	if (battery_pv != 0)
		set_pv(battery_pv, resp[RESP_BATTERY_LEVEL]);
}

static void parse_rssi(uint8_t *resp, size_t len) {
	int node_id = resp[RESP_ID];
	aSubRecord *rssi_pv = get_pv(node_id, ID_RSSI);
	int8_t rssi = resp[RESP_RSSI_VAL];

	if (rssi_pv != 0)
		set_pv(rssi_pv, rssi);
}

static void parse_temperature(uint8_t *resp, size_t len) {
	int node_id = resp[RESP_ID];
	aSubRecord *temp_pv = get_pv(node_id, ID_TEMPERATURE);

	if (temp_pv != 0) {
		int8_t integer = resp[RESP_TEMPERATURE_INT];
		uint8_t decimal = resp[RESP_TEMPERATURE_DEC];
		float temperature = integer + (float)(decimal / 100.0);
		set_pv(temp_pv, temperature);
	}
}

static void parse_pressure(uint8_t *resp, size_t len) {
	int node_id = resp[RESP_ID];
	aSubRecord *pressure_pv = get_pv(node_id, ID_PRESSURE);

	if (pressure_pv != 0) {
		int i = RESP_PRESSURE_INT;
		int32_t integer = (resp[i]) | (resp[i+1] << 8) | (resp[i+2] << 16) | (resp[i+3] << 24);
		uint8_t decimal = resp[RESP_PRESSURE_DEC];
		float pressure = integer + (float)(decimal / 100.0);
		set_pv(pressure_pv, pressure);
	}
}

static void parse_humidity(uint8_t *resp, size_t len) {
	int node_id = resp[RESP_ID];
	aSubRecord *humid_pv = get_pv(node_id, ID_HUMIDITY);

	if (humid_pv != 0)
		set_pv(humid_pv, resp[RESP_HUMIDITY_VAL]);
}

static void parse_gas(uint8_t *resp, size_t len) {
	int node_id = resp[RESP_ID];
	aSubRecord *gas_pv = get_pv(node_id, ID_GAS);
	aSubRecord *co_pv = get_pv(node_id, ID_CO2);
	aSubRecord *tvoc_pv = get_pv(node_id, ID_TVOC);

	if (gas_pv != 0 && g_ioc_started) {
		int i = RESP_GAS_CO2;
		uint16_t co2 = (resp[i]) | (resp[i+1] << 8);
		if (co_pv != 0)
			set_pv(co_pv, co2);
		i = RESP_GAS_TVOC;
		uint16_t tvoc = (resp[i]) | (resp[i+1] << 8);
		if (tvoc_pv != 0)
			set_pv(tvoc_pv, tvoc);
		char buf[40];
		memset(buf, 0, sizeof(buf));
		snprintf(buf, sizeof(buf), "%u eCO2 ppm\n%u TVOC ppb", (unsigned int)co2, (unsigned int)tvoc);
		strncpy(gas_pv->vala, buf, sizeof(buf));
		scanOnce(gas_pv);
	}
}

static void parse_env_config(uint8_t *resp, size_t len) {
	//print_resp(resp, len);
	int node_id = resp[RESP_ID];
	aSubRecord *temp_interval_pv = get_pv(node_id, ID_TEMP_INTERVAL);
	aSubRecord *pressure_interval_pv = get_pv(node_id, ID_PRESSURE_INTERVAL);
	aSubRecord *humid_interval_pv = get_pv(node_id, ID_HUMID_INTERVAL);
	aSubRecord *gas_mode_pv = get_pv(node_id, ID_GAS_MODE);
	uint16_t interval;
	if (temp_interval_pv != 0) {
		interval = (resp[3]) | (resp[4] << 8);
		set_pv(temp_interval_pv, interval);
		set_pv(temp_interval_pv, interval);
	}
	if (pressure_interval_pv != 0) {
		interval = (resp[5]) | (resp[6] << 8);
		set_pv(pressure_interval_pv, interval);
		set_pv(pressure_interval_pv, interval);
	}
	if (humid_interval_pv != 0) {
		interval = (resp[7]) | (resp[8] << 8);
		set_pv(humid_interval_pv, interval);
		set_pv(humid_interval_pv, interval);
	}
	if (gas_mode_pv != 0) {
		set_pv(gas_mode_pv, resp[11]);
		set_pv(gas_mode_pv, resp[11]);
	}
}

static void parse_quaternions(uint8_t *resp, size_t len) {
	int node_id = resp[RESP_ID];
	aSubRecord *pvs[4];
	pvs[0] = get_pv(node_id, ID_QUATERNION_W);
	pvs[1] = get_pv(node_id, ID_QUATERNION_X);
	pvs[2] = get_pv(node_id, ID_QUATERNION_Y);
	pvs[3] = get_pv(node_id, ID_QUATERNION_Z);

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
	int node_id = resp[RESP_ID];
	aSubRecord *pvs[9];
	pvs[0] = get_pv(node_id, ID_ACCEL_X); pvs[1] = get_pv(node_id, ID_ACCEL_Y); pvs[2] = get_pv(node_id, ID_ACCEL_Z);
	pvs[3] = get_pv(node_id, ID_GYRO_X); pvs[4] = get_pv(node_id, ID_GYRO_Y); pvs[5] = get_pv(node_id, ID_GYRO_Z);
	pvs[6] = get_pv(node_id, ID_COMPASS_X); pvs[7] = get_pv(node_id, ID_COMPASS_Y); pvs[8] = get_pv(node_id, ID_COMPASS_Z);
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
	int node_id = resp[RESP_ID];
	aSubRecord *pvs[3];
	pvs[0] = get_pv(node_id, ID_ROLL);
	pvs[1] = get_pv(node_id, ID_PITCH);
	pvs[2] = get_pv(node_id, ID_YAW);
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
	int node_id = resp[RESP_ID];
	aSubRecord *heading_pv = get_pv(node_id, ID_HEADING);
	if (heading_pv != 0) {
		int i = RESP_HEADING_VAL;
		int32_t raw = (resp[i]) | (resp[i+1] << 8) | (resp[i+2] << 16) | (resp[i+3] << 24);
		float x = ((float)(raw) / (float)(1 << 16)); // 16Q16 fixed point
		set_pv(heading_pv, x);
	}
}

static void parse_motion_config(uint8_t *resp, size_t len) {
	//print_resp(resp, len);
	int node_id = resp[RESP_ID];
	aSubRecord *step_interval_pv = get_pv(node_id, ID_STEP_INTERVAL);
	aSubRecord *temp_comp_pv = get_pv(node_id, ID_TEMP_COMP_INTERVAL);
	aSubRecord *mag_comp_pv = get_pv(node_id, ID_MAG_COMP_INTERVAL);
	aSubRecord *frequency_pv = get_pv(node_id, ID_MOTION_FREQ);
	aSubRecord *wake_pv = get_pv(node_id, ID_WAKE);
	uint16_t val;
	if (step_interval_pv != 0) {
		val = (resp[3]) | (resp[4] << 8);
		set_pv(step_interval_pv, val);
	}
	if (temp_comp_pv != 0) {
		val = (resp[5]) | (resp[6] << 8);
		set_pv(temp_comp_pv, val);
	}
	if (mag_comp_pv != 0) {
		val = (resp[7]) | (resp[8] << 8);
		set_pv(mag_comp_pv, val);
	}
	if (frequency_pv != 0) {
		val = (resp[9]) | (resp[10] << 8);
		set_pv(frequency_pv, val);
	}
	if (wake_pv != 0)
		set_pv(wake_pv, resp[11]);
}

static void parse_conn_param(uint8_t *resp, size_t len) {
	//print_resp(resp, len);
	int node_id = resp[RESP_ID];
	aSubRecord *min_interval_pv = get_pv(node_id, ID_CONN_MIN_INTERVAL);
	aSubRecord *max_interval_pv = get_pv(node_id, ID_CONN_MAX_INTERVAL);
	aSubRecord *latency_pv = get_pv(node_id, ID_CONN_LATENCY);
	aSubRecord *timeout_pv = get_pv(node_id, ID_CONN_TIMEOUT);
	float x;
	if (min_interval_pv != 0) {
		x = (resp[3]) | (resp[4] << 8);
		x *= 1.25;
		set_pv(min_interval_pv, x);
	}
	if (max_interval_pv != 0) {
		x = (resp[5]) | (resp[6] << 8);
		x *= 1.25;
		set_pv(max_interval_pv, x);
	}
	if (latency_pv != 0) {
		x = (resp[7]) | (resp[8] << 8);
		set_pv(latency_pv, x);
	}
	if (timeout_pv != 0) {
		x = (resp[9]) | (resp[10] << 8);
		x *= 10;
		set_pv(timeout_pv, x);
	}
}

static void parse_io(uint8_t *resp, size_t len) {
	//print_resp(resp, len);
	int node_id = resp[RESP_ID];
	aSubRecord *ext0 = get_pv(node_id, ID_EXT0);
	aSubRecord *ext1 = get_pv(node_id, ID_EXT1);
	aSubRecord *ext2 = get_pv(node_id, ID_EXT2);
	aSubRecord *ext3 = get_pv(node_id, ID_EXT3);
	float x;
	if (ext0 != 0) {
		set_pv(ext0, resp[3]);
	}
	if (ext1 != 0) {
		set_pv(ext1, resp[4]);
	}
	if (ext2 != 0) {
		set_pv(ext2, resp[5]);
	}
	if (ext3 != 0) {
		set_pv(ext3, resp[6]);
	}
}

// Parse response
void parse_resp(uint8_t *resp, size_t len) {
	//print_resp(resp, len);
	uint8_t op = resp[RESP_OPCODE];
	if (op == OPCODE_CONNECT)
		parse_connect(resp, len);
	else if (op == OPCODE_DISCONNECT)
		parse_disconnect(resp, len);
	else if (op == OPCODE_BUTTON)
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
	else if (op == OPCODE_ENV_CONFIG)
		parse_env_config(resp, len);
	else if (op == OPCODE_QUATERNIONS)
		parse_quaternions(resp, len);
	else if (op == OPCODE_RAW_MOTION)
		parse_raw_motion(resp, len);
	else if (op == OPCODE_EULER)
		parse_euler(resp, len);
	else if (op == OPCODE_HEADING)
		parse_heading(resp, len);
	else if (op == OPCODE_MOTION_CONFIG)
		parse_motion_config(resp, len);
	else if (op == OPCODE_CONN_PARAM)
		parse_conn_param(resp, len);
	else if (op == OPCODE_EXTIO)
		parse_io(resp, len);
	else {
		printf("unknown opcode: %d\n", op);
		print_resp(resp, len);
	}
}

// print response
static void print_resp(uint8_t* resp, size_t len) {
	printf("resp: ");
	for (int i=0; i<len; i++)
		printf("%d ", resp[i]);
	printf("\n");
}

// set status PV 
int set_status(int node_id, char* status) {
	//printf("status = %s for node %d\n", status, node_id);
	aSubRecord *pv = get_pv(node_id, ID_STATUS);
	if (pv == 0)
		return 1;
	strncpy(pv->vala, status, 40);
	if (g_ioc_started) {
		scanOnce(pv);
	}
	return 0;
}

// set gp_connection PV
int set_connection(int node_id, int status) {
	aSubRecord *pv = get_pv(node_id, ID_CONNECTION);
	if (pv == 0)
		return 1;
	//printf("set connection %d for node %d\n", status, node_id);
	set_pv(pv, status);
	return 0;
}

// Check if a read command PV was triggered, and send command if so
long poll_command_pv(aSubRecord *pv, int opcode) {
	int val;
	memcpy(&val, pv->b, sizeof(int));
	if (val != 0) {
		int node_id;
		memcpy(&node_id, pv->a, sizeof(int));
		#ifdef USE_CUSTOM_IDS
			node_id = get_actual_node_id(node_id);
		#endif

		uint8_t command[2];
		command[0] = opcode;
		command[1] = node_id;
		gattlib_write_char_by_uuid(gp_connection, &g_send_uuid, command, sizeof(command));
		set_pv(pv, 0);
	}
	return 0;
}

// send a read command (which has only 1 argument) to aggregator
void send_read_command(int opcode, int node_id) {
	#ifdef USE_CUSTOM_IDS
		node_id = get_actual_node_id(node_id);
	#endif

	uint8_t command[2];
	command[0] = opcode;
	command[1] = node_id;
	gattlib_write_char_by_uuid(gp_connection, &g_send_uuid, command, sizeof(command));
}

// fetch PV from linked list given node/PV IDs
aSubRecord* get_pv(int node_id, int pv_id) {
	#ifdef USE_CUSTOM_IDS
		if (g_ioc_started)
			node_id = g_custom_node_ids[node_id];
	#endif

	PVnode *node = g_first_pv;
	while (node != 0) {
		if (node->node_id == node_id && node->pv_id == pv_id) {
			return node->pv;
		}
		node = node->next;
	}
	printf("WARNING: No PV for node %d sensor %d\n", node_id, pv_id);
	return 0;
}

// set PV value and scan it
int set_pv(aSubRecord *pv, float val) {
	if (pv == 0)
		return 1;
	memcpy(pv->vala, &val, sizeof(float));
	if (g_ioc_started) {
		scanOnce(pv);
	}
	return 0;
}

// mark dead nodes through PV values
static void nullify_node_pvs(int node_id) {
	#ifdef USE_CUSTOM_IDS
		if (g_custom_node_ids[node_id] != -1)
			node_id = g_custom_node_ids[node_id];
	#endif

	float null = 0;
	int pv_id;
	PVnode *node = g_first_pv;
	aSubRecord *pv;
	while (node != 0) {
		pv_id = node->pv_id;
		if (node->node_id == node_id && pv_id != ID_CONNECTION && pv_id != ID_STATUS) {
			if (pv_id == ID_BUTTON)
				set_pv(node->pv, 0);
			else
				set_pv(node->pv, null);
		}
		node = node->next;
	}
}

void disconnect_node(int node_id) {
	nullify_node_pvs(node_id);
	set_status(node_id, "DISCONNECTED");
	set_connection(node_id, DISCONNECTED);
	g_dead[node_id] = 1;
}

#ifdef USE_CUSTOM_IDS
	int get_actual_node_id(int node_id) {
		for (int i=0; i<MAX_NODES; i++)
			if (g_custom_node_ids[i] == node_id) {
				//printf("custom id %d -> actual id %d\n", node_id, i);
				return i;
			}
		return -1;
	}
#endif
