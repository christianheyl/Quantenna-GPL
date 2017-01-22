/*
 * must copy sram data + text segment BEFORE setting up the kernel stack,
 * as init_thread_union may reside in this section
 */
__copy_sram_datatext:
    mov r5, __sram_datatext_start
    mov r6, __sram_datatext_end
    mov r7, __sram_load_addr

__copy_sram_datatext_loop:
    ld.ab r3, [r7, 4]
    st.ab r3, [r5, 4]
    brlt  r5, r6, __copy_sram_datatext_loop

    ;; Flush and invalidate dcache
    ; Set flush mode for invalidate operation
    lr   r3, [0x48] ; DC_CTRL register
    bset r3, r3, 0x6
    sr   r3, [0x48] ; DC_CTRL register
    ; Start operation
    mov  r3, 0x1
    sr   r3, [0x47] ; DC_IVDC register
    ; Wait operation completion
__dcache_flush_continue:
    lr   r3, [0x48] ; DC_CTRL register
    and  r3, r3, 0x100
    brne r3, 0x0, __dcache_flush_continue

    ;; Invalidate icache
    mov r3, 0x1
    sr  r3, [0x10] ; IC_IVIC

