#ifndef CLI_H
#define CLI_H

#include "connection.h"

/* Thread-safe Command Queue API */
void CommandQueue_Init(CommandQueue *q);
void CommandQueue_Destroy(CommandQueue *q);
bool CommandQueue_Push(CommandQueue *q, const char *cmd);
bool CommandQueue_Pop(CommandQueue *q, char *out_cmd, size_t max_len);
int  CommandQueue_Count(CommandQueue *q);

/* CLI Management API */
bool CLI_Init(Connection *c);
void CLI_Cleanup(Connection *c);
void CLI_Tick(Connection *c);
void CLI_ProcessInput(Connection *c, const char *line);
void CLI_ShowStatus(Connection *c);

#endif /* CLI_H */
