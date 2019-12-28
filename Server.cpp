//
// Created by polinb on 09.11.2019.
//

#include "Server.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <iostream>
#include <cstring>
#include <stdexcept>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

Server::Server(int port) : queueSize(0), stop(false) {
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
}

void Server::start() {
    for (int i = 0; i < THREAD_NUMBER; ++i) {
        threads.emplace_back([this] {
            while (true) {
                std::unique_lock<std::mutex> lg(m);
                while (queueSize.fetch_sub(1) <= 0 && !stop) {
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

    if (listen(socketDescriptor, SOMAXCONN) == SOCKET_ERROR) {
        throw ServerException("Socket not listen.");
    }
}

Server::~Server() {
    std::unique_lock<std::mutex> lg(m);
    stop = true;
    hasClientInQueue.notify_all();
    for (size_t i = 0; i < THREAD_NUMBER; ++i) {
        threads[i].join();
    }
    for (const auto& p : clients) {
        int clientFd = p.first;
        if (close(clientFd) < 0) {
            std::cerr << "Descriptor was not closed : " << clientFd << "." << std::endl;
        }
    }
}

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
    ips.emplace_back("IP addresses " + request + ":\n");
    for (auto p = result; p != nullptr; p = p->ai_next) {
        char buf[1024];
        inet_ntop (p->ai_family, p->ai_addr->sa_data, buf, sizeof(buf));

        void *ptr = nullptr;
        switch (p->ai_family)
        {
            case AF_INET:
                ptr = &(reinterpret_cast<sockaddr_in *>(p->ai_addr))->sin_addr;
                break;
            case AF_INET6:
                ptr = &(reinterpret_cast<sockaddr_in6 *>(p->ai_addr))->sin6_addr;
                break;
        }
        inet_ntop (p->ai_family, ptr, buf, sizeof(buf));

        std::string address(buf);
        address.append("\n");
        ips.emplace_back(address);
    }

    freeaddrinfo(result);

    return ips;
}

int Server::getSocketDescriptor() {
    return socketDescriptor;
}

void Server::addClient(int client) {
    std::unique_lock<std::mutex> lg(m);
    clients[client] = Client(client);
    lg.unlock();
}

void Server::eraseClient(int client) {
    std::unique_lock<std::mutex> lg(m);
    shutdown(client, SHUT_RDWR);
    close(client);
    clients.erase(client);
    lg.unlock();
}

void Server::addTask(int client, const std::string& request) {
    std::unique_lock<std::mutex> lg(m);
    Client &curClient = clients[client];
    curClient.addTask(request);
    if (inQueue.find(client) == inQueue.end()) {
        workQueue.push(client);
        queueSize += 1;
        hasClientInQueue.notify_one();
    }
    lg.unlock();
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
