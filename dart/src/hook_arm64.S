// Generic Dart AOT hook shims. Each proxy selects a slot and loads its
// native trampoline. The common body snapshots registers before entering
// C++, restores Dart's reserved registers, then either returns an overridden
// x0/q0 result or tail-calls the original function.

    .text
    .p2align 2

    // Advertise BTI compatibility for this hand-written object. The other
    // puredart objects are compiled with -mbranch-protection=standard on
    // Android ARM64, allowing a BTI-enabled final shared object to retain its
    // GNU property instead of silently dropping the protection.
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

    // Stack layout shared by the proxies and common shim. Keep these derived
    // values together so changing the save list cannot silently invalidate the
    // HookContext register loads.
    .set PUREDART_CONTEXT_SIZE, 400
    .set PUREDART_COMMON_SAVE_SIZE, 128
    .set PUREDART_PROXY_SAVE_SIZE, 16
    .set PUREDART_COMMON_FRAME_SIZE, PUREDART_CONTEXT_SIZE + PUREDART_COMMON_SAVE_SIZE
    .set PUREDART_TOTAL_FRAME_SIZE, PUREDART_COMMON_FRAME_SIZE + PUREDART_PROXY_SAVE_SIZE
    .set PUREDART_PROXY_DART_SP_OFFSET, PUREDART_COMMON_FRAME_SIZE
    .set PUREDART_PROXY_CPU_SP_OFFSET, PUREDART_PROXY_DART_SP_OFFSET + 8

    .macro PUREDART_PROXY number
    .global puredart_hook_proxy_\number
    .hidden puredart_hook_proxy_\number
    .type puredart_hook_proxy_\number, %function
