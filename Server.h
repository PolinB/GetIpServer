//
// Created by polinb on 09.11.2019.
//

#pragma once

#include <string>
#include <sys/socket.h>
#include <netinet/in.h>
#include <zconf.h>
#include <vector>
#include <poll.h>
#include <cstring>
#include <sys/ioctl.h>
#include <poll.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <stdexcept>
#include <netdb.h>
#include <arpa/inet.h>
#include <thread>
#include <cstring>
#include <iostream>
#include <csignal>
#include <zconf.h>
#include <unordered_map>
#include <queue>

#include "ServerException.h"

class Server {
public:
    explicit Server(int port);
    ~Server();
    void start();
private:
    struct Client {
        Client();
        explicit Client(int fd);
        void addTask(const std::string& task);
        std::string getTask();
        int fd;
        std::queue<std::string> tasks;
    };
    std::unordered_map<int, Client> clients;
    [[nodiscard]] std::vector<std::string> getIps(const std::string &request) const;
    void listeningWithEpoll();
    static const int SOCKET_ERROR = -1;
    static const int MAX_EVENTS_SIZE = 32;
    static int set_nonblock(int fd);
    int socketDescriptor;
};
