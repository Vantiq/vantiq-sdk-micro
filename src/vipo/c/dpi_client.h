#ifndef VIPO_DPI_CLIENT_H
#define VIPO_DPI_CLIENT_H

struct dpi_client {
	int socket_fd;
};

typedef struct dpi_client dpi_client_t;

#define MAXHOSTNAMELEN 512
#define MAXPORTLEN 5

dpi_client_t *init_dpi_usock_client(char *socket_path);
dpi_client_t *init_dpi_inet_client(char *port);
char *fetch_discovery_msg(dpi_client_t *dpic);

#endif
