.equ __NR_exit, 60
.equ RTLD_NOW, 2

.section .text
.global _start
.type _start, @function
.extern dlopen
.extern dlsym
.extern dlclose
_start:
    lea libm_name(%rip), %rdi
    mov $RTLD_NOW, %esi
    call dlopen@PLT
    test %rax, %rax
    jz fail_dlopen
    mov %rax, %rbx

    mov %rbx, %rdi
    lea cos_name(%rip), %rsi
    call dlsym@PLT
    test %rax, %rax
    jz fail_dlsym

    mov %rbx, %rdi
    call dlclose@PLT
    test %eax, %eax
    jnz fail_dlclose

    xor %edi, %edi
    jmp exit_now

fail_dlopen:
    mov $1, %edi
    jmp exit_now
fail_dlsym:
    mov $2, %edi
    jmp exit_now
fail_dlclose:
    mov $3, %edi

exit_now:
    mov $__NR_exit, %eax
    syscall
.size _start, .-_start

.section .rodata
libm_name:
    .asciz "libm.so.6"
cos_name:
    .asciz "cos"

.section .note.GNU-stack,"",@progbits
