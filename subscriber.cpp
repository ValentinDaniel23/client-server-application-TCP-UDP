#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <iostream>
#include <map>
#include <fstream>

#include "common.h"
#include "helpers.h"

using namespace std;

char id[ID_SIZE];

void run_client(int sockfd) {
    tcp_packet tcp_packet;
    char topic[TOPIC_SIZE];
    memset(topic, 0, TOPIC_SIZE);
    char buf[BUFSIZ];
    memset(buf, 0, BUFSIZ);
    int rc;

    struct pollfd pfds[2];
    pfds[0].fd = STDIN_FILENO;
    pfds[0].events = POLLIN;

    pfds[1].fd = sockfd;
    pfds[1].events = POLLIN;

    while (1) {
        rc = poll(pfds, 2, -1);
        DIE(rc < 0, "poll");

        if (pfds[0].revents & POLLIN) {
            fgets(buf, sizeof(buf), stdin);

            if (strncmp(buf, "subscribe ", strlen("subscribe ")) == 0) {   // take topic, send subscribed
                char *msj = buf + strlen("subscribe ");
                strncpy(tcp_packet.data, msj, 50);
                for (int i=0; i<50; i++)
                    if (tcp_packet.data[i] == '\0') break;
                    else if (tcp_packet.data[i] == '\n' || tcp_packet.data[i] == ' ') {
                        tcp_packet.data[i] = '\0';
                        break;
                    }
                tcp_packet.metadata.size = htonl(strlen(tcp_packet.data));
                tcp_packet.metadata.type = 3;
                rc = send_all(sockfd, &tcp_packet);
                DIE(rc < 0, "send");

                cout<<"Subscribed to topic "<<msj;
            } 
            else if (strncmp(buf, "unsubscribe ", strlen("unsubscribe ")) == 0) {   // take topic, send unsubscribed
                char *msj = buf + strlen("unsubscribe ");
                strncpy(tcp_packet.data, msj, 50);
                for (int i=0; i<50; i++)
                    if (tcp_packet.data[i] == '\0') break;
                    else if (tcp_packet.data[i] == '\n' || tcp_packet.data[i] == ' ') {
                        tcp_packet.data[i] = '\0';
                        break;
                    }
                tcp_packet.metadata.size = htonl(strlen(tcp_packet.data));
                tcp_packet.metadata.type = 4;
                rc = send_all(sockfd, &tcp_packet);
                DIE(rc < 0, "send");

                cout<<"Unsubscribed from topic "<<msj;
            } 
            else if (strcmp(buf, "exit\n") == 0) {   // close socket
                tcp_packet.metadata.size = htonl(0);
                tcp_packet.metadata.type = 0;
                rc = send_all(sockfd, &tcp_packet);
                DIE(rc < 0, "send");

                close(sockfd);
                close(STDIN_FILENO);
                break;
            }
            else
                cerr << sizeof(tcp_packet) << "close server with command \"exit\" or try to \"subscribe <TOPIC>\" or \"unsubscribe <TOPIC>\"\n";
        }

        if (pfds[1].revents & POLLIN) {   // packet from socket
            rc = recv_all(pfds[1].fd, &tcp_packet);
            DIE(rc < 0, "recv");

            if (tcp_packet.metadata.type == 1) {   // a valid packet
                cout<<tcp_packet.data<<'\n';
            }
            else if (tcp_packet.metadata.type == 0) {   // exit
                close(sockfd);
                close(STDIN_FILENO);
                break;
            }
            else
                cerr << "unknown packet format\n";
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc != 4) {
        cerr << "\n Usage: " << argv[0] << " <ID_CLIENT> <IP_SERVER> <PORT_SERVER>\n";
        return 1;
    }
    
    setvbuf(stdout, NULL, _IONBF, BUFSIZ);

    strcpy(id, argv[1]);

    uint16_t port;
    int rc = sscanf(argv[3], "%hu", &port);
    DIE(rc != 1, "Given port is invalid");

    const int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    DIE(sockfd < 0, "socket");

    const int enable = 1;

    if (setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, &enable, sizeof(int)) < 0)
        perror("setsockopt(TCP_NODELAY) failed for TCP");

    struct sockaddr_in serv_addr;
    socklen_t socket_len = sizeof(struct sockaddr_in);

    memset(&serv_addr, 0, socket_len);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    rc = inet_pton(AF_INET, argv[2], &serv_addr.sin_addr.s_addr);
    DIE(rc <= 0, "inet_pton");

    rc = connect(sockfd, (struct sockaddr *)&serv_addr, socket_len);
    DIE(rc < 0, "connect");

    tcp_packet send_id;
    send_id.metadata.size = htonl(strlen(id));
    send_id.metadata.type = 2;
    strcpy(send_id.data, id);
    rc = send_all(sockfd, &send_id);
    DIE(rc < 0, "send");

    run_client(sockfd);


    return 0;
}
