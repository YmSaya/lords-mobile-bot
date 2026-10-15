#ifndef CONFIG_H
#define CONFIG_H

#include "connection.h"
#include "items.h"

void Configuration(Connection *client);
bool CreateDefaultConfig(const char *filename);
bool LoadConfig(Connection *c, const char *filename);
bool ReloadConfig(Connection *c, const char *filename);

char *TrimWhitespace(char *str);
bool ParseWhitelist(Connection *c, const char *value);
bool IsPlayerWhitelisted(const Connection *c, const char *player_name);

#endif