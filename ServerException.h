//
// Created by polinb on 09.11.2019.
//

#include <exception>
#include <string>

class ServerException: public std::exception {
public:
    explicit ServerException(const std::string& message) noexcept;
    explicit ServerException(std::string&& message) noexcept;
    ServerException(const ServerException& parserException) noexcept;
    [[nodiscard]] const char* what() const noexcept override;

private:
    const std::string message;
};

