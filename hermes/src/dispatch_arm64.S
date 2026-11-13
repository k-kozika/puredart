.text
.hidden ph_dispatch
// Save the complete interpreter register state; x8 is its dispatch scratch.
.macro PH_RET index
.p2align 2
.global ph_ret_\index
.hidden ph_ret_\index
.type ph_ret_\index, %function
ph_ret_\index:
.cfi_startproc
bti jc
sub sp, sp, #800
.cfi_def_cfa_offset 800
stp x0, x1, [sp, #0]
stp x2, x3, [sp, #16]
stp x4, x5, [sp, #32]
stp x6, x7, [sp, #48]
stp x8, x9, [sp, #64]
stp x10, x11, [sp, #80]
stp x12, x13, [sp, #96]
stp x14, x15, [sp, #112]
stp x16, x17, [sp, #128]
stp x18, x19, [sp, #144]
stp x20, x21, [sp, #160]
stp x22, x23, [sp, #176]
stp x24, x25, [sp, #192]
stp x26, x27, [sp, #208]
stp x28, x29, [sp, #224]
str x30, [sp, #240]
.cfi_offset 30, -560
.cfi_offset 19, -648
.cfi_offset 20, -640
.cfi_offset 21, -632
.cfi_offset 22, -624
.cfi_offset 23, -616
.cfi_offset 24, -608
.cfi_offset 25, -600
.cfi_offset 26, -592
.cfi_offset 27, -584
.cfi_offset 28, -576
.cfi_offset 29, -568
.cfi_offset 72, -416
.cfi_offset 73, -400
.cfi_offset 74, -384
.cfi_offset 75, -368
.cfi_offset 76, -352
.cfi_offset 77, -336
.cfi_offset 78, -320
.cfi_offset 79, -304
stp q0, q1, [sp, #256]
stp q2, q3, [sp, #288]
stp q4, q5, [sp, #320]
stp q6, q7, [sp, #352]
stp q8, q9, [sp, #384]
stp q10, q11, [sp, #416]
stp q12, q13, [sp, #448]
stp q14, q15, [sp, #480]
stp q16, q17, [sp, #512]
stp q18, q19, [sp, #544]
stp q20, q21, [sp, #576]
stp q22, q23, [sp, #608]
stp q24, q25, [sp, #640]
stp q26, q27, [sp, #672]
stp q28, q29, [sp, #704]
stp q30, q31, [sp, #736]
mrs x8, nzcv
mrs x9, fpcr
str x8, [sp, #768]
str x9, [sp, #776]
mrs x8, fpsr
str x8, [sp, #784]
mov x0, sp
mov x1, #\index
bl ph_dispatch
str x0, [sp, #64]
ldr x8, [sp, #768]
ldr x9, [sp, #776]
msr nzcv, x8
msr fpcr, x9
ldr x8, [sp, #784]
msr fpsr, x8
ldp q0, q1, [sp, #256]
ldp q2, q3, [sp, #288]
ldp q4, q5, [sp, #320]
ldp q6, q7, [sp, #352]
ldp q8, q9, [sp, #384]
ldp q10, q11, [sp, #416]
ldp q12, q13, [sp, #448]
ldp q14, q15, [sp, #480]
ldp q16, q17, [sp, #512]
ldp q18, q19, [sp, #544]
ldp q20, q21, [sp, #576]
ldp q22, q23, [sp, #608]
ldp q24, q25, [sp, #640]
ldp q26, q27, [sp, #672]
ldp q28, q29, [sp, #704]
ldp q30, q31, [sp, #736]
ldp x0, x1, [sp, #0]
ldp x2, x3, [sp, #16]
ldp x4, x5, [sp, #32]
ldp x6, x7, [sp, #48]
ldp x8, x9, [sp, #64]
ldp x10, x11, [sp, #80]
ldp x12, x13, [sp, #96]
ldp x14, x15, [sp, #112]
ldp x16, x17, [sp, #128]
ldp x18, x19, [sp, #144]
ldp x20, x21, [sp, #160]
ldp x22, x23, [sp, #176]
ldp x24, x25, [sp, #192]
ldp x26, x27, [sp, #208]
ldp x28, x29, [sp, #224]
ldr x30, [sp, #240]
add sp, sp, #800
.cfi_def_cfa_offset 0
.cfi_restore 30
.cfi_restore 19
.cfi_restore 20
.cfi_restore 21
.cfi_restore 22
.cfi_restore 23
.cfi_restore 24
.cfi_restore 25
.cfi_restore 26
.cfi_restore 27
.cfi_restore 28
.cfi_restore 29
.cfi_restore 72
.cfi_restore 73
.cfi_restore 74
.cfi_restore 75
.cfi_restore 76
.cfi_restore 77
.cfi_restore 78
.cfi_restore 79
br x8
.cfi_endproc
.size ph_ret_\index, .-ph_ret_\index
.endm
PH_RET 0
PH_RET 1
PH_RET 2
PH_RET 3
PH_RET 4
PH_RET 5
PH_RET 6
PH_RET 7
.section .note.GNU-stack,"",%progbits
