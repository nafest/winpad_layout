cmake_minimum_required(VERSION 3.5)

add_definitions(-D_UNICODE)

project(winpad_layout LANGUAGES CXX)

add_executable(winpad_layout WIN32 main.cpp resource.rc)