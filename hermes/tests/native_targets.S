.text
.p2align 2
.global ph_stack_target
.type ph_stack_target, %function
ph_stack_target:
    bti c
    sub sp, sp, #32
    stp x29, x30, [sp, #16]
    add x0, x0, #1
    ldp x29, x30, [sp, #16]
    add sp, sp, #32
    ret
.size ph_stack_target, .-ph_stack_target
.global ph_load_target
.type ph_load_target, %function
ph_load_target:
    bti c
    ldr x0, [x0]
    add x0, x0, #1
    ret
.size ph_load_target, .-ph_load_target
.section .note.GNU-stack,"",%progbits