puredart_hook_proxy_\number:
    .cfi_startproc
    bti c
    // The veneer preserved IP0/IP1 before using them for its absolute jump.
    // Dart uses x15 as SP, while CPU SP (CSP) may be at a different address.
    // The veneer saved IP0/IP1 on the Dart stack. Switch to an aligned native
    // stack for C++, recording both the IP frame pointer and original CSP.
    mov x16, sp
    and x17, x15, #-16
    mov sp, x17
    stp x15, x16, [sp, #-16]!
    .cfi_def_cfa_offset 16
    adrp x17, puredart_hook_originals
    add  x17, x17, :lo12:puredart_hook_originals
    ldr  x17, [x17, #(8 * \number)]
    mov  x16, #\number
    b    puredart_hook_common
    .cfi_endproc
    .size puredart_hook_proxy_\number, .-puredart_hook_proxy_\number
    .endm

    PUREDART_PROXY 0
    PUREDART_PROXY 1
    PUREDART_PROXY 2
    PUREDART_PROXY 3
    PUREDART_PROXY 4
    PUREDART_PROXY 5
    PUREDART_PROXY 6
    PUREDART_PROXY 7
    PUREDART_PROXY 8
    PUREDART_PROXY 9
    PUREDART_PROXY 10
    PUREDART_PROXY 11
    PUREDART_PROXY 12
    PUREDART_PROXY 13
    PUREDART_PROXY 14
    PUREDART_PROXY 15

    .hidden puredart_hook_common
    .type puredart_hook_common, %function
puredart_hook_common:
    .cfi_startproc
    // The proxy switches to an aligned native stack and saves a 16-byte
    // adapter frame before tail-branching here.
    .cfi_def_cfa_offset 16
    stp x29, x30, [sp, #-16]!
    .cfi_def_cfa_offset 32
    .cfi_offset x29, -32
    .cfi_offset x30, -24
    stp x27, x28, [sp, #-16]!
    .cfi_def_cfa_offset 48
    .cfi_offset x27, -48
    .cfi_offset x28, -40
    stp x25, x26, [sp, #-16]!
    .cfi_def_cfa_offset 64
    .cfi_offset x25, -64
    .cfi_offset x26, -56
    stp x23, x24, [sp, #-16]!
    .cfi_def_cfa_offset 80
    .cfi_offset x23, -80
    .cfi_offset x24, -72
    stp x21, x22, [sp, #-16]!
    .cfi_def_cfa_offset 96
    .cfi_offset x21, -96
    .cfi_offset x22, -88
    stp x19, x20, [sp, #-16]!
    .cfi_def_cfa_offset 112
    .cfi_offset x19, -112
    .cfi_offset x20, -104
    stp x15, x17, [sp, #-16]!
    .cfi_def_cfa_offset 128
    stp x16, x18, [sp, #-16]!
    .cfi_def_cfa_offset 144
    .cfi_offset x18, -136
    // HookContext includes q0..q7 at offset 272. Floating-point Dart arguments
    // are caller-saved by AAPCS64 and must survive the C++ callback before the
    // original function is entered.
    sub sp, sp, #PUREDART_CONTEXT_SIZE
    .cfi_def_cfa_offset PUREDART_TOTAL_FRAME_SIZE

    // HookContext::x[0..30]
    stp x0,  x1,  [sp, #0]
    stp x2,  x3,  [sp, #16]
    stp x4,  x5,  [sp, #32]
    stp x6,  x7,  [sp, #48]
    stp x8,  x9,  [sp, #64]
    stp x10, x11, [sp, #80]
    stp x12, x13, [sp, #96]
    str x14, [sp, #112]
    ldr x9, [sp, #PUREDART_PROXY_DART_SP_OFFSET] // veneer IP frame on Dart stack
    add x10, x9, #16
    str x10, [sp, #120] // original x15 (Dart SP)
    ldp x10, x11, [x9]
    stp x10, x11, [sp, #128]
    stp x18, x19, [sp, #144]
    stp x20, x21, [sp, #160]
    stp x22, x23, [sp, #176]
    stp x24, x25, [sp, #192]
    stp x26, x27, [sp, #208]
    stp x28, x29, [sp, #224]
    str x30, [sp, #240]
    ldr x9, [sp, #PUREDART_PROXY_CPU_SP_OFFSET] // original CPU SP
    str x9, [sp, #248]
    str wzr, [sp, #256]
    str wzr, [sp, #260]
    stp q0, q1, [sp, #272]
    stp q2, q3, [sp, #304]
    stp q4, q5, [sp, #336]
    stp q6, q7, [sp, #368]

    mov x0, x16
    mov x1, sp
    bl puredart_dispatch_hook

    ldr w9, [sp, #256]
    cbz w9, .Lcall_original

    // Restore the callback-provided return and all argument/scratch registers.
    .cfi_remember_state
    ldp q0, q1, [sp, #272]
    ldp q2, q3, [sp, #304]
    ldp q4, q5, [sp, #336]
    ldp q6, q7, [sp, #368]
    ldp x0,  x1,  [sp, #0]
    ldp x2,  x3,  [sp, #16]
    ldp x4,  x5,  [sp, #32]
    ldp x6,  x7,  [sp, #48]
    ldp x8,  x9,  [sp, #64]
    ldp x10, x11, [sp, #80]
    ldp x12, x13, [sp, #96]
    ldr x14, [sp, #112]
    add sp, sp, #PUREDART_CONTEXT_SIZE
    .cfi_def_cfa_offset 144
    ldp x16, x18, [sp], #16
    .cfi_def_cfa_offset 128
    .cfi_restore x18
    ldp x15, x17, [sp], #16
    .cfi_def_cfa_offset 112
    ldp x19, x20, [sp], #16
    .cfi_def_cfa_offset 96
    .cfi_restore x19
    .cfi_restore x20
    ldp x21, x22, [sp], #16
    .cfi_def_cfa_offset 80
    .cfi_restore x21
    .cfi_restore x22
    ldp x23, x24, [sp], #16
    .cfi_def_cfa_offset 64
    .cfi_restore x23
    .cfi_restore x24
    ldp x25, x26, [sp], #16
    .cfi_def_cfa_offset 48
    .cfi_restore x25
    .cfi_restore x26
    ldp x27, x28, [sp], #16
    .cfi_def_cfa_offset 32
    .cfi_restore x27
    .cfi_restore x28
    ldp x29, x30, [sp], #16
    .cfi_def_cfa_offset 16
    .cfi_restore x29
    .cfi_restore x30
    ldr x17, [sp, #8] // restore original CPU SP before returning to Dart
    mov sp, x17
    ldp x16, x17, [x15], #16 // consume veneer frame and restore Dart SP
    .cfi_def_cfa_offset 0
    ret

.Lcall_original:
    .cfi_restore_state
    // Restore the input argument state, then tail-call the native
    // trampoline. x17 is the AAPCS64 inter-procedure scratch branch register.
    ldp q0, q1, [sp, #272]
    ldp q2, q3, [sp, #304]
    ldp q4, q5, [sp, #336]
    ldp q6, q7, [sp, #368]
    ldp x0,  x1,  [sp, #0]
    ldp x2,  x3,  [sp, #16]
    ldp x4,  x5,  [sp, #32]
    ldp x6,  x7,  [sp, #48]
    ldp x8,  x9,  [sp, #64]
    ldp x10, x11, [sp, #80]
    ldp x12, x13, [sp, #96]
    ldr x14, [sp, #112]
    add sp, sp, #PUREDART_CONTEXT_SIZE
    .cfi_def_cfa_offset 144
    ldp x16, x18, [sp], #16
    .cfi_def_cfa_offset 128
    .cfi_restore x18
    ldp x15, x17, [sp], #16
    .cfi_def_cfa_offset 112
    ldp x19, x20, [sp], #16
    .cfi_def_cfa_offset 96
    .cfi_restore x19
    .cfi_restore x20
    ldp x21, x22, [sp], #16
    .cfi_def_cfa_offset 80
    .cfi_restore x21
    .cfi_restore x22
    ldp x23, x24, [sp], #16
    .cfi_def_cfa_offset 64
    .cfi_restore x23
    .cfi_restore x24
    ldp x25, x26, [sp], #16
    .cfi_def_cfa_offset 48
    .cfi_restore x25
    .cfi_restore x26
    ldp x27, x28, [sp], #16
    .cfi_def_cfa_offset 32
    .cfi_restore x27
    .cfi_restore x28
    ldp x29, x30, [sp], #16
    .cfi_def_cfa_offset 16
    .cfi_restore x29
    .cfi_restore x30
    // Restore CSP; the trampoline consumes the IP frame through x15, replays
    // the Dart prologue, and branches directly to the original continuation.
    ldr x16, [sp, #8]
    mov sp, x16
    br x17
    .cfi_endproc
    .size puredart_hook_common, .-puredart_hook_common
