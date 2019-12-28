#include "EPollCoordinator.h"

int main() {
    try {
        EPollCoordinator ePoll(1234);
        ePoll.start();
    } catch (ServerException& e) {
        std::cerr << e.what();
    }
    return 0;
}