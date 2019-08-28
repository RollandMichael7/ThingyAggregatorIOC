// helper functions

uuid_t aggregatorUUID(const char*);

void parse_resp(uint8_t*, size_t);

int set_pv(aSubRecord*, float);
int set_status(int, char*);
int set_connection(int, float);

void write_env_config_helper(int);
void write_conn_param_helper(int);
