/*
 ** server.c -- a stream socket server demo
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>

#define PORT "3490"  // the port users will be connecting to

#define BACKLOG 10     // how many pending connections queue will hold

void sigchld_handler(int s) {
	// waitpid() might overwrite errno, so we save and restore it:
	int saved_errno = errno;

	while (waitpid(-1, NULL, WNOHANG) > 0)
		;

	errno = saved_errno;
}

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa) {
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*) sa)->sin_addr);
	}

	return &(((struct sockaddr_in6*) sa)->sin6_addr);
}
void send_disconn(int sock_fd) {
	char *buf = "EOT";

	if (send(sock_fd, buf, strlen(buf), 0) == -1) {
		perror("send");
	}
}

void recv_ack(int sock_fd) {
	char buf[256];
	if (recv(sock_fd, buf, 256, 0) == -1) {
		perror("recv");
	}
	if (memcmp(buf, "ACK", 3) != 0) {
		fprintf(stderr, "unexpected response from client");
	}
}
void send_data(int sock_fd) {
	char buf[256];

	int fd = open("discovery.json", O_RDONLY);
	if (fd < 0) {
		fprintf(stderr, "unable to open discovery.json");
	}
	size_t nbytes = 0;
	/* send discovery file */
	while ((nbytes = read(fd, buf, sizeof(buf))) > 0) {
		if (send(sock_fd, buf, nbytes, 0) == -1) {
			perror("send");
			break;
		}
	}
	close(fd);
}

int main(void) {
	int sockfd, new_fd;  // listen on sock_fd, new connection on new_fd
	struct addrinfo hints, *servinfo, *p;
	struct sockaddr_storage their_addr; // connector's address information
	socklen_t sin_size;
	struct sigaction sa;
	int yes = 1;
	char s[INET6_ADDRSTRLEN];
	int rv;

    int use_inet_sock = 0;
    if (use_inet_sock) {
        memset(&hints, 0, sizeof hints);
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_flags = AI_PASSIVE; // use my IP

        if ((rv = getaddrinfo(NULL, PORT, &hints, &servinfo)) != 0) {
            fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
            return 1;
        }

        // loop through all the results and bind to the first we can
        for (p = servinfo; p != NULL; p = p->ai_next) {
            if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol))
                    == -1) {
                perror("server: socket");
                continue;
            }

            if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int))
                    == -1) {
                perror("setsockopt");
                exit(1);
            }

            if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
                close(sockfd);
                perror("server: bind");
                continue;
            }

            break;
        }

        freeaddrinfo(servinfo); // all done with this structure

        if (p == NULL) {
            fprintf(stderr, "server: failed to bind\n");
            exit(1);
        }
    } else {
        char *socket_path = "/tmp/vipo_socket";
        struct sockaddr_un addr;
        if ( (sockfd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
            perror("socket error");
            exit(-1);
        }
        
        memset(&addr, 0, sizeof(addr));
        addr.sun_family = AF_UNIX;
        if (*socket_path == '\0') {
            *addr.sun_path = '\0';
            strncpy(addr.sun_path+1, socket_path+1, sizeof(addr.sun_path)-2);
        } else {
            strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path)-1);
            unlink(socket_path);
        }
        
        if (bind(sockfd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
            perror("bind error");
            exit(-1);
        }
    }
    
	if (listen(sockfd, BACKLOG) == -1) {
		perror("listen");
		exit(1);
	}

	sa.sa_handler = sigchld_handler; // reap all dead processes
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	if (sigaction(SIGCHLD, &sa, NULL) == -1) {
		perror("sigaction");
		exit(1);
	}

	printf("server: waiting for connections...\n");

	while (1) {  // main accept() loop
		sin_size = sizeof their_addr;
		new_fd = accept(sockfd, (struct sockaddr *) &their_addr, &sin_size);
		if (new_fd == -1) {
			perror("accept");
			continue;
		}

        if (their_addr.ss_family == AF_INET || their_addr.ss_family == AF_INET6) {
            inet_ntop(their_addr.ss_family,
                    get_in_addr((struct sockaddr *) &their_addr), s, sizeof s);
            printf("server: got connection from %s\n", s);
        }

		if (!fork()) { // this is the child process
			close(sockfd); // child doesn't need the listener
			for (int i=0;i<3;i++) {
				printf("sending discovery data...\n");
				send_data(new_fd);
				printf("receiving acknowledgement...\n");
				recv_ack(new_fd);
			}
			printf("sending EOT...\n");
			send_disconn(new_fd);
			printf("closing connection...\n");
			close(new_fd);
			exit(0);
		}
		close(new_fd);  // parent doesn't need this
	}

	return 0;
}
