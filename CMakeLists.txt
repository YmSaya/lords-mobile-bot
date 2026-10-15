cmake_minimum_required(VERSION 3.10)

project(LordsMobileBot VERSION 1.0.2 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(client
    src/main.c
    src/connection.c
    src/log.c
    src/protocol.c
    src/des.c
    src/map_point.c
    src/command.c
    src/config.c
    src/cli.c
)

target_compile_definitions(client PRIVATE
    VERSION="${PROJECT_VERSION}"
)

target_include_directories(client PRIVATE include)

if(WIN32)
    target_link_libraries(client PRIVATE ws2_32)
else()
    target_link_libraries(client PRIVATE pthread)
endif()

enable_testing()

add_executable(test_phase1
    tests/test_phase1.c
    src/connection.c
    src/log.c
    src/protocol.c
    src/des.c
    src/map_point.c
    src/command.c
    src/config.c
    src/cli.c
)

target_compile_definitions(test_phase1 PRIVATE
    VERSION="${PROJECT_VERSION}"
)

target_include_directories(test_phase1 PRIVATE include)

if(WIN32)
    target_link_libraries(test_phase1 PRIVATE ws2_32)
else()
    target_link_libraries(test_phase1 PRIVATE pthread)
endif()

add_test(NAME test_phase1 COMMAND test_phase1)

add_executable(test_phase2
    tests/test_phase2.c
    src/cli.c
    src/connection.c
    src/log.c
    src/protocol.c
    src/des.c
    src/map_point.c
    src/command.c
    src/config.c
)

target_compile_definitions(test_phase2 PRIVATE
    VERSION="${PROJECT_VERSION}"
)

target_include_directories(test_phase2 PRIVATE include)

if(WIN32)
    target_link_libraries(test_phase2 PRIVATE ws2_32)
else()
    target_link_libraries(test_phase2 PRIVATE pthread)
endif()

add_test(NAME test_phase2 COMMAND test_phase2)
