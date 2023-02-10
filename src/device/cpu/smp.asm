; This code is adapted from the OsDev "SMP" page and SerenityOS (and the hhuOS boot.asm of course):
; https://github.com/SerenityOS/serenity/blob/master/Kernel/Arch/x86_64/Boot/ap_setup.S (visited on 10/02/23).
; This code is relocated by the OS on SMP initialization, so it has to be position independent.

; Export
global boot_ap
global boot_ap_size
global boot_ap_gdtr
global boot_ap_idtr
global boot_ap_cr0
global boot_ap_cr3
global boot_ap_cr4
global boot_ap_stacks
global boot_ap_entry

[SECTION .text]
align 8
bits 16
boot_ap:
    cli ; Disable interrupts

    ; Make a far jump to set the segment registers for real mode after code relocation.
	jmp dword 0x800:rm_segments - boot_ap ; I don't know why 0x800...
rm_segments:
    mov ax, cs
    mov ds, ax
    xor ax, ax
    mov sp, ax

    ; Enable A20 address line
    in al, 0x92
    or al, 2
    out 0x92, al

    ; Load the temporary GDT. It is located at apStartupAddress, so it will be deallocated after AP boot!
    lgdt [tmp_gdt_desc - boot_ap]

    ; Enable Protected Mode, must be executed from an identity mapped page (if paging is used)
    mov eax, cr0
    or al, 0x1 ; Set PE bit
    mov cr0, eax

    ; Far jump to protected mode
    jmp dword 0x8:boot_ap_32 - boot_ap + 0x8000 ; Sets cs (code segment register)

; =================================================================================================
; Keep these variables in the .text section, so they get copied to apStartupAddress

; https://github.com/Peekmo/nasm/blob/master/nasm32-prems/gdt.asm (visited on 10/02/23)
; https://github.com/Peekmo/nasm/blob/master/nasm32-prems/bootsect.asm (visited on 10/02/23)
align 8
tmp_gdt_start:
	dw	0x0, 0x0, 0x0, 0x0
tmp_gdt_code:
	dw 0xFFFF ; Limit
    dw 0x0000 ; Base
    db 0x0000 ; Base 23:16
    db 0b10011011
    db 0b11011111
    db 0x0000
tmp_gdt_data:
    dw 0xFFFF ; Limit
    dw 0x0000 ; Base
    db 0x0000 ; Base 23:16
    db 0b10010011
    db 0b11011111
    db 0x0000
tmp_gdt_end:

align 8
tmp_gdt_desc:
	dw tmp_gdt_end - tmp_gdt_start - 1 ; GDT size
	dd tmp_gdt_start

; The following is set by Apic::copySmpStartupCode()
align 8
boot_ap_gdtr:
    dw 0x0
    dd 0x0
align 8
boot_ap_idtr:
    dw 0x0
    dd 0x0
align 8
boot_ap_cr0:
    dw 0x0
align 8
boot_ap_cr3:
    dw 0x0
align 8
boot_ap_cr4:
    dw 0x0
align 8
boot_ap_stacks:
    dw 0x0
align 8
boot_ap_entry:
    dw 0x0

; =================================================================================================

bits 32
align 8
boot_ap_32:
    ; Setup the protected mode segments
    mov ax, 0x10
    mov ds, ax ; Data segment register
    mov es, ax ; Extra segment register
    mov ss, ax ; Stack segment register
    mov fs, ax ; General purpose segment register
    mov gs, ax ; General purpose segment register

    ; Set cr3 to BSP value (for the page directory)
    mov eax, [boot_ap_cr3 - boot_ap + 0x8000]
    mov cr3, eax

    ; Enable PAE + PSE
    ; mov eax, cr4
    ; or eax, 0x60
    ; mov cr4, eax

    ; Enable paging + page protection
    mov ecx, cr0
    or  ecx, 0x80000001
    mov cr0, ecx

    ; Load the system GDT and IDT
	lgdt [boot_ap_gdtr]
	lidt [boot_ap_idtr]

    ; Set cr0/cr4 to BSP values
    mov eax, [boot_ap_cr0 - boot_ap + 0x8000]
    mov cr0, eax
    mov eax, [boot_ap_cr4 - boot_ap + 0x8000]
    mov cr4, eax

    ; Get the local APIC's id/CPU id using CPUID
    mov eax, 0x1
    cpuid
    shr ebx, 0x18 ; >> 24
    mov edi, ebx ; Save the id

    ; Load our stack (allocated by Apic::allocateSmpStacks)
    mov ebx, [boot_ap_stacks] ; Stackpointer array
    mov esp, [ebx + edi * 0x4] ; Choose correct stack, each apStacks entry is a 4 byte pointer to a stack
    add esp, 0x1000 ; Stack starts at the bottom
    mov ebp, esp ; Update base pointer

    push edi ; smpEntry takes apicid argument
    call [boot_ap_entry] ; AP entry function

boot_ap_finish:
    jmp $ ; Should never be reached

[SECTION .data]
align 8
boot_ap_size:
    db boot_ap_finish - boot_ap + 1
