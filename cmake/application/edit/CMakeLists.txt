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
        ${HHUOS_SRC_DIR}/application/edit/Edit.cpp
        ${HHUOS_SRC_DIR}/application/edit/FileBuffer.cpp
        ${HHUOS_SRC_DIR}/application/edit/CursorBuffer.cpp
        ${HHUOS_SRC_DIR}/application/edit/event/EditEvent.cpp
        ${HHUOS_SRC_DIR}/application/edit/event/InsertCharEvent.cpp
        ${HHUOS_SRC_DIR}/application/edit/event/DeleteCharEvent.cpp
        # ${HHUOS_SRC_DIR}/application/edit/event/InsertStringEvent.cpp
        # ${HHUOS_SRC_DIR}/application/edit/event/DeleteStringEvent.cpp
        ${HHUOS_SRC_DIR}/application/edit/userinterface/Compositor.cpp
        ${HHUOS_SRC_DIR}/application/edit/userinterface/component/Component.cpp
        ${HHUOS_SRC_DIR}/application/edit/userinterface/component/TextView.cpp
        )

add_executable(${PROJECT_NAME} ${SOURCE_FILES})

target_link_libraries(${PROJECT_NAME} lib.user.crt0 lib.user.base lib.user.graphic)