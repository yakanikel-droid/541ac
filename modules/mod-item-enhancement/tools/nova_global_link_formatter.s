.intel_syntax noprefix
.section .text
.global _start

.equ OLD_FORMAT, 0x00A24F04
.equ NEW_FORMAT, 0x00DD1200
.equ DEST_BUFFER, 0x00C5CF50
.equ FORMAT_CALL, 0x0076F070
.equ RETURN_ADDRESS, 0x0061E352

_start:
    mov eax, dword ptr [ebp+0x0c]
    mov edx, dword ptr [ebp-0x04]
    push eax

    mov eax, dword ptr [ebp+0x14]
    mov eax, dword ptr [eax+0x0c]
    cmp eax, 50001
    jb old_link
    cmp eax, 50010
    ja old_link

    # Enhanced link: write the marker into gem4 and append +N to link text.
    mov ecx, eax
    sub ecx, 50000
    push ecx
    lea ecx, [ebp-0x104]
    push ecx
    push edx
    push dword ptr [ebp+0x1c]
    push dword ptr [ebp+0x18]
    push eax
    mov ecx, dword ptr [ebp+0x14]
    push dword ptr [ecx+0x08]
    push dword ptr [ecx+0x04]
    push dword ptr [ecx]
    push dword ptr [ebp+0x10]
    push edi
    push ebx
    push NEW_FORMAT
    push 0x400
    push DEST_BUFFER
    call FORMAT_CALL
    add esp, 0x50
    jmp RETURN_ADDRESS

old_link:
    lea ecx, [ebp-0x104]
    push ecx
    push edx
    push dword ptr [ebp+0x1c]
    push dword ptr [ebp+0x18]
    push 0
    mov ecx, dword ptr [ebp+0x14]
    push dword ptr [ecx+0x08]
    push dword ptr [ecx+0x04]
    push dword ptr [ecx]
    push dword ptr [ebp+0x10]
    push edi
    push ebx
    push OLD_FORMAT
    push 0x400
    push DEST_BUFFER
    call FORMAT_CALL
    add esp, 0x4c
    jmp RETURN_ADDRESS
