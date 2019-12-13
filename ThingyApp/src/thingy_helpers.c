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

#include "thingy_aggregator.h"
#include "thingy_helpers.h"
#include "thingy_shared.h"

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
static float get_writer_pv_value(int node_id, int pvID) {
	aSubRecord *pv = get_pv(node_id, pvID);
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
		return 0;
}

// write environment config values to node
void write_env_config_helper(int node_id) {
	uint16_t tempInterval = get_writer_pv_value(node_id, TEMP_INTERVAL_ID);
	uint16_t pressureInterval = get_writer_pv_value(node_id, PRESSURE_INTERVAL_ID);
	uint16_t humidInterval = get_writer_pv_value(node_id, HUMID_INTERVAL_ID);
	uint16_t colorInterval = 60000;
	uint8_t gasMode = get_writer_pv_value(node_id, GAS_MODE_ID);
	//printf("write env config: %d %d %d %d\n", tempInterval, pressureInterval, humidInterval, gasMode);
	uint8_t command[14];
	command[0] = COMMAND_ENV_CONFIG_WRITE;
	command[1] = get_actual_node_id(node_id);
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
	uint16_t steps = get_writer_pv_value(node_id, STEP_INTERVAL_ID);
	uint16_t tempComp = get_writer_pv_value(node_id, TEMP_COMP_INTERVAL_ID);
	uint16_t magComp = get_writer_pv_value(node_id, MAG_COMP_INTERVAL_ID);
	uint16_t freq = get_writer_pv_value(node_id, MOTION_FREQ_ID);
	uint8_t wake = get_writer_pv_value(node_id, WAKE_ID);
	//printf("write motion config: %d %d %d %d %d\n", steps, tempComp, magComp, freq, wake);
	uint8_t command[11];
	command[0] = COMMAND_MOTION_CONFIG_WRITE;
	command[1] = get_actual_node_id(node_id);
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
	uint16_t min = (uint16_t) (get_writer_pv_value(node_id, CONN_MIN_INTERVAL_ID) / 1.25);
	uint16_t max = (uint16_t) (get_writer_pv_value(node_id, CONN_MAX_INTERVAL_ID) / 1.25);
	uint16_t latency = get_writer_pv_value(node_id, CONN_LATENCY_ID);
	uint16_t timeout = (uint16_t) (get_writer_pv_value(node_id, CONN_TIMEOUT_ID) / 10);
	uint8_t command[10];
	command[0] = COMMAND_CONN_PARAM_WRITE;
	command[1] = get_actual_node_id(node_id);
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
			printf("Assigned custom node ID %d to device %s\n", custom_id, name);
			g_custom_node_ids[curr_id] = custom_id;
		}
		else
			printf("WARNING: Can not assign node ID %d to device %s: Already in use\n", custom_id, name);
	}
	else {
		if (g_custom_node_ids[curr_id] == -1) {
			printf("Assigned node ID %d to device %s\n", curr_id, name);
			g_custom_node_ids[curr_id] = curr_id;
		}
		else
			printf("WARNING: Can not assign node ID %d to device %s: Already in use\n", curr_id, name);
	}
	set_connection(curr_id, CONNECTED);
}

static void parse_disconnect(uint8_t *resp, size_t len) {
	int node_id = resp[RESP_ID];
	printf("disconnecting node %d\n", node_id);
	disconnect_node(node_id);
	g_custom_node_ids[node_id] = -1;
}

static void parse_button(uint8_t *resp, size_t len) {
	int node_id = resp[RESP_ID];
	aSubRecord *button_pv = get_pv(node_id, BUTTON_ID);

	if (button_pv != 0)
		set_pv(button_pv, resp[RESP_BUTTON_STATE]);
}

static void parse_battery(uint8_t *resp, size_t len) {
	int node_id = resp[RESP_ID];
	aSubRecord *battery_pv = get_pv(node_id, BATTERY_ID);

	if (battery_pv != 0)
		set_pv(battery_pv, resp[RESP_BATTERY_LEVEL]);
}

static void parse_rssi(uint8_t *resp, size_t len) {
	int node_id = resp[RESP_ID];
	aSubRecord *rssi_pv = get_pv(node_id, RSSI_ID);
	int8_t rssi = resp[RESP_RSSI_VAL];

	if (rssi_pv != 0)
		set_pv(rssi_pv, rssi);
}

static void parse_temperature(uint8_t *resp, size_t len) {
	int node_id = resp[RESP_ID];
	aSubRecord *temp_pv = get_pv(node_id, TEMPERATURE_ID);

	if (temp_pv != 0) {
		int8_t integer = resp[RESP_TEMPERATURE_INT];
		uint8_t decimal = resp[RESP_TEMPERATURE_DEC];
		float temperature = integer + (float)(decimal / 100.0);
		set_pv(temp_pv, temperature);
	}
}

