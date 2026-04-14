cmake_minimum_required(VERSION 3.20)
project(canteen_backend CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Drogon CONFIG REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(hiredis CONFIG REQUIRED)
find_package(amqpcpp CONFIG REQUIRED)

# hiredis_ssl: try vcpkg target first, fall back to system lib
if(TARGET hiredis::hiredis_ssl)
    set(HIREDIS_SSL_LIB hiredis::hiredis_ssl)
elseif(TARGET hiredis_ssl)
    set(HIREDIS_SSL_LIB hiredis_ssl)
else()
    find_library(HIREDIS_SSL_LIB_PATH hiredis_ssl)
    if(HIREDIS_SSL_LIB_PATH)
        set(HIREDIS_SSL_LIB ${HIREDIS_SSL_LIB_PATH})
    else()
        message(WARNING "hiredis_ssl not found, TLS disabled")
        set(HIREDIS_SSL_LIB "")
    endif()
endif()

file(GLOB_RECURSE SOURCES
    "main.cc"
    "controllers/*.cc"
    "filters/*.cc"
    "plugins/*.cc"
)

add_executable(${PROJECT_NAME} ${SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    Drogon::Drogon
    OpenSSL::SSL
    OpenSSL::Crypto
    hiredis::hiredis
    ${HIREDIS_SSL_LIB}
    amqpcpp
    ev
)
