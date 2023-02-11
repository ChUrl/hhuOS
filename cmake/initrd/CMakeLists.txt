# Copyright (C) 2018-2023 Heinrich-Heine-Universitaet Duesseldorf,
# Institute of Computer Science, Department Operating Systems
# Burak Akguel, Christian Gesse, Fabian Ruhland, Filip Krakowski, Michael Schoettner
#
#
# This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public
# License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any
# later version.
#
# This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied
# warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
# details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>

cmake_minimum_required(VERSION 3.14)

project(initrd)
message(STATUS "Project " ${PROJECT_NAME})

if ($ENV{HHUOS_MINIMAL_INITRD})
    add_custom_command(OUTPUT "${CMAKE_BINARY_DIR}/hhuOS.initrd"
            COMMAND /run/current-system/sw/bin/mkdir -p "${HHUOS_ROOT_DIR}/initrd/bin"
            COMMAND /run/current-system/sw/bin/cp "$<TARGET_FILE:shell>" "${HHUOS_ROOT_DIR}/initrd/bin/shell"
            COMMAND /run/current-system/sw/bin/tar -C "${HHUOS_ROOT_DIR}/initrd/" --xform s:'./':: -cf "${CMAKE_BINARY_DIR}/hhuOS.initrd" ./
            COMMAND /run/current-system/sw/bin/rm -f "${HHUOS_ROOT_DIR}/hhuOS.img" "${HHUOS_ROOT_DIR}/hhuOS.iso"
            DEPENDS shell)

    add_custom_target(${PROJECT_NAME} DEPENDS shell "${CMAKE_BINARY_DIR}/hhuOS.initrd")
