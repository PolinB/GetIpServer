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

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-noreturn"

Server::Server(int port) {
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

Server::~Server() = default;

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

/**
 * Сигналы на прерывание:
 *
 * Хотим если приходит SIGINT или SIGTERM завершать работу listener, epoll_wait тогда бросит ошибку, но errno тогда установится в EINTR
 * SIGINT ctrl+C
 * SIGTERM
 * send NOSIGNAL
 *
 * SIGKILL - нельзя обрабатывать
 * В ответ на ошибку программы приходят сигналы:
 * SIGSEGV
 * SIGBUS
 * SIGILL - illigal obstruction
 * SIGFPE
 * SIGPIPE - историческое недоразумение: пишем в socket, у которого другой конец закрыт
 *
 *
 * Ключевое слово volatile => можем обращаться из обработчика сигналов
 * volatile bool quit = false;
 *
 * setjump, longjump не использовать
 * volatile ограничивает то, как мы можем переупорядочивать код
 *
 * write и read можно вызывать из обработчика сигналов
 * read может прочитать меньше, чем есть, если пришел сигнал
 * quit = true упадет epoll_wait
 *
 * Как закьюить?
 * 1) pipe
 *    out добавить в epoll
 *    1 байт в in
 * 2) eventfd - специальный файловый дескриптор
 *    в обработчике сигналов сделать write в eventfd
 *
 */

void Server::listeningWithEpoll() {
    int ePoll = epoll_create1(0);
    if (ePoll == SOCKET_ERROR) {
        throw ServerException("Epoll not create.");
    }

    {
        epoll_event event{
                .events = EPOLLIN,
                .data = {.fd = socketDescriptor}
        };

        int ep = epoll_ctl(ePoll, EPOLL_CTL_ADD, socketDescriptor, &event);
        if (ep == SOCKET_ERROR) {
            throw ServerException("Epool_ctl.");
        }
    }


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
                    throw ServerException("Connection failed");
                }

                set_nonblock(client);

                epoll_event event{
                        .events = EPOLLIN,
                        .data = {.fd = client}
                };

                if (epoll_ctl(ePoll, EPOLL_CTL_ADD, client, &event) == SOCKET_ERROR) {
                    if (close(client) < 0) {
                        throw ServerException("Descriptor was not closed");
                    }
                    throw ServerException("Failed to register");
                }

                clients[client] = Client(client);

            } else {
                char buf[1024];
                Client &curClient = clients[events[i].data.fd];

                int r = recv(curClient.fd, buf, sizeof(buf), 0);
                if ((r == 0 && errno != EAGAIN) || strncmp(buf, "exit", 4) == 0) {
                    shutdown(curClient.fd, SHUT_RDWR);
                    close(curClient.fd);
                    clients.erase(curClient.fd);
                    continue;
                }

                std::string request(buf, r - 2);
                curClient.addTask(request);

                // Это вынести бы в отдельный поток для обработки... и не текущий реквест обрабатывать, а первый на очереди
                // Возможно стоит сделать также очередь клиентов...
                // При этом стоит поскольку каждому клиенту нужно примерно одинаковое время давать, то стоит
                // организовывать очередь как-то в стиле есть на очереди клиенты, если у всех есть таски, то в порядке
                // поступления тасок, если нет, но добавляется, то добавлять в конец всех, если же уже выполнена таска
                // на очереди, но список ее тасок не пуст, то в конец.
                std::vector<std::string> ips = getIps(request);
                for (auto const &ip : ips) {
                    int res = send(events[i].data.fd, ip.c_str(), ip.length(), 0);
                    if (res < 0) {
                        std::cout << "Could not send data to client " << events[i].data.fd << std::endl;
                    }
                }
            }
        }
    }
}

#pragma clang diagnostic pop

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
        return {"Error with website " + request + " : " + errorMessage + "\n"};
    }

    std::vector<std::string> ans;
    ans.emplace_back("IP addresses for " + request + '\n');
    for (auto p = result; p != nullptr; p = p->ai_next) {
        char buf[1024];
        inet_ntop(p->ai_family, &(reinterpret_cast<sockaddr_in *>(p->ai_addr)->sin_addr), buf, sizeof(buf));
        std::string address(buf);
        address.append("\n");
        ans.emplace_back(address);
    }

    freeaddrinfo(result);

    return ans;
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
