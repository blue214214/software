cmake_minimum_required(VERSION 3.20)
project(canteen_backend CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find packages
find_package(Drogon CONFIG REQUIRED)
find_package(OpenSSL REQUIRED)

# jwt-cpp (header-only, via vcpkg or manual include)
find_path(JWT_CPP_INCLUDE_DIRS "jwt-cpp/jwt.h")

# hiredis
find_library(HIREDIS_LIB hiredis REQUIRED)
find_path(HIREDIS_INCLUDE hiredis/hiredis.h REQUIRED)

# AMQP-CPP
find_library(AMQPCPP_LIB amqpcpp REQUIRED)
find_path(AMQPCPP_INCLUDE amqpcpp.h REQUIRED)

# bcrypt
find_library(BCRYPT_LIB bcrypt)

# Collect sources
file(GLOB_RECURSE SOURCES
    "main.cc"
    "controllers/*.cc"
    "services/*.cc"
    "filters/*.cc"
    "plugins/*.cc"
    "utils/*.cc"
)

add_executable(${PROJECT_NAME} ${SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${JWT_CPP_INCLUDE_DIRS}
    ${HIREDIS_INCLUDE}
    ${AMQPCPP_INCLUDE}
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    Drogon::Drogon
    OpenSSL::SSL
    OpenSSL::Crypto
    ${HIREDIS_LIB}
    ${AMQPCPP_LIB}
    ev
)

if(BCRYPT_LIB)
    target_link_libraries(${PROJECT_NAME} PRIVATE ${BCRYPT_LIB})
endif()
