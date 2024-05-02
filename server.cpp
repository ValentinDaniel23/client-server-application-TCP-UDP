#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <iostream>
#include <arpa/inet.h>
#include <map>
#include <vector>
#include <fstream>
#include <sstream>

#include <sys/time.h>
#include <sys/timerfd.h>

#include "common.h"
#include "helpers.h"

using namespace std;

char topic[TOPIC_SIZE];

struct IDs
{
    char id[ID_SIZE];
    int waiting, socket;
    sockaddr_in tcp_addr;
};

map<string, IDs> unique_ID;
map<int, IDs> unique_SOCKET;
map<string, vector<string>> clients_TOPIC;

sockaddr_in coi_leray;

string appINT(udp_packet &udp_packet, sockaddr_in &addr) {
    stringstream ss;
    ss << inet_ntoa(addr.sin_addr)<<':'<<addr.sin_port;
    ss << " - " << topic << " - INT - ";
    if (*((uint8_t *) udp_packet.content) == 0 || ntohl(*((uint32_t *) (udp_packet.content + 1))) == 0) {
        ss << ntohl(*((uint32_t *) (udp_packet.content + 1)));
    } else {
        ss << '-' << ntohl(*((uint32_t *) (udp_packet.content + 1)));
    }
    
    return ss.str();
}

string appSHORT_REAL(udp_packet &udp_packet, sockaddr_in &addr) {
    stringstream ss;
    ss << inet_ntoa(addr.sin_addr)<<':'<<addr.sin_port;
    ss << " - " << topic << " - SHORT_REAL - ";
    ss << ntohs(*((uint16_t *) udp_packet.content)) / 100 << '.';
    if (ntohs(*((uint16_t *) udp_packet.content)) % 100 < 10) {
        ss << '0';
    }
    ss << ntohs(*((uint16_t *) udp_packet.content)) % 100;

    return ss.str();
}

string appFLOAT(udp_packet &udp_packet, sockaddr_in &addr) {
    stringstream ss;
    ss << inet_ntoa(addr.sin_addr)<<':'<<addr.sin_port;
    ss << " - " << topic << " - FLOAT - ";

    if (*((uint8_t *) udp_packet.content) == 1 && ntohl(*((uint32_t *) (udp_packet.content + 1))) != 0) {
        ss << '-';
    }
    uint32_t put = 1, number = ntohl(*((uint32_t *) (udp_packet.content + 1)));
    uint8_t exp = *((uint8_t *) udp_packet.content + 5);

    if (exp == 0) {
        ss << number;
    } else {
        for (int i = 0; i < exp; i++) {
            put *= 10;
        }
        ss << number / put << '.';
        while (put > 1) {
            put /= 10;
            ss << (number / put) % 10;
        }
    }

    return ss.str();
}

string appSTRING(udp_packet &udp_packet, sockaddr_in &addr) {
    stringstream ss;
    ss << inet_ntoa(addr.sin_addr)<<':'<<addr.sin_port;
    ss << " - " << topic << " - STRING - ";
    for (int i = 0; i < 1500; i++) {
        if (udp_packet.content[i] == '\n' || udp_packet.content[i] == '\0') {
            break;
        } else {
            ss << udp_packet.content[i];
        }
    }

    return ss.str();
}

string app(udp_packet &udp_packet, sockaddr_in &addr) {
    if (udp_packet.data_type == 0) {
        return appINT(udp_packet, addr);
    } else if (udp_packet.data_type == 1) {
        return appSHORT_REAL(udp_packet, addr);
    } else if (udp_packet.data_type == 2) {
        return appFLOAT(udp_packet, addr);
    } else {
        return appSTRING(udp_packet, addr);
    }
}

bool match(string a, string b)
{
    char sa[TOPIC_SIZE], sb[TOPIC_SIZE];
    strcpy(sa, a.c_str());
    strcpy(sb, b.c_str());

    char lista[51][51], listb[51][51];   // lists of words for both string
    int nra = 0, nrb = 0;

    char* token = nullptr;

    token = strtok(sa, "/");
    while (token != nullptr) {
        nra++;
        strcpy(lista[nra], token);
        token = strtok(nullptr, "/");
    }

    token = strtok(sb, "/");
    while (token != nullptr) {
        nrb++;
        strcpy(listb[nrb], token);
        token = strtok(nullptr, "/");
    }

    if (nra == 0 || nrb == 0) return false;

    int i = 1, j = 1;

    while (i <= nra && j <= nrb) {   // algorithm for matching words
        if (listb[j][0] == '+') {
            i++;
            j++;
            continue;
        }
        if (listb[j][0] == '*') {
            j++;
            if (j == nrb + 1) return true;
            while (i <= nra) {
                if ( strcmp(lista[i], listb[j]) == 0 ) break;
                i++;
            }
            if (i == nra + 1) return false;
            continue;
        }
        if (strcmp(lista[i], listb[j]) == 0) {
            i++;
            j++;
            continue;
        }
        return false;
    }

    if (i <= nra || j <= nrb) return false;
    return true;
}

