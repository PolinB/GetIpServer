cmake_minimum_required(VERSION 3.15)
project(GetIpServer)

set(CMAKE_CXX_STANDARD 20)

SET(CMAKE_CXX_FLAGS "-pthread ${CMAKE_CXX_FLAGS}")

add_executable(GetIpServer main.cpp Server.cpp Server.h ServerException.cpp ServerException.h EPollCoordinator.cpp EPollCoordinator.h)