#include "connection.h"

int set_nonblocking(Connection *conn)
{
#ifdef _WIN32
    u_long mode = 1;
    return ioctlsocket(conn->sock, FIONBIO, &mode);
#else
    int flags = fcntl(conn->sock, F_GETFL, 0);
    if (flags < 0) return -1;

    return fcntl(conn->sock, F_SETFL, flags | O_NONBLOCK);
#endif
}

int connect_server(const char *ip, unsigned short port)
{
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket");
        return -1;
    }

    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);

    if (inet_pton(AF_INET, ip, &serv_addr.sin_addr) <= 0) {
        perror("inet_pton");
        close_socket(sock);
        return -1;
    }

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("connect");
        close_socket(sock);
        return -1;
    }

    return sock;
}

bool send_packet(Connection *conn, bool enc)
{
	if (enc) {
		EncryptData(conn->data + 4, conn->size - 4, conn->data + 4, ENCRYPTION_KEY);
	}
	
    return send(conn->sock, conn->data, conn->size, 0) == conn->size;
}

void disconnect(Connection *c)
{
    if (!c)
        return;

    if (c->sock >= 0) {
        close_socket(c->sock);
        c->sock = -1;
    }
}


void reset_connection(Connection *c)
{
    if (!c)
        return;

    memset(c, 0, sizeof(*c));
    c->sock = -1;
}

void Reconnect_Init(ReconnectTracker *rt)
{
    if (!rt)
        return;

    rt->initial_delay_sec = 2;
    rt->max_delay_sec = 60;
    rt->current_delay_sec = 2;
    rt->reconnect_attempts = 0;
    rt->reconnecting = false;
}

uint32_t Reconnect_GetNextBackoff(ReconnectTracker *rt)
{
    if (!rt)
        return 2;

    uint32_t delay = rt->current_delay_sec;
    rt->reconnect_attempts++;
    rt->reconnecting = true;

    uint32_t next = rt->current_delay_sec * 2;
    if (next > rt->max_delay_sec || next < rt->current_delay_sec) {
        next = rt->max_delay_sec;
    }
    rt->current_delay_sec = next;
    return delay;
}

void Reconnect_Reset(ReconnectTracker *rt)
{
    if (!rt)
        return;

    rt->current_delay_sec = rt->initial_delay_sec ? rt->initial_delay_sec : 2;
    rt->reconnect_attempts = 0;
    rt->reconnecting = false;
}