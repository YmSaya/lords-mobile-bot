#ifndef COMMAND_PARSER
#define COMMAND_PARSER

#include <stdint.h>
#include "connection.h"

void command_handler(Connection *c, const char *player_name, const char *message);
void SuperUserAccess(Connection *c, const char *player_name, const char *new_admin);

#endif