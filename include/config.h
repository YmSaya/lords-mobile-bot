#ifndef CONFIG_H
#define CONFIG_H

#include "connection.h"
#include "items.h"

void Configuration(Connection *client);
bool CreateDefaultConfig(const char *filename);
bool LoadConfig(Connection *c, const char *filename);

#endif