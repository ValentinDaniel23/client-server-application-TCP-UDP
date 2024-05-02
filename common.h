#ifndef __COMMON_H__
#define __COMMON_H__

#include <stddef.h>
#include <stdint.h>
#include <iostream>
#include <string>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

using namespace std;

#define ID_SIZE 11
#define TOPIC_SIZE 51 

struct udp_packet {
  char topic[50];
  uint8_t data_type;
  char content[1500];
};

struct tcp_packet {
  struct __attribute__((packed)) {
    int       size;
    uint8_t   type;
  } metadata;
  char data[1600];
};

int recv_all(int sockfd, tcp_packet *data);
int send_all(int sockfd, tcp_packet *buffer);

#endif
