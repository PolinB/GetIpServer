#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <zconf.h>
#include <vector>
#include <poll.h>
#include "Server.h"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-noreturn"
int main() {
    Server server(1235);
    server.start();
    // не обрабатываются ошибки, erase за квадрат и многое другое (олимпиадное программирование)
    // poll работает за линейное время
    // epoll тоже файл-дескриптор, хочу/не хочу ждать этого чувака, а готов ли он
    // accept можно вызывать из разных потоков
    // очередь внутри сокета одна, так что многопоточка не оч многопоточна)
    // можно несколько сокетов на один порт, можно распараллеливать уже после accept
    // сделать свои обертки
    // и читать и писать poll { , POLLIN | POLLOUT, }
    return 0;
}
#pragma clang diagnostic pop