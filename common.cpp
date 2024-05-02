#include "common.h"

#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <iostream>
#include <string.h>


int recv_all(int sockfd, tcp_packet *data)
{
    ssize_t length = sizeof(tcp_packet::metadata);
    ssize_t remaining = length;
    // first read the metadata to know the size
    while (remaining > 0)
    {
        ssize_t received_bytes = recv(sockfd, &data->metadata + length - remaining, remaining, 0);
        if (received_bytes <= 0)
        {
            return -1;
        }
        remaining -= received_bytes;
    }
    
    length = ntohl(data->metadata.size);
    remaining = length;
    // the message
    while (remaining > 0)
    {
        ssize_t received_bytes = recv(sockfd, data->data + length - remaining, remaining, 0);
        if (received_bytes <= 0)
        {
            return -1;
        }
        remaining -= received_bytes;
    }
    data->data[length] = '\0';

    return length;
}

int send_all(int sockfd, tcp_packet *packet)
{
    ssize_t length = ntohl(packet->metadata.size) + sizeof(tcp_packet::metadata);
    ssize_t remaining = length;

    while (remaining > 0)
    {
        ssize_t sent_bytes = send(sockfd, (char *) packet + length - remaining, remaining, 0);
        if (sent_bytes <= 0) {
            return -1;
        }
        remaining -= sent_bytes;
    }

    return length;
}