void run_chat_multi_server(int listenTCPfd, int listenUDPfd)
{
    tcp_packet tcp_packet;
    udp_packet udp_packet;
    sockaddr_in addr;
    socklen_t len = sizeof(addr);
    char buf[BUFSIZ];
    memset(buf, 0, BUFSIZ);
    vector<pollfd> poll_fds(3, {-1, POLLIN});

    int rc;
    rc = listen(listenTCPfd, SOMAXCONN);
    DIE(rc < 0, "listen");

    poll_fds[0].fd = STDIN_FILENO;
    poll_fds[0].events = POLLIN;

    poll_fds[1].fd = listenTCPfd;
    poll_fds[1].events = POLLIN;

    poll_fds[2].fd = listenUDPfd;
    poll_fds[2].events = POLLIN;

    while (1)
    {
        rc = poll(poll_fds.data(), poll_fds.size(), -1);
        DIE(rc < 0, "poll");

        if (poll_fds[0].revents & POLLIN)
        {
            fgets(buf, sizeof(buf), stdin);

            if (strcmp(buf, "exit\n") == 0) {
                for (int i = 3; i < (int)poll_fds.size(); i++) {
                    tcp_packet.metadata.size = htonl(0);
                    tcp_packet.metadata.type = 0;
                    rc = send_all(poll_fds[i].fd, &tcp_packet);
                    DIE(rc < 0, "send");
                    
                    close(poll_fds[i].fd);
                }
                unique_ID.clear();
                unique_SOCKET.clear();
                clients_TOPIC.clear();
                poll_fds.clear();

                close(listenTCPfd);
                close(listenUDPfd);
                close(STDIN_FILENO);
                return;
            }
            else
                cerr << "close server with command: \"exit\"\n";
        }

        if (poll_fds[1].revents & POLLIN)   // new client
        {            
            const int newsockfd = accept(listenTCPfd, (sockaddr *)&addr, &len);
            DIE(newsockfd < 0, "accept");

            const int enable = 1;
            if (setsockopt(newsockfd, IPPROTO_TCP, TCP_NODELAY, &enable, sizeof(int)) < 0)
                perror("setsockopt(TCP_NODELAY) failed for TCP");
            
            if (setsockopt(newsockfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0)
                perror("setsockopt(SO_REUSEADDR) failed for UDP");

            pollfd pollfd;
            pollfd.fd = newsockfd;
            pollfd.events = POLLIN;
            poll_fds.push_back(pollfd);

            IDs ids;
            ids.waiting = 1;
            ids.socket = newsockfd;
            ids.tcp_addr = addr;
            unique_SOCKET[newsockfd] = ids;   // add the socket to list of sockets
        }

        if (poll_fds[2].revents & POLLIN)   // new notification
        {   
            rc = recvfrom(listenUDPfd, &udp_packet, sizeof(udp_packet), 0, (sockaddr *)&addr, &len);
            DIE(rc < 0, "recvfrom");
            
            memcpy(topic, udp_packet.topic, 50);
            string subject = string(topic);
            string msj = app(udp_packet, addr);

            tcp_packet.metadata.size = htonl(msj.size());
            tcp_packet.metadata.type = 1;
            strcpy(tcp_packet.data, msj.c_str());

            for (int i = 3; i < (int)poll_fds.size(); i++) {
                string id = string(unique_SOCKET[poll_fds[i].fd].id);
                auto topics = clients_TOPIC.find(id);
                for (string path : topics->second) {
                    if (match(subject, path)) {   // check if client is subscribed to subject
                        rc = send_all(poll_fds[i].fd, &tcp_packet);
                        DIE(rc < 0, "send");
                        break;
                    }
                }
            }
        }
        // now the sockets for clients
        for (int i = 3; i < (int)poll_fds.size(); i++)
        {
            if (poll_fds[i].revents & POLLIN)
            {
                rc = recv_all(poll_fds[i].fd, &tcp_packet);
                DIE(rc < 0, "recv");
                
                if (tcp_packet.metadata.type == 2) {   // receive id of client
                    if (unique_ID.find(string(tcp_packet.data)) == unique_ID.end()) {   // first use of id
                        IDs ids = unique_SOCKET[poll_fds[i].fd];
                        ids.waiting = 0;
                        strcpy(ids.id, tcp_packet.data);
                        cout << "New client " << ids.id << " connected from " << inet_ntoa(ids.tcp_addr.sin_addr) << ":" << ntohs(ids.tcp_addr.sin_port) << '\n';

                        unique_SOCKET[poll_fds[i].fd] = ids;
                        unique_ID[string(tcp_packet.data)] = ids;
                    }
                    else {
                        IDs idsID = unique_ID[string(tcp_packet.data)];
                        IDs idsSOCKET = unique_SOCKET[poll_fds[i].fd];

                        if (idsID.waiting == 1) {   // free id
                            idsID.socket = poll_fds[i].fd;
                            idsID.waiting = 0;
                            idsID.tcp_addr = idsSOCKET.tcp_addr;   // known because of connect

                            cout << "New client " << idsID.id << " connected from " << inet_ntoa(idsID.tcp_addr.sin_addr) << ":" << ntohs(idsID.tcp_addr.sin_port) << '\n';

                            unique_SOCKET[poll_fds[i].fd] = idsID;
                            unique_ID[string(tcp_packet.data)] = idsID;
                        }
                        else {   // id in use
                            cout<< "Client " << idsID.id << " already connected.\n";
                            unique_SOCKET.erase(poll_fds[i].fd);

                            tcp_packet.metadata.size = htonl(0);
                            tcp_packet.metadata.type = 0;
                            rc = send_all(poll_fds[i].fd, &tcp_packet);
                            DIE(rc < 0, "send");

                            close(poll_fds[i].fd);
                            poll_fds.erase(poll_fds.begin() + i);
                        }
                    }
                }
                else if (tcp_packet.metadata.type == 0) {   // receive closing message
                    IDs ids = unique_SOCKET[poll_fds[i].fd];
                    unique_SOCKET.erase(ids.socket);   // erase socket from used socket
                    
                    ids = unique_ID[string(ids.id)];   // id is now free to use
                    ids.waiting = 1;
                    unique_ID[string(ids.id)] = ids;
                    
                    cout<<"Client "<<ids.id<<" disconnected.\n";

                    close(poll_fds[i].fd);
                    poll_fds.erase(poll_fds.begin() + i);
                }
                else if (tcp_packet.metadata.type == 3 || tcp_packet.metadata.type == 4) {
                    memcpy(topic, tcp_packet.data, 50);
                    string subject = string(topic);
                    string id = string(unique_SOCKET[poll_fds[i].fd].id);

                    if (tcp_packet.metadata.type == 3) {   // subscribe
                        auto topics = clients_TOPIC.find(id);

                        if ( topics == clients_TOPIC.end() ) {
                            clients_TOPIC[id] = vector<string>(1, subject);
                        }
                        else {
                            int found = 0;
                            for (string path : topics->second) {
                                if (path == subject) {   
                                    found = 1;
                                    break;
                                }
                            }
                            if (found == 0)
                                topics->second.push_back(subject);
                        }
                    }
                    else {   // unsubscribe
                        auto topics = clients_TOPIC.find(id);

                        if ( topics != clients_TOPIC.end() ) {
                            int found = -1, i = -1;
                            topics->second = clients_TOPIC[id];
                            for (string path : topics->second) {
                                i++;
                                if (path == subject) {
                                    found = i;
                                    break;
                                }
                            }
                            if (found != -1) topics->second.erase(topics->second.begin() + found);
                        }
                    }
                }
                else
                    cerr << "unknown packet format\n";
            }
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        cerr << "Usage: " << argv[0] << " <port>\n";
        return 1;
    }

    setvbuf(stdout, NULL, _IONBF, BUFSIZ);   // disable stdout buffering

    uint16_t port;
    int rc = sscanf(argv[1], "%hu", &port);
    DIE(rc != 1, "Given port is invalid");

    const int listenTCPfd = socket(AF_INET, SOCK_STREAM, 0);
    DIE(listenTCPfd < 0, "socket");

    const int listenUDPfd = socket(AF_INET, SOCK_DGRAM, 0);
    DIE(listenUDPfd < 0, "socket");

    const int enable = 1;
    if (setsockopt(listenTCPfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0)   // enabling SO_REUSEADDR for TCP socket
        DIE(1, "setsockopt(SO_REUSEADDR) failed for TCP");

    if (setsockopt(listenTCPfd, IPPROTO_TCP, TCP_NODELAY, &enable, sizeof(int)) < 0)   // disabling the Nagle algorithm
        DIE(1, "setsockopt(TCP_NODELAY) failed for TCP");

    sockaddr_in serv_addr;
    socklen_t socket_len = sizeof(sockaddr_in);

    memset(&serv_addr, 0, socket_len);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(port);

    rc = bind(listenTCPfd, (const sockaddr *)&serv_addr, socket_len);
    DIE(rc < 0, "failed to bind");

    rc = bind(listenUDPfd, (const sockaddr *)&serv_addr, socket_len);
    DIE(rc < 0, "failed to bind");

    run_chat_multi_server(listenTCPfd, listenUDPfd);

    return 0;
}