static void parse_pressure(uint8_t *resp, size_t len) {
	int node_id = resp[RESP_ID];
	aSubRecord *pressure_pv = get_pv(node_id, PRESSURE_ID);

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
	aSubRecord *humid_pv = get_pv(node_id, HUMIDITY_ID);

	if (humid_pv != 0)
		set_pv(humid_pv, resp[RESP_HUMIDITY_VAL]);
}

static void parse_gas(uint8_t *resp, size_t len) {
	int node_id = resp[RESP_ID];
	aSubRecord *gas_pv = get_pv(node_id, GAS_ID);
	aSubRecord *co_pv = get_pv(node_id, CO2_ID);
	aSubRecord *tvoc_pv = get_pv(node_id, TVOC_ID);

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
	aSubRecord *temp_interval_pv = get_pv(node_id, TEMP_INTERVAL_ID);
	aSubRecord *pressure_interval_pv = get_pv(node_id, PRESSURE_INTERVAL_ID);
	aSubRecord *humid_interval_pv = get_pv(node_id, HUMID_INTERVAL_ID);
	aSubRecord *gas_mode_pv = get_pv(node_id, GAS_MODE_ID);
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
	pvs[0] = get_pv(node_id, QUATERNION_W_ID);
	pvs[1] = get_pv(node_id, QUATERNION_X_ID);
	pvs[2] = get_pv(node_id, QUATERNION_Y_ID);
	pvs[3] = get_pv(node_id, QUATERNION_Z_ID);

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
	pvs[0] = get_pv(node_id, ACCEL_X_ID); pvs[1] = get_pv(node_id, ACCEL_Y_ID); pvs[2] = get_pv(node_id, ACCEL_Z_ID);
	pvs[3] = get_pv(node_id, GYRO_X_ID); pvs[4] = get_pv(node_id, GYRO_Y_ID); pvs[5] = get_pv(node_id, GYRO_Z_ID);
	pvs[6] = get_pv(node_id, COMPASS_X_ID); pvs[7] = get_pv(node_id, COMPASS_Y_ID); pvs[8] = get_pv(node_id, COMPASS_Z_ID);
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
	pvs[0] = get_pv(node_id, ROLL_ID);
	pvs[1] = get_pv(node_id, PITCH_ID);
	pvs[2] = get_pv(node_id, YAW_ID);
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
	aSubRecord *heading_pv = get_pv(node_id, HEADING_ID);
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
	aSubRecord *step_interval_pv = get_pv(node_id, STEP_INTERVAL_ID);
	aSubRecord *temp_comp_pv = get_pv(node_id, TEMP_COMP_INTERVAL_ID);
	aSubRecord *mag_comp_pv = get_pv(node_id, MAG_COMP_INTERVAL_ID);
	aSubRecord *frequency_pv = get_pv(node_id, MOTION_FREQ_ID);
	aSubRecord *wake_pv = get_pv(node_id, WAKE_ID);
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
	aSubRecord *min_interval_pv = get_pv(node_id, CONN_MIN_INTERVAL_ID);
	aSubRecord *max_interval_pv = get_pv(node_id, CONN_MAX_INTERVAL_ID);
	aSubRecord *latency_pv = get_pv(node_id, CONN_LATENCY_ID);
	aSubRecord *timeout_pv = get_pv(node_id, CONN_TIMEOUT_ID);
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
	aSubRecord *pv = get_pv(node_id, STATUS_ID);
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
	aSubRecord *pv = get_pv(node_id, CONNECTION_ID);
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
		node_id = get_actual_node_id(node_id);

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
	node_id = get_actual_node_id(node_id);

	uint8_t command[2];
	command[0] = opcode;
	command[1] = node_id;
	gattlib_write_char_by_uuid(gp_connection, &g_send_uuid, command, sizeof(command));
}

// fetch PV from linked list given node/PV IDs
aSubRecord* get_pv(int node_id, int pv_id) {
	if (g_ioc_started)
		node_id = g_custom_node_ids[node_id];

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
	if (g_custom_node_ids[node_id] != -1)
		node_id = g_custom_node_ids[node_id];

	float null = 0;
	int pv_id;
	PVnode *node = g_first_pv;
	aSubRecord *pv;
	while (node != 0) {
		pv_id = node->pv_id;
		if (node->node_id == node_id && pv_id != CONNECTION_ID && pv_id != STATUS_ID) {
			if (pv_id == BUTTON_ID)
				set_pv(node->pv, 0);
			else
				set_pv(node->pv, null);
		}
		node = node->next;
	}
}

void disconnect_node(int node_id) {
	// mark PVs null
	nullify_node_pvs(node_id);
	set_status(node_id, "DISCONNECTED");
	set_connection(node_id, DISCONNECTED);
}

int get_actual_node_id(int node_id) {
	for (int i=0; i<MAX_NODES; i++)
		if (g_custom_node_ids[i] == node_id) {
			//printf("custom id %d -> actual id %d\n", node_id, i);
			return i;
		}
	return -1;
}
