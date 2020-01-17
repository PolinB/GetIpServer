//
// Created by polinb on 09.11.2019.
//

#include "Server.h"

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

Server::Server(int port, EPollCoordinator &_ePollCoordinator)
        : queueSize(0), stop(false), ePollCoordinator(_ePollCoordinator) {
    serverDescriptor = socket(AF_INET, SOCK_STREAM, 0);
    if (serverDescriptor == SOCKET_ERROR) {
        throw ServerException("Socket not created.");
    }

    sockaddr_in local_addr{
            .sin_family = AF_INET,
            .sin_port = htons(port),
            .sin_addr = {.s_addr = 0}
    };

    int b = bind(serverDescriptor, reinterpret_cast<sockaddr const *>(&local_addr), sizeof local_addr);
    if (b == SOCKET_ERROR) {
        throw ServerException("Socket not binded.");
    }

    serverFunction = [this]() {
        int client = accept(serverDescriptor, nullptr, nullptr);
        if (client < 0) {
            throw ServerException("Connection failed.");
        }
        std::cout << "New client " << client << ".\n";

        set_nonblock(client);

        std::function<void()> clientFunction([this, client]() {
            char buf[1024];
            int r = recv(client, buf, sizeof(buf), 0);
            if ((r <= 0 && errno != EAGAIN) || strncmp(buf, "exit", 4) == 0) {
                eraseClient(client);
                ePollCoordinator.eraseDescriptor(client);
                return;
            }
            std::string request(buf, r - 2);
            addTask(client, request);
        });

        addClient(client, clientFunction);
        ePollCoordinator.addDescriptor(client, &clients[client].function);
    };

    set_nonblock(serverDescriptor);

    start();

    ePollCoordinator.addDescriptor(serverDescriptor, &serverFunction);
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
                } else {
                    client.inQueue = false;
                }
                lg.unlock();
            }
        });
    }

    if (listen(serverDescriptor, SOMAXCONN) == SOCKET_ERROR) {
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
    for (const auto &p : clients) {
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
        size_t curPos = 0;
        while (curPos < ip.length()) {
            int res = send(clientFd, ip.c_str() + curPos, ip.length(), 0);
            if (res == SOCKET_ERROR) {
                std::cout << "Could not send data to client " << clientFd << std::endl;
                break;
            }
            curPos += res;
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
        inet_ntop(p->ai_family, p->ai_addr->sa_data, buf, sizeof(buf));

        void *ptr = nullptr;
        switch (p->ai_family) {
            case AF_INET:
                ptr = &(reinterpret_cast<sockaddr_in *>(p->ai_addr))->sin_addr;
                break;
            case AF_INET6:
                ptr = &(reinterpret_cast<sockaddr_in6 *>(p->ai_addr))->sin6_addr;
                break;
        }
        inet_ntop(p->ai_family, ptr, buf, sizeof(buf));

        std::string address(buf);
        address.append("\n");
        ips.emplace_back(address);
    }

    freeaddrinfo(result);

    return ips;
}

void Server::addClient(int client, std::function<void()> &function) {
    std::unique_lock<std::mutex> lg(m);
    clients[client] = Client(client, function);
    lg.unlock();
}

void Server::eraseClient(int client) {
    std::unique_lock<std::mutex> lg(m);
    shutdown(client, SHUT_RDWR);
    close(client);
    clients.erase(client);
    lg.unlock();
}

void Server::addTask(int client, const std::string &request) {
    std::unique_lock<std::mutex> lg(m);
    Client &curClient = clients[client];
    curClient.addTask(request);
    if (!curClient.inQueue) {
        workQueue.push(client);
        curClient.inQueue = true;
        queueSize += 1;
        hasClientInQueue.notify_one();
    }
    lg.unlock();
}

Server::Client::Client(int _fd, std::function<void()> &_function) : fd(_fd), function(std::move(_function)) {}

void Server::Client::addTask(const std::string &task) {
    tasks.push(task);
}

std::string Server::Client::getTask() {
    std::string result = tasks.front();
    tasks.pop();
    return result;
}

Server::Client::Client() : fd(-1) {}
