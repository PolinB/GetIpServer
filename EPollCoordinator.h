//
// Created by polinb on 27.12.2019.
//

#ifndef GETIPSERVER_EPOLLCOORDINATOR_H
#define GETIPSERVER_EPOLLCOORDINATOR_H

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

#include "Server.h"

class EPollCoordinator {
public:
    explicit EPollCoordinator(int port);
    ~EPollCoordinator();
    void start();
private:
    void addSocketDescriptor(int socketDescriptor) const;
    void addSignalFd();
    static int set_nonblock(int fd);

    static const int SOCKET_ERROR = -1;
    static const int MAX_EVENTS_SIZE = 32;
    int ePoll;
    int signalFd;
    Server server;
};


#endif //GETIPSERVER_EPOLLCOORDINATOR_H
