cmake_minimum_required(VERSION 3.20)

project(smart_movement LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(SMART_MOVEMENT_ENABLE_WARNINGS "Enable strict compiler warnings" ON)
option(SMART_MOVEMENT_SANITIZE "Enable sanitizers when supported" OFF)
option(SMART_MOVEMENT_BUILD_APP "Build the interactive OpenGL/GLUT application" ON)

if(MSVC)
    add_compile_options(/W4 /permissive-)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
    if(SMART_MOVEMENT_SANITIZE)
        add_compile_options(-fsanitize=address,undefined)
        add_link_options(-fsanitize=address,undefined)
    endif()
endif()

set(SMART_MOVEMENT_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(SMART_MOVEMENT_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)

file(GLOB_RECURSE SMART_MOVEMENT_CORE_SOURCES
    ${SMART_MOVEMENT_SRC_DIR}/ai/*.cpp
    ${SMART_MOVEMENT_SRC_DIR}/core/*.cpp
    ${SMART_MOVEMENT_SRC_DIR}/utils/*.cpp
)

add_library(smart_ai STATIC
    ${SMART_MOVEMENT_CORE_SOURCES}
)

target_include_directories(smart_ai
    PUBLIC
        ${SMART_MOVEMENT_INCLUDE_DIR}
)

if(SMART_MOVEMENT_BUILD_APP)
    find_package(OpenGL REQUIRED)
    find_package(GLUT)

    if(GLUT_FOUND)
        set(_glut_link_target)
        if(TARGET GLUT::GLUT)
            set(_glut_link_target GLUT::GLUT)
        else()
            if(GLUT_glut_LIBRARY_RELEASE)
                set(_glut_link_target ${GLUT_glut_LIBRARY_RELEASE})
            elseif(GLUT_LIBRARIES)
                set(_glut_link_target ${GLUT_LIBRARIES})
            else()
                message(FATAL_ERROR "GLUT was reported as found but no library path was supplied.")
            endif()
        endif()

        set(SMART_MOVEMENT_APP_SOURCES
            ${SMART_MOVEMENT_SRC_DIR}/main.cpp
            ${SMART_MOVEMENT_SRC_DIR}/app/glut_application.cpp
        )

        add_executable(smart_movement
            ${SMART_MOVEMENT_APP_SOURCES}
        )

        set(_smart_movement_link_libs
            smart_ai
            ${_glut_link_target}
        )

        list(APPEND _smart_movement_link_libs OpenGL::GL)

        if(TARGET OpenGL::GLU)
            list(APPEND _smart_movement_link_libs OpenGL::GLU)
        else()
            list(APPEND _smart_movement_link_libs glu32)
        endif()

        if(WIN32)
            list(APPEND _smart_movement_link_libs winmm gdi32 user32 imm32)
        endif()

        target_link_libraries(smart_movement
            PRIVATE
                ${_smart_movement_link_libs}
        )

        target_include_directories(smart_movement
            PRIVATE
                ${SMART_MOVEMENT_INCLUDE_DIR}
                ${GLUT_INCLUDE_DIR}
        )

        target_compile_definitions(smart_movement PRIVATE FREEGLUT_STATIC)
    else()
        message(WARNING "GLUT/OpenGL application will not be built because GLUT was not found. Set SMART_MOVEMENT_BUILD_APP=OFF to silence this warning.")
    endif()
endif()

enable_testing()

file(GLOB SMART_MOVEMENT_TEST_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp
)

add_executable(smart_ai_tests
    ${SMART_MOVEMENT_TEST_SOURCES}
)

target_link_libraries(smart_ai_tests
    PRIVATE
        smart_ai
)

target_include_directories(smart_ai_tests
    PRIVATE
        ${SMART_MOVEMENT_INCLUDE_DIR}
)

add_test(NAME smart_ai_tests COMMAND smart_ai_tests)
