//
// Created by polinb on 09.11.2019.
//

#include "Server.h"
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

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-noreturn"

Server::Server(int port) : queueSize(0), stop(false) {
    for (int i = 0; i < THREAD_NUMBER; ++i) {
        threads.emplace_back([this] {
            while (true) {
                std::unique_lock<std::mutex> lg(m);
                    while (queueSize.fetch_sub(1) <= 0 && !stop) {
                        std::cout << "In while" << std::endl;
                        queueSize += 1;
                        hasClientInQueue.wait(lg);
                    }
                    if (stop) {
                        break;
                    }
                    int clientFd = workQueue.front();
                    workQueue.pop();
                    // client was deleted
                    if (clients.find(clientFd) == clients.end()) {
                        continue;
                    }
                    Client &client = clients[clientFd];
                    std::string request = client.getTask();
                lg.unlock();

                if (stop) {
                    break;
                }
                doTask(clientFd, request);
                if (stop) {
                    break;
                }

                lg.lock();
                    // client was deleted
                    if (clients.find(clientFd) == clients.end()) {
                        continue;
                    }
                    if (!client.tasks.empty()) {
                        workQueue.push(clientFd);
                        queueSize += 1;
                        hasClientInQueue.notify_one();
                    }
                lg.unlock();
            }
        });
    }
    socketDescriptor = socket(AF_INET, SOCK_STREAM, 0);
    if (socketDescriptor == SOCKET_ERROR) {
        throw ServerException("Socket not created.");
    }

    sockaddr_in local_addr{
            .sin_family = AF_INET,
            .sin_port = htons(port),
            .sin_addr = {.s_addr = 0}
    };

    int b = bind(socketDescriptor, reinterpret_cast<sockaddr const *>(&local_addr), sizeof local_addr);
    if (b == SOCKET_ERROR) {
        throw ServerException("Socket not binded.");
    }

    set_nonblock(socketDescriptor);
}

void Server::start() {
    if (listen(socketDescriptor, SOMAXCONN) == SOCKET_ERROR) {
        throw ServerException("Socket not listen.");
    }

    listeningWithEpoll();
}

Server::~Server() {
    std::cout << "Hello from begin of destructor" << std::endl;
    std::unique_lock<std::mutex> lg(m);
    stop = true;
    hasClientInQueue.notify_all();
    for (size_t i = 0; i < THREAD_NUMBER; ++i) {
        threads[i].join();
    }
    std::cout << "Hello from after join of destructor" << std::endl;
    for (const auto& p : clients) {
        int clientFd = p.first;
        if (close(clientFd) < 0) {
            std::cerr << "Descriptor was not closed : " << clientFd << "." << std::endl;
        } else {
            std::cout << "Descriptor was closed : " << clientFd << "." << std::endl;
        }
    }
}

int Server::set_nonblock(int fd) {
    int flags;
#if defined(O_NONBLOCK)
    if (-1 == (flags = fcntl(fd, F_GETFL, 0))) {
        flags = 0;
    }
    return fcntl(fd, F_GETFL, flags | O_NONBLOCK);
#else
    flags = 1;
    return ioctl(fd, FIONBIO, &flags);
#endif
}

void Server::listeningWithEpoll() {
    int ePoll = epoll_create1(0);
    if (ePoll == SOCKET_ERROR) {
        throw ServerException("Epoll not create.");
    }

    addSocketDescriptor(ePoll);

    int signalFd = addSignalFd(ePoll);

    while (true) {
        epoll_event events[MAX_EVENTS_SIZE];
        int nfds = epoll_wait(ePoll, events, MAX_EVENTS_SIZE, -1);
        if (nfds == SOCKET_ERROR) {
            throw ServerException("Don't wait epoll.");
        }

        for (size_t i = 0; i < nfds; ++i) {
            if (events[i].data.fd == socketDescriptor) {
                int client = accept(socketDescriptor, nullptr, nullptr);
                if (client < 0) {
                    throw ServerException("Connection failed.");
                }

                set_nonblock(client);

                epoll_event event{
                        .events = EPOLLIN,
                        .data = {.fd = client}
                };

                if (epoll_ctl(ePoll, EPOLL_CTL_ADD, client, &event) == SOCKET_ERROR) {
                    if (close(client) < 0) {
                        throw ServerException("Descriptor was not closed.");
                    }
                    throw ServerException("Failed to register.");
                }

                std::unique_lock<std::mutex> lg(m);
                std::cout << "WANT LOCK" << std::endl;
                clients[client] = Client(client);
                lg.unlock();
            } else if (events[i].data.fd == signalFd) {
                std::cerr << "Hello from exit" << std::endl;
                exit(0);
            } else {
                char buf[1024];

                std::unique_lock<std::mutex> lg(m);
                lg.unlock();

                int clientFd = events[i].data.fd;

                int r = recv(clientFd, buf, sizeof(buf), 0);
                if ((r <= 0 && errno != EAGAIN) || strncmp(buf, "exit", 4) == 0) {
                    lg.lock();
                        shutdown(clientFd, SHUT_RDWR);
                        close(clientFd);
                        clients.erase(clientFd);
                    lg.unlock();
                    continue;
                }

                std::string request(buf, r - 2);

                lg.lock();
                    Client &curClient = clients[clientFd];
                    curClient.addTask(request);
                    if (inQueue.find(clientFd) == inQueue.end()) {
                        workQueue.push(clientFd);
                        queueSize += 1;
                        hasClientInQueue.notify_one();
                    }
                lg.unlock();
            }
        }
    }
}

