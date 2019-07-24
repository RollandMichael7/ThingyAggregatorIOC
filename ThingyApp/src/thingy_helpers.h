// helper functions

uuid_t aggregatorUUID(const char*);

void	parse_humidity(uint8_t*, size_t);
void	parse_button_report(uint8_t*, size_t);

int 	set_pv(aSubRecord*, float);
int 	set_status(int, char*);
int 	set_connection(int, float);
