/*
 *  Magnetic Pattern is base on the value from FLOWDSP
 *  which is a TCP server. FlowdspSocket is a TCP client.
 *
 *  Author: <hoky.guan@tymphany.com>
*/

#include "flowdspsocket.h"

FlowdspSocket::FlowdspSocket(const char *ip_addr, int port):ip_addr_(ip_addr), port_(port)
{
    client_ = socket(AF_INET, SOCK_STREAM, 0);

    server_.sin_family = AF_INET;
    server_.sin_port = htons(port_);
    server_.sin_addr.s_addr = inet_addr(ip_addr_.c_str());
}

FlowdspSocket::~FlowdspSocket()
{
    if(closed == false)
        close(client_);
}

FlowdspSocket *FlowdspSocket::Create(const char *ip_addr, int port) {
    return new FlowdspSocket(ip_addr, port);
}

int FlowdspSocket::Connect()
{
#ifdef X86_DEBUG
    return 0;
#endif
    if (connect(client_, (struct sockaddr*)&server_, sizeof(server_)) == -1)
        return 1;
    else
        return 0;
}

int FlowdspSocket::Send(const char *data_send)
{
#ifdef X86_DEBUG
    return 0;
#endif
    if (send(client_, data_send, strlen(data_send), 0) == -1)
        return 1;
    else
        return 0;
}

int FlowdspSocket::Receive(char *data_receive)
{
#ifdef X86_DEBUG
    return 0;
#endif

    int ret = recv(client_, data_receive, receive_size_, 0);
    if (ret == -1)
        return 1;
    else
        return 0;
}

int FlowdspSocket::closesock()
{
#ifdef X86_DEBUG
    return 0;
#endif
    if(close(client_) == 0) {
        closed = true;
    }
}

