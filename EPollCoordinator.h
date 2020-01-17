//
// Created by polinb on 27.12.2019.
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
#include <unordered_set>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <iostream>
#include <cstring>
#include <sys/signalfd.h>
#include <stdexcept>
#include <unistd.h>
#include <csignal>
#include <functional>
#include <map>

#include "ServerException.h"

class EPollCoordinator {
public:
    EPollCoordinator();
    ~EPollCoordinator();
    void start();
    void addDescriptor(int descriptor, std::function<void()>);
    int ePoll;
private:
    void addSignalFd();
    static const int SOCKET_ERROR = -1;
    static const int MAX_EVENTS_SIZE = 32;
    int signalFd;
    std::unordered_map<int, std::function<void()>> functions;
};

