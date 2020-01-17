//
// Created by polinb on 27.12.2019.
//

#pragma once

#include <string>
#include <sys/socket.h>
#include <netinet/in.h>
#include <zconf.h>
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
#include <atomic>
#include <sys/socket.h>
#include <fcntl.h>
#include <sys/signalfd.h>
#include <stdexcept>
#include <unistd.h>
#include <csignal>
#include <functional>

#include "ServerException.h"

class EPollCoordinator {
public:
    EPollCoordinator();
    ~EPollCoordinator();

    void start();
    void addDescriptor(int descriptor, std::function<void()>*);
    void eraseDescriptor(int descriptor);
private:
    void addSignalFd();

    static const int SOCKET_ERROR = -1;
    static const int MAX_EVENTS_SIZE = 32;
    int ePoll;
    int signalFd;
    std::unordered_map<int, std::function<void()>*> functions;
};

