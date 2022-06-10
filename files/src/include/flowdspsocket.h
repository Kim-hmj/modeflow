/*
 *  Magnetic Pattern is base on the value from FLOWDSP
 *  which is a TCP server. FlowdspSocket is a TCP client.
 *
 *  Author: <hoky.guan@tymphany.com>
*/
#ifndef LED_SOCKET_H
#define LED_SOCKET_H
#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
using namespace std;
struct socket_info
{
    const char *ip_addr;
    int port;
};

class FlowdspSocket {
public:
    FlowdspSocket(const char *ip_addr, int port);
    ~FlowdspSocket();
    static FlowdspSocket *Create(const char *ip_addr, int port);
    int Connect();
    int Send(const char *data_send);
    int Receive(char *data_receive);
    int closesock();

    enum LevelType { UNKNOWN = 0, RGB_LEVEL_1, RGB_LEVEL_2, RGB_LEVEL_3,
                     RGB_LEVEL_4, RGB_LEVEL_5, RGB_LEVEL_6,
                     RGB_LEVEL_7, RGB_LEVEL_8, RGB_LEVEL_9,
                     RGB_LEVEL_10, RGB_LEVEL_11, RGB_LEVEL_12
                   };

private:
    string ip_addr_;
    int port_;
    int client_ =  -1;
    struct sockaddr_in server_;
    int receive_size_ = 128;
    bool closed = false;
};
#endif
