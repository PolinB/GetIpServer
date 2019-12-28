#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <zconf.h>
#include <vector>
#include <poll.h>
#include "Server.h"
#include "EPollCoordinator.h"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-noreturn"
int main() {
    EPollCoordinator ePoll(1234);
    ePoll.start();
    return 0;
}
#pragma clang diagnostic pop