#pragma clang diagnostic pop

void Server::doTask(int clientFd, const std::string &request) {
    std::vector<std::string> ips = getIps(request);

    std::cout << "Request: " << request << std::endl;

    for (auto const &ip : ips) {
        int res = send(clientFd, ip.c_str(), ip.length(), 0);
        if (res < 0) {
            std::cout << "Could not send data to client " << clientFd << std::endl;
        }
    }
}

std::vector<std::string> Server::getIps(const std::string &request) const {
    addrinfo hints{
            .ai_flags = AI_PASSIVE,
            .ai_family = AF_UNSPEC,
            .ai_socktype = SOCK_STREAM
    };
    memset(&hints, 0, sizeof(addrinfo));
    addrinfo *result(nullptr);

    int errcode = getaddrinfo(request.c_str(), "http", &hints, &result);
    if (errcode != 0) {
        std::string errorMessage(gai_strerror(errcode));
        freeaddrinfo(result);
        return {"Error with website " + request + " : " + errorMessage + ".\n"};
    }

    std::vector<std::string> ips;
    ips.emplace_back("IP addresses for " + request + ":\n");
    for (auto p = result; p != nullptr; p = p->ai_next) {
        char buf[1024];
        inet_ntop(p->ai_family, &(reinterpret_cast<sockaddr_in *>(p->ai_addr)->sin_addr), buf, sizeof(buf));
        std::string address(buf);
        address.append("\n");
        ips.emplace_back(address);
    }

    freeaddrinfo(result);

    return ips;
}

void Server::addSocketDescriptor(int ePoll) const {
    epoll_event event{
            .events = EPOLLIN,
            .data = {.fd = socketDescriptor}
    };

    int ep = epoll_ctl(ePoll, EPOLL_CTL_ADD, socketDescriptor, &event);
    if (ep == SOCKET_ERROR) {
        throw ServerException("Epoll_ctl.");
    }
}

int Server::addSignalFd(int ePoll) const {
    int signalFd;
    sigset_t all;
    sigfillset(&all);

    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGTERM);
    sigaddset(&mask, SIGINT);

    if (sigprocmask(SIG_SETMASK, &all, nullptr) == SOCKET_ERROR) {
        throw ServerException("All signals not set ignored.");
    }
    if (sigprocmask(SIG_BLOCK, &mask, nullptr) == SOCKET_ERROR) {
        throw ServerException("SIGINT and SIGTERM not set.");
    }

    signalFd = signalfd(-1, &mask, 0);

    if (signalFd == SOCKET_ERROR) {
        throw ServerException("Signal not create.");
    }

    epoll_event event{
            .events = EPOLLIN | EPOLLET,
            .data = {.fd = signalFd}
    };
    int result = epoll_ctl(ePoll, EPOLL_CTL_ADD, signalFd, &event);
    if (result == SOCKET_ERROR) {
        throw ServerException("Failed to register signalFd.");
    }
    return signalFd;
}

Server::Client::Client(int fd) : fd(fd) {}

void Server::Client::addTask(const std::string &task) {
    tasks.push(task);
}

std::string Server::Client::getTask() {
    std::string result = tasks.front();
    tasks.pop();
    return result;
}

Server::Client::Client() {
    fd = 0;
}
