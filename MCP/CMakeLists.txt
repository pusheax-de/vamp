cmake_minimum_required(VERSION 3.16)

project(vampire_mcp_server LANGUAGES CXX)

add_executable(vampire_mcp_server
    src/main.cpp
)

target_compile_features(vampire_mcp_server PRIVATE cxx_std_14)

if (MSVC)
    target_compile_options(vampire_mcp_server PRIVATE /W4 /permissive-)
else()
    target_compile_options(vampire_mcp_server PRIVATE -Wall -Wextra -Wpedantic)
endif()

target_compile_definitions(vampire_mcp_server PRIVATE UNICODE _UNICODE)
