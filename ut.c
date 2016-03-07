#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/ip.h>
#include <arpa/inet.h>

static struct option options[] = {
    { "bindaddr", required_argument, NULL, 'b' },
    { "bindport", required_argument, NULL, 'f' },
    { "connectaddr", required_argument, NULL, 'c' },
    { "connectport", required_argument, NULL, 'p' },
    { "sendaddr", required_argument, NULL, 's' },
    { "sendport", required_argument, NULL, 't' },
    { "interval", required_argument, NULL, 'i' },
    { "message", required_argument, NULL, 'm' },
    { "reuseaddr", no_argument, NULL, 'r' },
    { "echo", no_argument, NULL, 'e' },
    { NULL }
};

int main(int argc, char **argv) {
    int n = 0, sock, enable = 1, rtn, sel = 1, max_fd, interval = 0, reuse = 0, echo = 0;
    in_addr_t bindaddr = INADDR_ANY, connectaddr = INADDR_ANY, sendaddr = INADDR_ANY;
    uint16_t bindport = 0, connectport = 0, sendport = 0;
    struct sockaddr_in sin_me, sin_peer, sin_from, sin_to;
    socklen_t addrlen;
    fd_set fd_rd;
    struct timeval select_timeout;
    char *msg = "Test";
    char txbuffer[65536];
    char rxbuffer[65536];

    while (n >= 0) {
        n = getopt_long(argc, argv, "b:f:c:p:s:t:i:m:re", options, NULL);
        switch (n) {
            case 'b':
                if ((bindaddr = inet_addr(optarg)) == -1) {
                    fprintf(stderr, "Invalid address: %s\n", optarg);
                    exit(1);
                }
                break;
            case 'c':
                if ((connectaddr = inet_addr(optarg)) == -1) {
                    fprintf(stderr, "Invalid address: %s\n", optarg);
                    exit(1);
                }
                break;
            case 's':
                if ((sendaddr = inet_addr(optarg)) == -1) {
                    fprintf(stderr, "Invalid address: %s\n", optarg);
                    exit(1);
                }
                break;
            case 'f':
                bindport = atoi(optarg);
                break;
            case 'p':
                connectport = atoi(optarg);
                break;
            case 't':
                sendport = atoi(optarg);
                break;
            case 'i':
                interval = atoi(optarg);
                break;
            case 'm':
                msg = optarg;
                break;
            case 'r':
                reuse = 1;
                break;
            case 'e':
                echo = 1;
                break;
		}
    }

    // setup socket
    if ((sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
        perror("socket");
        exit(1);
    }
    if (reuse) {
        printf("setsockopt(SO_REUSEADDR)\n");

        if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable)) < 0) {
            perror("setsockopt(SO_REUSEADDR)");
            exit(1);
        }
    }

    // bind to address / port on this host
    memset(&sin_me, 0, sizeof(sin_me));
    sin_me.sin_family = AF_INET;
    sin_me.sin_port = htons(bindport);
    sin_me.sin_addr.s_addr = bindaddr;

    if (bindaddr || bindport) {
        printf("bind %s:%d\n", inet_ntoa(sin_me.sin_addr), ntohs(sin_me.sin_port));

        if (bind(sock, (struct sockaddr *)&sin_me, sizeof(sin_me)) != 0) {
            perror("bind");
            exit(1);
        }
    }

    // "connect" to other end
    memset(&sin_peer, 0, sizeof(sin_peer));
    sin_peer.sin_family = AF_INET;
    sin_peer.sin_port = htons(connectport);
    sin_peer.sin_addr.s_addr = connectaddr;

    if (connectaddr || connectport) {
        printf("connect %s:%d\n", inet_ntoa(sin_peer.sin_addr), ntohs(sin_peer.sin_port));

        if (connect(sock, (struct sockaddr *)&sin_peer, sizeof(sin_peer)) < 0) {
            perror("connect");
            exit(1);
        }
    }

    while (1) {
        strcpy(txbuffer, msg);

        FD_ZERO(&fd_rd);
        max_fd = sock;
        FD_SET(sock, &fd_rd);

        select_timeout.tv_sec = 0;
        select_timeout.tv_usec = interval * 1000;

        // if an interval is set, use select to wait
        if (interval && (sel = select(max_fd + 1, &fd_rd, NULL, NULL, &select_timeout)) < 0 ) {
            perror("select");
            exit(1);
        }

        if (!interval || sel) {
            addrlen = sizeof(sin_from);
            if ((rtn = recvfrom(sock, (void *)rxbuffer, sizeof(rxbuffer), 0, (struct sockaddr *)&sin_from, &addrlen)) <= 0) {
                perror("recvfrom");
            }
            else {
                rxbuffer[rtn] = 0;
                printf("recvfrom %s:%d : %s\n", inet_ntoa(sin_from.sin_addr), ntohs(sin_from.sin_port), rxbuffer);
                if (echo) {
                    strcpy(txbuffer, rxbuffer);
                    sel = 0;
                }
            }
        }
        if (!sel) {
            if (sendaddr || sendport) {
                memset(&sin_to, 0, sizeof(sin_to));
                sin_to.sin_family = AF_INET;
                sin_to.sin_port = htons(sendport);
                sin_to.sin_addr.s_addr = sendaddr;

                printf("sendto %s:%d : %s\n", inet_ntoa(sin_to.sin_addr), ntohs(sin_to.sin_port), txbuffer);

                if ((rtn = sendto(sock, (void *)txbuffer, strlen(txbuffer), 0, (struct sockaddr *)&sin_to, sizeof(sin_to))) <= 0) {
                    perror("sendto");
                }
            }
            else {
                printf("send %s:%d : %s\n", inet_ntoa(sin_peer.sin_addr), ntohs(sin_peer.sin_port), txbuffer);

                if ((rtn = send(sock, (void *)txbuffer, strlen(txbuffer), 0)) <= 0) {
                    perror("send");
                }
            }
        }
    }

    return 0;
}