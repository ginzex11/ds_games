cmake_minimum_required(VERSION 3.15)
project(AISimulationGame VERSION 1.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find OpenGL
find_package(OpenGL REQUIRED)

# FreeGLUT paths (adjust if needed)
set(FREEGLUT_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/../third_party/freeglut/include")
set(FREEGLUT_LIBRARY_DIR "${CMAKE_SOURCE_DIR}/../third_party/freeglut/lib")

# Include directories
include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${FREEGLUT_INCLUDE_DIR}
    ${OPENGL_INCLUDE_DIR}
)

# Link directories
link_directories(${FREEGLUT_LIBRARY_DIR})

# Source files
set(SOURCES
    src/main.cpp
    src/map.cpp
    src/ai.cpp
    src/character.cpp
    src/commander.cpp
    src/warrior.cpp
    src/medic.cpp
    src/supplier.cpp
    src/simulation.cpp
)

# Header files
set(HEADERS
    include/common.h
    include/map.h
    include/ai.h
    include/character.h
    include/commander.h
    include/warrior.h
    include/medic.h
    include/supplier.h
    include/simulation.h
)

# Main executable
add_executable(AISimulationGame ${SOURCES} ${HEADERS})

# Link libraries
target_link_libraries(AISimulationGame
    ${OPENGL_LIBRARIES}
    freeglut
    opengl32
    glu32
)

# Test executable
add_executable(test_main
    test/tests.cpp
    src/map.cpp
    src/ai.cpp
    src/character.cpp
    src/commander.cpp
    src/warrior.cpp
    src/medic.cpp
    src/supplier.cpp
    src/simulation.cpp
)

target_link_libraries(test_main
    ${OPENGL_LIBRARIES}
    freeglut
    opengl32
    glu32
)

# Copy FreeGLUT DLL to build directory (Windows) if it exists
if(WIN32)
    if(EXISTS "${FREEGLUT_LIBRARY_DIR}/freeglut.dll")
        add_custom_command(TARGET AISimulationGame POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${FREEGLUT_LIBRARY_DIR}/freeglut.dll"
            $<TARGET_FILE_DIR:AISimulationGame>
        )
    endif()
endif()

# Set startup project for Visual Studio
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT AISimulationGame)

message(STATUS "CMake configuration complete")
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "Build Type: ${CMAKE_BUILD_TYPE}")
