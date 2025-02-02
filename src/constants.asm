; Copyright (C) 2018-2023 Heinrich-Heine-Universitaet Duesseldorf,
; Institute of Computer Science, Department Operating Systems
; Burak Akguel, Christian Gesse, Fabian Ruhland, Filip Krakowski, Michael Schoettner; Olaf Spinczyk, TU Dortmund
;
; This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public
; License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any
; later version.
;
; This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied
; warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
; details.
;
; You should have received a copy of the GNU General Public License
; along with this program.  If not, see <http://www.gnu.org/licenses/>

%ifndef CONSTANTS_ASM
%define CONSTANTS_ASM

%define KERNEL_START        0xc0000000
%define STACK_SIZE          0x00001000
%define MULTIBOOT_SIZE      0x00001000
%define ACPI_SIZE           0x00010000

%macro  call_physical_function 1
    mov eax, %1
    sub eax, KERNEL_START
    call eax
%endmacro

%endif
