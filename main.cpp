#include "../network_common/main.h"

using namespace std;

bool lldone = false;
bool forceQuit = false;
string clientName;

bool CheckForExit(string input)
{
    transform(input.begin(), input.end(), input.begin(), ::tolower); //convert to lower case
    return (input == "exit");
}

string InterpretClientList(const char* data)
{
    //format - name ipaddress:port|1, name ipaddress:port|0, [next record]...
    //the 1 and 0 signify whether or not the client is connected
    string client_list = "";
    string data_string;
    data_string.assign(data);
    if (data_string == "")
        return "No Clients Available";
    size_t pos = -1;
    do
    {
        size_t temp_pos = pos + 1;
        pos = data_string.find(',', temp_pos); //get the position of the comma
        //now extract all the characters before that
        if (pos != string::npos)
        {
            string temp = data_string.substr(temp_pos, pos - temp_pos);
            //now get the name, ipaddress, and whether or not you're connected to them
            string temp_two = "";
            //name and ipaddress
            size_t pos_two = temp.find('|', 0);
            client_list += temp.substr(0, pos_two);
            //connection status
            if (temp[++pos_two] == '0')
                client_list += " (Not Connected)\n";
            else
                client_list += " (Connected)\n";
        }
    } while (pos != string::npos);
    return client_list;
}

int WaitForMessage(int socket, char* data, CHAT_MESSAGE& cm, int timeout_value)
{
    char incomingBuf[128];
    memset(data, 0, sizeof(char) * MAX_MESSAGE);
    char* originalData = data;
    int status = -1; //server sent nothing by default
    //memset(&cm, 0, sizeof(cm));
    if (timeout_value > 0)
    {
        //at this point the socket should be in blocking mode
        cout << "Waiting for server...\n";
        //however, there should be a reasonable timeout value
        struct timeval tv;
        tv.tv_sec = timeout_value;
        setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, (char*)&tv, sizeof(struct timeval));
    }
    unsigned int packetSize = 0, totalReceived = 0;

    int bytesReceived = recv(socket, incomingBuf, 256, 0);
    if (bytesReceived > 0)
    {
        //grab the packet data
        //now check to see if the number of bytes received is less than the packet length
        do
        {
            if (packetSize == 0)
            { //only do this once
                unsigned long* _pMessageSize = (unsigned long*)incomingBuf;
                packetSize = ntohl(*_pMessageSize); //convert to host order
            }
            memcpy(data, incomingBuf, sizeof(char) * packetSize);
            data += bytesReceived;
            totalReceived += bytesReceived;
        } while (totalReceived < packetSize);
        //deserialize the message
        cout << "Deserializing Message...\n";
        cm = deserializeMessage(originalData, cm.data);
        //data = originalData;
        //memcpy(data, cm.data, cm.data_size); //we can overwrite the originalData
        //data[dataSize] = '\0'; //make it a zero-terminated string
        status = cm.message_type;
    }
    else if (bytesReceived == 0)
        //server force-quitted
        forceQuit = true;
    return status;
}

void InputLoop(void* lParam)
{
    //keep running the loop until the user types "disconnect"
    thread_socket* pts = (thread_socket*)lParam;
    char inpline[MAX_MESSAGE];
    string input;
    cin.clear();
    cin.ignore(numeric_limits<streamsize>::max(), '\n'); //"flush" the buffer
    do
    {
        string command, argument = "";
        cin.getline(inpline, MAX_MESSAGE); //get some input
        //now extract it into a command
        cout << inpline << endl;
        input.assign(inpline);
        size_t pos = input.find(' ', 0);
        if (pos != string::npos)
            command = input.substr(0, pos);
        else
            command = input; //lone command

        transform(command.begin(), command.end(), command.begin(), ::tolower); //turn it to lower case

        //extract the argument
        if (pos != string::npos)
        {
            size_t temp_pos = pos + 1;
            pos = input.find(' ', temp_pos);
            if (pos == string::npos)
                //that means it is not data
                argument = input.substr(temp_pos, input.length() - temp_pos);
        }

        cout << "command = " << command << " " << "argument = " << argument << endl;

        if (command == "?" && argument == "")
        {
            cout << "Here is a list of available commands:  " << endl;
            cout << "? -- this list" << endl;
            cout << "connect [host] -- all subsequent data messages will be sent also to this host" << endl;
            cout << "disconnect [host] -- no more data messages will be sent to this host" << endl;
            cout << "disconnect/quit/exit -- closes the connection with everybody, including server" << endl;
            cout << "poweroff [password] -- shuts the server down" << endl;
            cout << "whoami -- prints the name of yourself" << endl;
        }
        else if ((command == "disconnect" || command == "quit" || command == "exit"))
        {
            if (argument == "")
                //send a CM_EXIT command
                SendMessage(pts, CM_EXIT);
            else
                //send a disconnect command
                SendMessage(pts, CM_DISCONNECT, argument.c_str());
        }
        else if (command == "connect" && argument != "")
            SendMessage(pts, CM_CONNECT, argument.c_str());
        else if (command == "poweroff" && argument != "")
            SendMessage(pts, CM_POWEROFF, argument.c_str());
        else if (command == "available" && argument == "")
            SendMessage(pts, CM_RETRIEVE);
        else if (command == "whoami" && argument == "")
            cout << clientName << endl;
        else
            SendMessage(pts, DATA, input.c_str()); //send data
        //this is for the loop to quit
        if (argument == "")
            input = command;
    } while(input != "disconnect" && input != "quit" && input != "exit");
    lldone = true;
    _endthread();
}

