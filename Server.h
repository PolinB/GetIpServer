//
// Created by polinb on 09.11.2019.
//

#pragma once

#include <string>
#include <sys/socket.h>
#include <netinet/in.h>
#include <zconf.h>
#include <vector>
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
#include <stdexcept>
#include <unistd.h>
#include <utility>


#include "EPollCoordinator.h"

class Server {
public:
    Server(int port, EPollCoordinator& ePollCoordinator);
    ~Server();
    void start();
    void addClient(int client, std::function<void()>& function);
    void eraseClient(int client);
    void addTask(int client, const std::string& request);

    std::atomic<bool> stop;

private:
    struct Client {
        Client();
        explicit Client(int fd, std::function<void()>& function);

        void addTask(const std::string& task);
        std::string getTask();
        std::queue<std::string> tasks;

        int fd;
        std::function<void()> function;
        bool inQueue = false;
    };

    std::unordered_map<int, Client> clients;
    std::queue<int> workQueue;
    std::atomic<int> queueSize;

    void doTask(int clientFd, const std::string &request);
    int set_nonblock(int fd);

    [[nodiscard]] std::vector<std::string> getIps(const std::string &request) const;
    static const int SOCKET_ERROR = -1;
    static const int THREAD_NUMBER = 8;

    EPollCoordinator& ePollCoordinator;
    int serverDescriptor;
    std::function<void()> serverFunction;

    mutable std::mutex m;
    std::condition_variable hasClientInQueue;
    std::vector<std::thread> threads;
};
