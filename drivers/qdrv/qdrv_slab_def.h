# 1 "../drivers/qdrv/qdrv_slab_def.h.in"
# 1 "<command-line>"
# 1 "/usr/include/stdc-predef.h" 1 3 4
# 1 "<command-line>" 2
# 1 "../drivers/qdrv/qdrv_slab_def.h.in"
# 52 "../drivers/qdrv/qdrv_slab_def.h.in"
struct qdrv_slab_watch {
# 61 "../drivers/qdrv/qdrv_slab_def.h.in"
# 1 "../drivers/qdrv/qdrv_slab_watch.h" 1
unsigned int stat_size_tot_alloc_64; unsigned int stat_size_cur_alloc_64; unsigned int stat_size_act_alloc_64; unsigned int stat_size_hwm_alloc_64;
unsigned int stat_size_tot_alloc_96; unsigned int stat_size_cur_alloc_96; unsigned int stat_size_act_alloc_96; unsigned int stat_size_hwm_alloc_96;
unsigned int stat_size_tot_alloc_128; unsigned int stat_size_cur_alloc_128; unsigned int stat_size_act_alloc_128; unsigned int stat_size_hwm_alloc_128;
unsigned int stat_size_tot_alloc_192; unsigned int stat_size_cur_alloc_192; unsigned int stat_size_act_alloc_192; unsigned int stat_size_hwm_alloc_192;
unsigned int stat_size_tot_alloc_256; unsigned int stat_size_cur_alloc_256; unsigned int stat_size_act_alloc_256; unsigned int stat_size_hwm_alloc_256;
unsigned int stat_size_tot_alloc_512; unsigned int stat_size_cur_alloc_512; unsigned int stat_size_act_alloc_512; unsigned int stat_size_hwm_alloc_512;
unsigned int stat_size_tot_alloc_1024; unsigned int stat_size_cur_alloc_1024; unsigned int stat_size_act_alloc_1024; unsigned int stat_size_hwm_alloc_1024;
unsigned int stat_size_tot_alloc_2048; unsigned int stat_size_cur_alloc_2048; unsigned int stat_size_act_alloc_2048; unsigned int stat_size_hwm_alloc_2048;
unsigned int stat_size_tot_alloc_4096; unsigned int stat_size_cur_alloc_4096; unsigned int stat_size_act_alloc_4096; unsigned int stat_size_hwm_alloc_4096;
unsigned int stat_size_tot_alloc_RX_BUF_SIZE_KMALLOC; unsigned int stat_size_cur_alloc_RX_BUF_SIZE_KMALLOC; unsigned int stat_size_act_alloc_RX_BUF_SIZE_KMALLOC; unsigned int stat_size_hwm_alloc_RX_BUF_SIZE_KMALLOC;
unsigned int stat_tot_alloc_skbuff_head_cache; unsigned int stat_cur_alloc_skbuff_head_cache; unsigned int stat_act_alloc_skbuff_head_cache; unsigned int stat_hwm_alloc_skbuff_head_cache;
# 62 "../drivers/qdrv/qdrv_slab_def.h.in" 2


} __packed;

enum qdrv_slab_index {


# 1 "../drivers/qdrv/qdrv_slab_watch.h" 1
QDRV_SLAB_IDX_SIZE_64,
QDRV_SLAB_IDX_SIZE_96,
QDRV_SLAB_IDX_SIZE_128,
QDRV_SLAB_IDX_SIZE_192,
QDRV_SLAB_IDX_SIZE_256,
QDRV_SLAB_IDX_SIZE_512,
QDRV_SLAB_IDX_SIZE_1024,
QDRV_SLAB_IDX_SIZE_2048,
QDRV_SLAB_IDX_SIZE_4096,
QDRV_SLAB_IDX_SIZE_RX_BUF_SIZE_KMALLOC,
QDRV_SLAB_IDX_skbuff_head_cache,
# 70 "../drivers/qdrv/qdrv_slab_def.h.in" 2


 QDRV_SLAB_IDX_MAX
};
