//
// Created by polinb on 09.11.2019.
//

#include "ServerException.h"

#include <utility>

ServerException::ServerException(const std::string&  message) noexcept : message(message){}

ServerException::ServerException(std::string &&message) noexcept : message(message){}

ServerException::ServerException(const ServerException &parserException) noexcept : message(parserException.message){}

const char *ServerException::what() const noexcept {
    return message.c_str();
}