else()
    add_custom_command(OUTPUT "${CMAKE_BINARY_DIR}/hhuOS.initrd"
            COMMAND /run/current-system/sw/bin/mkdir -p "${HHUOS_ROOT_DIR}/initrd/bin"
            COMMAND /run/current-system/sw/bin/cp "$<TARGET_FILE:shell>" "${HHUOS_ROOT_DIR}/initrd/bin/shell"
            COMMAND /run/current-system/sw/bin/cp "$<TARGET_FILE:ant>" "${HHUOS_ROOT_DIR}/initrd/bin/ant"
            COMMAND /run/current-system/sw/bin/cp "$<TARGET_FILE:asciimate>" "${HHUOS_ROOT_DIR}/initrd/bin/asciimate"
            COMMAND /run/current-system/sw/bin/cp "$<TARGET_FILE:beep>" "${HHUOS_ROOT_DIR}/initrd/bin/beep"
            COMMAND /run/current-system/sw/bin/cp "$<TARGET_FILE:cat>" "${HHUOS_ROOT_DIR}/initrd/bin/cat"
            COMMAND /run/current-system/sw/bin/cp "$<TARGET_FILE:color>" "${HHUOS_ROOT_DIR}/initrd/bin/color"
            COMMAND /run/current-system/sw/bin/cp "$<TARGET_FILE:cp>" "${HHUOS_ROOT_DIR}/initrd/bin/cp"
            COMMAND /run/current-system/sw/bin/cp "$<TARGET_FILE:cube>" "${HHUOS_ROOT_DIR}/initrd/bin/cube"
            COMMAND /run/current-system/sw/bin/cp "$<TARGET_FILE:date>" "${HHUOS_ROOT_DIR}/initrd/bin/date"
            COMMAND /run/current-system/sw/bin/cp "$<TARGET_FILE:dino>" "${HHUOS_ROOT_DIR}/initrd/bin/dino"
            COMMAND /run/current-system/sw/bin/cp "$<TARGET_FILE:echo>" "${HHUOS_ROOT_DIR}/initrd/bin/echo"
            COMMAND /run/current-system/sw/bin/cp "$<TARGET_FILE:head>" "${HHUOS_ROOT_DIR}/initrd/bin/head"
            COMMAND /run/current-system/sw/bin/cp "$<TARGET_FILE:hexdump>" "${HHUOS_ROOT_DIR}/initrd/bin/hexdump"
            COMMAND /run/current-system/sw/bin/cp "$<TARGET_FILE:ip>" "${HHUOS_ROOT_DIR}/initrd/bin/ip"
            COMMAND /run/current-system/sw/bin/cp "$<TARGET_FILE:kill>" "${HHUOS_ROOT_DIR}/initrd/bin/kill"
            COMMAND /run/current-system/sw/bin/cp "$<TARGET_FILE:ls>" "${HHUOS_ROOT_DIR}/initrd/bin/ls"
            COMMAND /run/current-system/sw/bin/cp "$<TARGET_FILE:lvgl_demo>" "${HHUOS_ROOT_DIR}/initrd/bin/lvgl"
            COMMAND /run/current-system/sw/bin/cp "$<TARGET_FILE:membench>" "${HHUOS_ROOT_DIR}/initrd/bin/membench"
            COMMAND /run/current-system/sw/bin/cp "$<TARGET_FILE:mkdir>" "${HHUOS_ROOT_DIR}/initrd/bin/mkdir"
            COMMAND /run/current-system/sw/bin/cp "$<TARGET_FILE:mount>" "${HHUOS_ROOT_DIR}/initrd/bin/mount"
            COMMAND /run/current-system/sw/bin/cp "$<TARGET_FILE:mouse>" "${HHUOS_ROOT_DIR}/initrd/bin/mouse"
            COMMAND /run/current-system/sw/bin/cp "$<TARGET_FILE:ping>" "${HHUOS_ROOT_DIR}/initrd/bin/ping"
            COMMAND /run/current-system/sw/bin/cp "$<TARGET_FILE:polygon>" "${HHUOS_ROOT_DIR}/initrd/bin/polygon"
            COMMAND /run/current-system/sw/bin/cp "$<TARGET_FILE:ps>" "${HHUOS_ROOT_DIR}/initrd/bin/ps"
            COMMAND /run/current-system/sw/bin/cp "$<TARGET_FILE:pwd>" "${HHUOS_ROOT_DIR}/initrd/bin/pwd"
            COMMAND /run/current-system/sw/bin/cp "$<TARGET_FILE:rm>" "${HHUOS_ROOT_DIR}/initrd/bin/rm"
            COMMAND /run/current-system/sw/bin/cp "$<TARGET_FILE:rmdir>" "${HHUOS_ROOT_DIR}/initrd/bin/rmdir"
            COMMAND /run/current-system/sw/bin/cp "$<TARGET_FILE:shutdown>" "${HHUOS_ROOT_DIR}/initrd/bin/shutdown"
            COMMAND /run/current-system/sw/bin/cp "$<TARGET_FILE:touch>" "${HHUOS_ROOT_DIR}/initrd/bin/touch"
            COMMAND /run/current-system/sw/bin/cp "$<TARGET_FILE:tree>" "${HHUOS_ROOT_DIR}/initrd/bin/tree"
            COMMAND /run/current-system/sw/bin/cp "$<TARGET_FILE:uecho>" "${HHUOS_ROOT_DIR}/initrd/bin/uecho"
            COMMAND /run/current-system/sw/bin/cp "$<TARGET_FILE:unmount>" "${HHUOS_ROOT_DIR}/initrd/bin/unmount"
            COMMAND /run/current-system/sw/bin/cp "$<TARGET_FILE:uptime>" "${HHUOS_ROOT_DIR}/initrd/bin/uptime"
            COMMAND /run/current-system/sw/bin/cp -r "${CMAKE_BINARY_DIR}/beep" "${HHUOS_ROOT_DIR}/initrd"
            COMMAND /run/current-system/sw/bin/cp -r "${CMAKE_BINARY_DIR}/asciimation" "${HHUOS_ROOT_DIR}/initrd"
            COMMAND /run/current-system/sw/bin/tar -C "${HHUOS_ROOT_DIR}/initrd/" --xform s:'./':: -cf "${CMAKE_BINARY_DIR}/hhuOS.initrd" ./
            COMMAND /run/current-system/sw/bin/rm -f "${HHUOS_ROOT_DIR}/hhuOS.img" "${HHUOS_ROOT_DIR}/hhuOS.iso"
            DEPENDS asciimation music shell ant asciimate beep cat color cp cube date dino echo head hexdump ip kill ls lvgl_demo membench mkdir mount mouse ping polygon ps pwd rm rmdir shutdown touch tree uecho unmount uptime)

    add_custom_target(${PROJECT_NAME} DEPENDS music asciimation shell ant asciimate beep cat color cp cube date dino echo head hexdump ip kill ls lvgl_demo membench mkdir mount mouse ping polygon ps pwd rm rmdir shutdown touch tree uecho unmount uptime "${CMAKE_BINARY_DIR}/hhuOS.initrd")
endif()