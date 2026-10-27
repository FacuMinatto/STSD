cmake_minimum_required(VERSION 3.10)
project(ConfiguradorTransporte VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 COMPONENTS Widgets SerialPort REQUIRED)

add_executable(ConfiguradorTransporte
    main.cpp
    mainwindow.cpp
    mainwindow.h
)

target_link_libraries(ConfiguradorTransporte PRIVATE Qt6::Widgets Qt6::SerialPort)

