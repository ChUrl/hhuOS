; This code is adapted from the OsDev "SMP" page and SerenityOS (and the hhuOS' boot.asm of course):
; https://github.com/SerenityOS/serenity/blob/master/Kernel/Arch/x86_64/Boot/ap_setup.S (visited on 10/02/23).
; This code is relocated by the OS on SMP initialization, so it has to be position independent.
; The startup is relatively simple (in comparison to the BSP's startup sequence), because we just reuse
; all the stuff already initialized for the BSP (like GDT, IDT, cr0, cr3, cr4).

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

%define startup_address 0x8000
%define stack_size 0x1000

[SECTION .text]
align 8
bits 16
boot_ap:
    cli ; Disable interrupts

    ; Enable A20 address line
    in al, 0x92
    or al, 2
    out 0x92, al

    ; Load the temporary GDT required for the far jump into protected mode.
    ; It is located in the startup memory, so it will be deallocated after AP boot!
    lgdt [tmp_gdt_desc - boot_ap + startup_address]

    ; Enable Protected Mode, must be executed from an identity mapped page (if paging is used).
    ; Our page is identity mapped at startup_address.
    mov eax, cr0
    or al, 0x1 ; Set PE bit
    mov cr0, eax

    ; Far jump to protected mode
    jmp dword 0x8:boot_ap_32 - boot_ap + startup_address ; Sets cs (code segment register)

; =================================================================================================
; Keep these variables in the .text section, so they get copied to startup_address and can be accessed
; by offsets relative to startup_address.

; The GDT is copied from the lecture "Betriebssystementwicklung":
align 8
tmp_gdt_start:
	dw	0x0, 0x0, 0x0, 0x0
tmp_gdt_code:
	dw	0xFFFF ; 4Gb - (0x100000*0x1000 = 4Gb)
	dw	0x0000 ; base address=0
	dw	0x9A00 ; code read/exec
	dw	0x00CF ; granularity=4096, 386 (+5th nibble of limit)
tmp_gdt_data:
	dw	0xFFFF ; 4Gb - (0x100000*0x1000 = 4Gb)
	dw	0x0000 ; base address=0
	dw	0x9200 ; data read/write
	dw	0x00CF ; granularity=4096, 386 (+5th nibble of limit)
tmp_gdt_end:
tmp_gdt_desc:
	dw tmp_gdt_end - tmp_gdt_start - 1 ; GDT size
	dd tmp_gdt_start - boot_ap + startup_address

; The following is set at runtime by Apic::copySmpStartupCode():
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
    mov eax, [boot_ap_cr3 - boot_ap + startup_address]
    mov cr3, eax

    ; Enable paging + page protection
    mov ecx, cr0
    or  ecx, 0x80000001
    mov cr0, ecx

    ; Load the system GDT and IDT
	lgdt [boot_ap_gdtr - boot_ap + startup_address]
	lidt [boot_ap_idtr - boot_ap + startup_address]

    ; Set cr0/cr4 to BSP values
    mov eax, [boot_ap_cr0 - boot_ap + startup_address]
    mov cr0, eax
    mov eax, [boot_ap_cr4 - boot_ap + startup_address]
    mov cr4, eax

    ; Get the local APIC's id/CPU id using CPUID
    mov eax, 0x1
    cpuid
    shr ebx, 0x18 ; >> 24
    mov edi, ebx ; Save the id

    ; Load our stack (allocated by Apic::allocateSmpStacks)
    mov ebx, [boot_ap_stacks - boot_ap + startup_address] ; Stackpointer array
    mov esp, [ebx + edi * 0x4] ; Choose correct stack, each apStacks entry is a 4 byte pointer to a stack
    add esp, stack_size ; Stack starts at the bottom
    mov ebp, esp ; Update base pointer

    ; Call our entry function
    push edi ; smpEntry takes apicid argument
    call [boot_ap_entry - boot_ap + startup_address] ; AP entry function

boot_ap_finish:
    jmp $ ; Should never be reached

[SECTION .data]
align 8
boot_ap_size:
    db boot_ap_finish - boot_ap + 1
