#ifndef IPC_H
#define IPC_H

#include "connection.h"
#include <stdbool.h>
#include <stddef.h>

#define IPC_DEFAULT_PORT 28888
#define IPC_MAX_MSG_LEN 4096

/* Initialize IPC TCP listener on 127.0.0.1:port */
bool IPC_Init(Connection *c, uint16_t port);

/* Stop and clean up IPC TCP listener */
void IPC_Cleanup(Connection *c);

/* Process a single incoming JSON-Line message and produce response JSON */
bool IPC_ProcessCommand(Connection *c, const char *json_in, char *json_out, size_t max_out_len);

#endif /* IPC_H */
