//
// Created by polinb on 27.12.2019.
//

#include "EPollCoordinator.h"

#include <utility>

EPollCoordinator::EPollCoordinator() {
    ePoll = epoll_create1(0);
    if (ePoll == SOCKET_ERROR) {
        throw ServerException("Epoll not create.");
    }

    signalFd = -1;
    addSignalFd();
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

void EPollCoordinator::addDescriptor(int socketDescriptor, std::function<void()> function) {
    epoll_event event{
            .events = EPOLLIN,
            .data = {.fd = socketDescriptor}
    };

    int ep = epoll_ctl(ePoll, EPOLL_CTL_ADD, socketDescriptor, &event);
    if (ep == SOCKET_ERROR) {
        throw ServerException("Epoll_ctl failed.");
    }

    functions[socketDescriptor] = std::move(function);
}

void EPollCoordinator::start() {
    while (true) {
        epoll_event events[MAX_EVENTS_SIZE];
        int nfds = epoll_wait(ePoll, events, MAX_EVENTS_SIZE, -1);
        if (nfds == SOCKET_ERROR) {
            throw ServerException("Don't wait epoll.");
        }

        for (size_t i = 0; i < nfds; ++i) {
            if (events[i].data.fd == signalFd) {
                exit(0);
            } else {
                std::cout << "Try func " << events[i].data.fd << "\n";
                (functions[events[i].data.fd])();
            }
        }
    }
}

EPollCoordinator::~EPollCoordinator() = default;
