//
// Created by polinb on 27.12.2019.
//

#include "EPollCoordinator.h"

int EPollCoordinator::set_nonblock(int fd) {
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

EPollCoordinator::EPollCoordinator(int port) : server(port) {
    ePoll = epoll_create1(0);
    if (ePoll == SOCKET_ERROR) {
        throw ServerException("Epoll not create.");
    }

    //signalFd = addSignalFd();
    addSignalFd();
    set_nonblock(server.getSocketDescriptor());

    server.start();

    addSocketDescriptor(server.getSocketDescriptor());
}

void EPollCoordinator::addSignalFd() {
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
}

void EPollCoordinator::addSocketDescriptor(int socketDescriptor) const {
    epoll_event event{
            .events = EPOLLIN,
            .data = {.fd = socketDescriptor}
    };

    int ep = epoll_ctl(ePoll, EPOLL_CTL_ADD, socketDescriptor, &event);
    if (ep == SOCKET_ERROR) {
        throw ServerException("Epoll_ctl.");
    }
}

void EPollCoordinator::start() {
    while (true) {
        epoll_event events[MAX_EVENTS_SIZE];
        int nfds = epoll_wait(ePoll, events, MAX_EVENTS_SIZE, -1);
        if (nfds == SOCKET_ERROR) {
            throw ServerException("Don't wait epoll.");
        }

        for (size_t i = 0; i < nfds; ++i) {
            if (events[i].data.fd == server.getSocketDescriptor()) {
                int client = accept(server.getSocketDescriptor(), nullptr, nullptr);
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

                server.addClient(client);

            } else if (events[i].data.fd == signalFd) {
                exit(0);
            } else {
                char buf[1024];

                int clientFd = events[i].data.fd;

                int r = recv(clientFd, buf, sizeof(buf), 0);
                if ((r <= 0 && errno != EAGAIN) || strncmp(buf, "exit", 4) == 0) {
                    server.eraseClient(clientFd);
                    continue;
                }

                std::string request(buf, r - 2);

                server.addTask(clientFd, request);
            }
        }
    }
}

EPollCoordinator::~EPollCoordinator() = default;
