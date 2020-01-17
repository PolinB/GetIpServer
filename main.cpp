#include "EPollCoordinator.h"
#include "Server.h"

int main() {
    try {
        EPollCoordinator ePoll;
        Server server(1234, ePoll);
        Server server1(12345, ePoll);
        ePoll.start();
    } catch (ServerException& e) {
        std::cerr << e.what();
    }
    return 0;
}