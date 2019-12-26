// helper functions

uuid_t aggregator_UUID(const char*);

void disconnect_node(int);

void parse_resp(uint8_t*, size_t);

int set_status(int, char*);
int set_connection(int, int);

long poll_command_pv(aSubRecord*, int);
void send_read_command(int, int);

void toggle_io_helper(int, int);

void write_env_config_helper(int);
void write_conn_param_helper(int);

int get_actual_node_id(int);
