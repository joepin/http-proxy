cmake_minimum_required(VERSION 3.12)
project(2_http)

set(CMAKE_CXX_STANDARD 14)

include_directories(.)
include_directories(tiny)

add_executable(2_http
        tiny/cgi-bin/adder.c
        tiny/csapp.c
        tiny/csapp.h
        tiny/godzilla.gif
        tiny/godzilla.jpg
        tiny/home.html
        tiny/Icon\r
        tiny/Makefile
        tiny/README
        tiny/tiny.c
        csapp.c
        csapp.h
        csapp.o
        driver.sh
        free-port.sh
        Makefile
        nop-server.py
        port-for-user.pl
        proxy
        proxy.c
        proxy.o
        README)
