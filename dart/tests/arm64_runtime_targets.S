    .pushsection .note.gnu.property, "a"
    .p2align 3
    .long 4
    .long 16
    .long 5
    .asciz "GNU"
    .long 0xc0000000
    .long 4
    .long 1
    .long 0
    .popsection
    .text
    .p2align 2
    .global test_dart_target
    .type test_dart_target, %function
test_dart_target:
    bti c
    stp x29,x30,[x15,#-16]!
    mov x29,x15
    add x0,x0,x1
    ldp x29,x30,[x15],#16
    ret
    .size test_dart_target, .-test_dart_target

    // x0=output, x1/x2=arguments, d0=FP argument.
    .global test_call_dart
    .type test_call_dart, %function
test_call_dart:
    bti c
    stp x29,x30,[sp,#-16]!
    stp x19,x20,[sp,#-16]!
    stp x26,x27,[sp,#-16]!
    stp x28,x18,[sp,#-16]!
    mov x19,x0
    mov x15,sp
    sub x15,x15,#8 // exercise a Dart SP that is only 8-byte aligned
    mov x0,x1
    mov x1,x2
    mov x16,#0x1616
    mov x17,#0x1717
    mov x18,#0x1818
    mov x26,#0x2626
    mov x27,#0x2727
    mov x28,#0x2828
    mov x20,sp
    str x20,[x19,#64]
    bl test_dart_target
    str x0,[x19,#0]
    str x16,[x19,#8]
    str x17,[x19,#16]
    str x18,[x19,#24]
    stp x26,x27,[x19,#32]
    str x28,[x19,#48]
    mov x20,sp
    str x20,[x19,#56]
    str d0,[x19,#72]
    str x15,[x19,#80]
    ldp x28,x18,[sp],#16
    ldp x26,x27,[sp],#16
    ldp x19,x20,[sp],#16
    ldp x29,x30,[sp],#16
    ret
    .size test_call_dart, .-test_call_dart
    .section .note.GNU-stack,"",%progbits
