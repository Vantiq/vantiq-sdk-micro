//  dpi_client.c
//
//  Copyright © 2018 VANTIQ. All rights reserved.

/*
 ** client.c -- a socket client for the deep packet inspector
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/select.h>

#include <arpa/inet.h>

#include "dpi_client.h"
#include "vme.h"
#include "log.h"

#define INITBUFSIZE 512 // max number of bytes we can get at once
#define PROT_EOT "EOT"
#define PROT_ACK "ACK"

static const char *hostname = "localhost";

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa) {
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*) sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*) sa)->sin6_addr);
}

void build_fd_set(fd_set *read_fds, int listen_sock) {
    FD_ZERO(read_fds);
    FD_SET(listen_sock, read_fds);
}

void config_hostname_port(char *hostname, char *port)
{
    /* todo: read from config file */
    strncpy(hostname, "locahost", MAXHOSTNAMELEN);
    strncpy(port, "3490", MAXPORTLEN);
}

dpi_client_t *_dpi_client_alloc()
{
    dpi_client_t *dpic = malloc(sizeof(dpi_client_t));
    memset(dpic, 0, sizeof(dpi_client_t));
    return dpic;
}

dpi_client_t *init_dpi_usock_client(char *socket_path)
{
    struct sockaddr_un addr;
    int sock_fd;

    dpi_client_t *dpic = _dpi_client_alloc();
    dpic->socket_fd = -1;
    if ((sock_fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
        log_info("socket error");
        return NULL;
    }
    
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    // linux virtual socket address space
    if (*socket_path == '\0') {
        *addr.sun_path = '\0';
        strncpy(addr.sun_path+1, socket_path+1, sizeof(addr.sun_path)-2);
    } else {
        strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path)-1);
    }
    
    if (connect(sock_fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        log_info("client: connect %s", strerror(errno));
        return NULL;
    }

    dpic->socket_fd = sock_fd;
    return dpic;
}

dpi_client_t *init_dpi_inet_client(char *port)
{
    int sockfd;
    struct addrinfo hints, *servinfo, *p;
    int rv;
    char s[INET6_ADDRSTRLEN];

    dpi_client_t *dpic = _dpi_client_alloc();
    dpic->socket_fd = -1;
    
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    
    if ((rv = getaddrinfo(hostname, port, &hints, &servinfo)) != 0) {
        log_die("getaddrinfo: %s\n", gai_strerror(rv));
    }
    
    // loop through all the results and connect to the first we can
    for (p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            log_info("client: socket", strerror(errno));
            continue;
        }
        
        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            log_info("client: connect %s", strerror(errno));
            continue;
        }
        
        log_info("socket descriptor is %d", sockfd);
        dpic->socket_fd = sockfd;
        break;
    }
    
    
    if (p == NULL) {
        log_info("client: failed to connect\n");
        return NULL;
    }
    
    inet_ntop(p->ai_family, get_in_addr((struct sockaddr *) p->ai_addr), s, sizeof s);
    log_info("client: connecting to %s", s);
    
    freeaddrinfo(servinfo); // all done with this structure
    return dpic;
}

char *fetch_discovery_msg(dpi_client_t *dpic)
{
    size_t numbytes;
    fd_set read_fds;
    vmebuf_t  *buf = vmebuf_ensure_size(NULL, INITBUFSIZE);

    struct timeval timeout;
    timeout.tv_sec = 300;
    timeout.tv_usec = 0;

    build_fd_set(&read_fds, dpic->socket_fd);
    log_info("waiting for more data...");
    /* todo: do not time out */
    int activity = select(dpic->socket_fd + 1, &read_fds, NULL, NULL, &timeout);
    log_info("select: activity %d\n", activity);
    if (activity == 0) {
        log_info("client: select timed out");
        return NULL;
    }
    if (activity == -1) {
        log_info("client: error on select %s", strerror(errno));
        return NULL;
    }

    size_t avail = buf->limit - buf->len;
    while ((numbytes = recv(dpic->socket_fd, (&buf->data[0]) + buf->len, avail, 0)) == avail) {
        buf->len += numbytes;
        buf->data = realloc(buf->data, buf->limit*2);
        buf->limit = buf->limit*2;
        avail = buf->limit - buf->len;
    }

    if (numbytes == -1) {
        log_syserr("recv");
        /* NOTREACHED */
    } else if (numbytes == 0 && buf->len == 0) {
        log_info("server has closed the connection...\n");
        return NULL;
    } else if (numbytes == sizeof(PROT_EOT)-1 && memcmp(buf->data, PROT_EOT, sizeof(PROT_EOT)) == 0){
        log_info("received end of transmission");
        return NULL;
    } else {
        buf->len += numbytes;
        buf->data[buf->len] = '\0';
        log_debug("client: received '%s'\n", buf->data);
        numbytes = send(dpic->socket_fd, PROT_ACK, sizeof(PROT_ACK)-1, 0);
        if (numbytes == -1) {
            log_syserr("error sending acknowledgement");
        }
    }

    char *data = vmebuf_tostr(buf);
    vmebuf_dealloc(buf);
    return data;
}