void HandleMessage(CHAT_MESSAGE& cm)
{
    switch (cm.message_type)
    {
        case SM_SERVEROFF:
        {
            //server turning off
            cout << "Server turned off!" << endl;
            lldone = true;
            forceQuit = true;
        }
        break;
        case SM_UNAVAILABLE:
        {
            //print [host] disconnected
            string unavailable_host;
            unavailable_host.assign(cm.data);
            cout << unavailable_host << " disconnected!" << endl;
        }
        break;
        case DATA:
        case SM_SUCCESS:
        case SM_FAILURE:
        {
            //just print the message
            string message;
            message.assign(cm.data);
            cout << message << endl;
        }
        break;
        case SM_CLIENTLIST:
        { //decode the client list
            cout << "List of Clients:\n\n" << InterpretClientList(cm.data) << endl;
        }
        break;
    }
}

void ProperClose(int sock, char* buf)
{
    shutdown(sock, SD_SEND); //I will send no more data
    unsigned long iMode = 0;
    CHAT_MESSAGE cm;
    ioctlsocket(sock, FIONBIO, &iMode); //set socket to blocking
    while (WaitForMessage(sock, buf, cm, 10) != -1) //receive the last messages
        HandleMessage(cm);
    closesocket(sock); //[[]]
}

int main()
{
    //wsa startup crap
    WSADATA wsadata;
    int result = WSAStartup(MAKEWORD(2,2), &wsadata); //start up WSA
    bool serverConnected = false, exited = false;
    int sockfd; //this is the socket existing between client and server
    thread_socket* pts; //thread_socket structure for client and server
    DWORD ipbuffer_length = 256;
    char* data = (char*)malloc(MAX_MESSAGE); //contains data for our chat messages
    CHAT_MESSAGE cm; //used for receiving messages
    memset(&cm, 0, sizeof(cm));
    cm.data = data; //make sure the data points to valid, allocated space
    while (!serverConnected && !exited)
    {
        string input = "";
        cout << "Hi, welcome to Chat Spaces!  Type server name [type 'exit' to quit]:  ";
        cin >> input;
        if (CheckForExit(input))
        {
            exited = true;
            continue;
        }
        //now, attempt to connect to the server using the given port number
        addrinfo hints;
        addrinfo* servInfo, *res; //output addrinfo linked list

        memset(&hints, 0, sizeof(hints));

        hints.ai_family = AF_INET; //internet address
        hints.ai_socktype = SOCK_STREAM; //socket

        int status = getaddrinfo(input.c_str(), PORT_NUMBER, &hints, &servInfo);
        if (status != 0)
            cout << "getaddrinfo error: " << gai_strerror(status) << endl;
        else
        {
            //okay, we were able to get some addresses
            //let's try to connect now
            bool connected = false;
            for (res = servInfo; res != NULL && !connected; res = res->ai_next)
            {
                sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol); //create the socket
                if (sockfd == -1)
                    continue;
                if (connect(sockfd, res->ai_addr, res->ai_addrlen) == -1) //now connect to it
                {
                    closesocket(sockfd);
                    continue;
                }
                else
                {
                    serverConnected = true;
                    connected = true;
                }
            }
            if (res == NULL)
                //guess we didn't connect
                cout << "Couldn't connect (probably because server name was invalid)" << endl;
        }
        freeaddrinfo(servInfo); //free the address
    }
    if (!exited)
    {
        //now that we've gotten this far...
        //store info into thread_socket structure
        pts = new thread_socket();
        pts->socket = sockfd;
        pts->thread_id = 0;
        //we need to get client address info
        sockaddr_storage client_address;
        socklen_t sin_size = sizeof(sockaddr_storage);
        if ((getsockname(pts->socket, (sockaddr*)&client_address, &sin_size)) != 0)
            cout << "getsockname error:  " << WSAGetLastError() << endl;
        sockaddr_in* sai_client = (sockaddr_in*)&client_address;
        IP_ADDR client_ia;
        client_ia.source_port = ntohs(sai_client->sin_port); //ntohs just to make sure
        unsigned long ia = ntohl(sai_client->sin_addr.s_addr); //ntohl just to make sure
        client_ia.source_ip = ia;
        pts->ip_port = client_ia;
        pts->sai = *sai_client;
        char ipbuffer[ipbuffer_length];
        WSAAddressToString((sockaddr*)&client_address, sizeof(client_address), NULL, ipbuffer, &ipbuffer_length);
        string ipaddress;
        ipaddress.assign(ipbuffer);
        pts->address_string = ipaddress;
        //let's check our messages!  specifically, for an SM_WELCOME message
        unsigned long mode = 0;
        ioctlsocket(pts->socket, FIONBIO, &mode); //set socket to blocking
        char* tempData = (char*)malloc(MAX_MESSAGE);
        cout << static_cast<void*>(tempData) << endl;
        int message_status = WaitForMessage(pts->socket, tempData, cm, 10000);
        if (message_status == -1)
            cout << "Server timed out." << endl;
        else if (message_status == SM_WELCOME)
            cout << "Message from Server:  " << cm.data << endl;

        //request a client list
        SendMessage(pts, CM_RETRIEVE);
        cout << "Getting list of available clients..." << endl;
        //now wait for the response
        ioctlsocket(pts->socket, FIONBIO, &mode); //set socket to blocking
        message_status = WaitForMessage(pts->socket, tempData, cm, 10000);
        if (message_status == -1)
            cout << "Server timed out." << endl;
        else if (message_status == SM_CLIENTLIST)
            cout << "Here is a list of clients:\n" << InterpretClientList(cm.data) << endl;

        //now go into the "name" loop
        if (message_status != -1)
        {
            bool success = false;
            while(!success)
            {
                string name;
                cout << "Enter your name here (no spaces):  ";
                cin >> name;

                //now send a CM_ADD message
                SendMessage(pts, CM_ADD, name.c_str());
                message_status = WaitForMessage(sockfd, tempData, cm, 20);
                switch (message_status)
                {
                    case SM_ALREADYUSED:
                        cout << "Server Message:  Host name already taken.  Pick another one." << endl;
                    break;
                    case SM_SUCCESS:
                    {
                        cout << "Server Message:  Host Successfully Added!" << endl;
                        clientName = name;
                        success = true;
                    }
                    break;
                    default:
                    {
                        string response;
                        cout << "Server timed out.  Try again? ";
                        cin >> response;
                        transform(response.begin(), response.end(), response.begin(), ::tolower);
                        if (response != "yes")
                            success = true;
                    }
                    break;
                }
            }
            //now that the host is successfully added, why don't we start sending messages now?
            if (message_status != -1)
            {
                cout << "Now you can start sending messages to all hosts you're connected to." << endl;
                cout << "To see what hosts you're connected to, request the list of available hosts at any time." << endl;
                cout << "Again, type '?' for a list of available commands." << endl;
                //start a receiving thread
                _beginthread(InputLoop, 0, (void*)pts);
                //now start the listening loop
                unsigned long iMode = 1;
                ioctlsocket(sockfd, FIONBIO, &iMode); //set socket to non-blocking
                while (!lldone && !forceQuit)
                {
                    if (WaitForMessage(sockfd, tempData, cm, 0) != -1)
                        //get data
                        HandleMessage(cm);
                }
                if (!forceQuit)
                    ProperClose(pts->socket, tempData);
                else
                    closesocket(pts->socket); //what else can you do when the other side force-quitted?
            }
        }
        free(tempData);
    }
    delete pts;
    free(data);
    WSACleanup();
    return 0;
}
