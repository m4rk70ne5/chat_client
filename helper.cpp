#include "main.h"

int serializeMessage(char* buffer, CHAT_MESSAGE& cm)
{
    //create a pointer to the buffer
    char* bufferAddress = buffer;
    buffer += sizeof(short); //make room for the total packet size
    //convert a chat_message to binary data
    //make sure it is "network" order
    unsigned short message = htons(cm.message_type);
    unsigned long ip = htonl(cm.source_ip);
    unsigned short portNumber = htons(cm.source_port);
    unsigned short hn_size = htons(cm.hn_length);
    unsigned short dataSize = htons(cm.data_size);

    memcpy(bufferAddress, &message, sizeof(short));
    bufferAddress += sizeof(short);
    memcpy(bufferAddress, &ip, sizeof(long));
    bufferAddress += sizeof(long);
    memcpy(bufferAddress, &portNumber, sizeof(short));
    bufferAddress += sizeof(short);
    memcpy(bufferAddress, &hn_size, sizeof(short));
    bufferAddress += sizeof(short);
    memcpy(bufferAddress, &cm.host_name, sizeof(char) * cm.hn_length);
    bufferAddress += sizeof(char) * cm.hn_length;
    memcpy(bufferAddress, &dataSize, sizeof(short));
    bufferAddress += sizeof(short);
    memcpy(bufferAddress, &cm.data, sizeof(char) * cm.data_size);
    bufferAddress += sizeof(char) * cm.data_size;

    return bufferAddress - buffer; //return the number of bytes written
}

//get a chat message from a buffer
CHAT_MESSAGE deserializeMessage(char* buffer)
{
    CHAT_MESSAGE cm;
    char* pBuffer = buffer;
    pBuffer += sizeof(short); //skip the first two bytes
    unsigned short* _pMessageType = (unsigned short*)pBuffer;
    pBuffer += sizeof(short);
    unsigned long* _pIp = (unsigned long*)(pBuffer);
    pBuffer += sizeof(unsigned long);
    unsigned short* _pPortNumber = (unsigned short*)pBuffer;
    pBuffer += sizeof(short);
    unsigned short* _pHostSize = (unsigned short*)pBuffer;
    pBuffer += sizeof(short);
    cm.hn_length = ntohs(*_pHostSize);
    memcpy(&cm.host_name, pBuffer, sizeof(char) * cm.hn_length);
    pBuffer += sizeof(char) * cm.hn_length;
    unsigned short* _pDataSize = (unsigned short*)pBuffer;
    pBuffer += sizeof(short);
    cm.message_type = ntohs(*_pMessageType); //convert to host order
    cm.source_ip = ntohl(*_pIp);
    cm.source_port = ntohs(*_pPortNumber);
    cm.data_size = ntohs(*_pDataSize);
    memcpy(&cm.data, pBuffer, sizeof(char) * cm.data_size);

    return cm;
}

void SendMessage(thread_socket* pts, unsigned short message_type, const char* data, const char* hostname)
{
    //craft a chat message
    CHAT_MESSAGE cm;
    char* buf = (char*)malloc(MAX_MESSAGE);

    cm.message_type = message_type;
    cm.source_ip = pts->ip_port.source_ip;
    cm.source_port = pts->ip_port.source_port;
    cm.hn_length = strlen(hostname);
    cm.host_name = hostname;
    cm.data_size = strlen(data);
    cm.data = data;

    //serialize it
    int num_of_bytes = serializeMessage(buf, cm);
    unsigned short num_bytes = htons(num_of_bytes); //put the total size of packet in header
    memcpy(buf, &num_bytes, sizeof(short));

    //send
    int bytes_left = num_of_bytes;
    while (bytes_left > 0)
    {
        //keep sending the message until there are no more bytes to send
        int bytes_sent = send(pts->socket, buf + (num_of_bytes - bytes_left), bytes_left, 0);
        bytes_left -= bytes_sent;
    }

    //free buffer
    free(buf);
}
