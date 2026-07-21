cmake_minimum_required(VERSION 3.16)
project(VisionGlove_by_Grok LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(visionglove
    src/logger.cpp
    src/config.cpp
    src/security.cpp
    src/serial_stub.cpp
    src/sensors.cpp
    src/vision.cpp
    src/haptics.cpp
    src/comms.cpp
    src/glove_system.cpp
    src/main.cpp
)

target_include_directories(visionglove PRIVATE include)

if (MSVC)
    target_compile_options(visionglove PRIVATE /W4)
else()
    target_compile_options(visionglove PRIVATE -Wall -Wextra -O2)
    find_package(Threads REQUIRED)
    target_link_libraries(visionglove PRIVATE Threads::Threads)
endif()

# Copy default config next to binary for convenience
add_custom_command(TARGET visionglove POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:visionglove>/config
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_SOURCE_DIR}/config/config.json
            $<TARGET_FILE_DIR:visionglove>/config/config.json
)
