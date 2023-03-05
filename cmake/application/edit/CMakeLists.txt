#
# Created by christoph on 05.03.23.
#

cmake_minimum_required(VERSION 3.14)

project(edit)
message(STATUS "Project " ${PROJECT_NAME})

include_directories(${HHUOS_SRC_DIR})

# Set source files
set(SOURCE_FILES
        ${HHUOS_SRC_DIR}/application/edit/edit.cpp
        ${HHUOS_SRC_DIR}/application/edit/Edit.cpp)

add_executable(${PROJECT_NAME} ${SOURCE_FILES})

target_link_libraries(${PROJECT_NAME} lib.user.crt0 lib.user.base lib.user.graphic)