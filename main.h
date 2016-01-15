#ifndef MAIN_H
#define MAIN_H

#include <iostream>
#define _WIN32_WINNT 0x501
#include <ws2tcpip.h>
#include <algorithm>
#include <process.h>
#include <vector>
#include <fcntl.h>
#include <string>
#include <algorithm>

#define PORT_NUMBER "3480"
#define DATA 1
#define CM_RETRIEVE 2
#define CM_CONNECT 3
#define CM_ADD 4
#define CM_DISCONNECT 5
#define CM_EXIT 6
#define CM_POWEROFF 7
#define SM_SUCCESS 8
#define SM_UNAVAILABLE 9
#define SM_CLIENTLIST 10
#define SM_FAILURE 11
#define SM_ALREADYUSED 12
#define SM_WELCOME 13
#define SM_SERVEROFF 14

#define MAX_MESSAGE 512

typedef struct _CHAT_MESSAGE
{
    unsigned short message_type; //header
    unsigned long source_ip;
    unsigned short source_port;
    unsigned short hn_length;
    const char* host_name;
    unsigned short data_size;
    const char* data; //actual data
} CHAT_MESSAGE;

typedef struct _IP_ADDR
{
    unsigned long source_ip;
    unsigned short source_port;
} IP_ADDR;

typedef struct _thread_socket
{
    int socket;
    sockaddr_in sai;
    IP_ADDR ip_port;
    std::string address_string;
    int thread_id;
} thread_socket;

int serializeMessage(char* buffer, CHAT_MESSAGE& cm);
CHAT_MESSAGE deserializeMessage(char* buffer);
void SendMessage(thread_socket* pts, unsigned short message_type, const char* = "", const char* = "");
void ProperClose(int sock);

#endif
