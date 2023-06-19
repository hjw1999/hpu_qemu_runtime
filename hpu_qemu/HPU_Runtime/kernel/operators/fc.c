/* This is the code of fully-connected layers */
#include "hihw.h"
#include "libconv.h"
#include "dma.h"
#include "lock.h"
#include "int.h"
#include "operators/hi_krnl_param_fc.h"
#include "hi_addr_def.h"
#include "krnl_log.h"
#include "hpu_util.h"

// #define WAIT_COUNT_FOR_FC_WEIGHT    (10000000)
extern void wait(int input_var);
// static uint32_t bias_size = 0, shift_size = 0, bias_local_mem_start = 0, shift_local_mem_start = 0;
// static uint32_t ifm_local_mem_start = 0, fc_ifm_lcaddr = 0, wt_local_mem_start = 0, ofm_local_mem_start = 0, ifm_rsize = 0;
// static uint32_t mmac_region_start = 0, mmac_region_end = 0, mmac_fm_blk_size = 0, mmac_fm_cluster_num = 0,      \
//                 mmac_fm_blk_num = 0, mmac_fm_cluster_start = 0, mmac_fm_cluster_end = 0;


// static void init_static_var(paramTableFc_Entry_t *p_fc_param_entry)
// {
//     bias_size = 0, shift_size = 0;
//     ifm_rsize = 0;
//     ifm_local_mem_start = MMA_BANK0_START_ADDR;
//     if (p_fc_param_entry->b_connect_with_conv)
//     {
//         fc_ifm_lcaddr = MMA_BANK0_START_ADDR + MMA_BANK_SIZE / 2;
//     }
//     else
//     {
//         fc_ifm_lcaddr = ifm_local_mem_start;
//     }
//     ofm_local_mem_start = MMA_BANK1_START_ADDR;    
//     bias_local_mem_start = MMA_BANK2_START_ADDR, shift_local_mem_start = 0;
//     wt_local_mem_start = MMB_START_ADDR;
//     mmac_region_start = 0, mmac_region_end = 0, mmac_fm_blk_size = 0, mmac_fm_cluster_num = 0,      \
//     mmac_fm_blk_num = 0, mmac_fm_cluster_start = 0, mmac_fm_cluster_end = 0;
// }
#define define_init_local_variables(p_fc_param_entry)  int bias_local_mem_start = MMA_BANK3_START_ADDR;\
                                  int fc_ifm_lcaddr = 0;\
                                  uint32 ifm_rsize = p_fc_param_entry->fc_param.conv2d.cshape.ifm_c;\
                                  uint32 ofm_rsize = p_fc_param_entry->fc_param.conv2d.cshape.ofm_c;\
                                  int ifm_local_mem_start = MMA_BANK0_START_ADDR;\
                                  if (p_fc_param_entry->b_connect_with_conv) {\
                                      fc_ifm_lcaddr = MMA_BANK1_START_ADDR;\
                                  }\
                                  else {\
                                      fc_ifm_lcaddr = ifm_local_mem_start;\
                                  }\
                                  int wt_local_mem_start = MMB_START_ADDR; \
                                  int ofm_local_mem_start = MMA_BANK2_START_ADDR;\
	                              int bias_size = p_fc_param_entry->fc_param.conv2d.cshape.ofm_c * sizeof(int32_t);	\
	                              int shift_size = p_fc_param_entry->fc_param.conv2d.cshape.ofm_c;\
                                  int shift_local_mem_start = bias_local_mem_start + bias_size;

#define load_bias_shift(p_fc_param_entry) do{\
	KRNL_LOG_INFO(LOG_DEBUG, "Load Bias: [%x]->[%x] size: %d",  p_fc_param_entry->fc_param.bias_addr.lcaddr, bias_local_mem_start, GMEM_ALIGN(bias_size));\
	LIBHIKL_NASSERT(__rd_from_remote_chunk_blocking((uint32 *)(bias_local_mem_start), p_fc_param_entry->fc_param.bias_addr.x_pos, p_fc_param_entry->fc_param.bias_addr.y_pos, p_fc_param_entry->fc_param.bias_addr.lcaddr, GMEM_ALIGN(bias_size)));\
	KRNL_LOG_INFO(LOG_DEBUG, "Load Shift: [%x]->[%x] size: %d", p_fc_param_entry->fc_param.shift_addr.lcaddr, shift_local_mem_start, GMEM_ALIGN(shift_size));\
	LIBHIKL_NASSERT(__rd_from_remote_chunk_blocking((uint32 *)shift_local_mem_start, p_fc_param_entry->fc_param.shift_addr.x_pos, p_fc_param_entry->fc_param.shift_addr.y_pos, p_fc_param_entry->fc_param.shift_addr.lcaddr, GMEM_ALIGN(shift_size)));\
}while(0)

#define _set_mmac_param_fc_(p_fc_param_entry) do{\
    int mmac_region_start = ByteToW64(fc_ifm_lcaddr - MEM_LCMEM_ADDR_S);\
    int mmac_region_end = ByteToW64(fc_ifm_lcaddr - MEM_LCMEM_ADDR_S + ifm_rsize);\
    int mmac_fm_blk_size = p_fc_param_entry->fc_param.conv2d.cshape.ifm_c/VEC_SCALE - 1;\
    int mmac_fm_cluster_num = 0;\
    int mmac_fm_blk_num = 0; \
    int mmac_fm_cluster_start = ByteToW64(fc_ifm_lcaddr - MEM_LCMEM_ADDR_S);\
    int mmac_fm_cluster_end = ByteToW64(fc_ifm_lcaddr - MEM_LCMEM_ADDR_S + ifm_rsize);\
    _set_mmac_region_start(mmac_region_start);\
    _set_mmac_region_end(mmac_region_end);\
    _set_mmac_fm_blk_size(mmac_fm_blk_size);\
    _set_mmac_fm_cluster_num(mmac_fm_cluster_num);\
    _set_mmac_fm_blk_num(mmac_fm_blk_num);\
    _set_mmac_fm_cluster_start(mmac_fm_cluster_start);\
    _set_mmac_fm_cluster_end(mmac_fm_cluster_end);\
    KRNL_LOG_INFO(LOG_DEBUG, "mmac_region_start: 0x%x", mmac_region_start);\
    KRNL_LOG_INFO(LOG_DEBUG, "mmac_region_end: 0x%x", mmac_region_end);\
    KRNL_LOG_INFO(LOG_DEBUG, "mmac_fm_blk_size: 0x%x", mmac_fm_blk_size);\
    KRNL_LOG_INFO(LOG_DEBUG, "mmac_fm_cluster_num: 0x%x", mmac_fm_cluster_num);\
    KRNL_LOG_INFO(LOG_DEBUG, "mmac_fm_blk_num: 0x%x", mmac_fm_blk_num);\
    KRNL_LOG_INFO(LOG_DEBUG, "mmac_fm_cluster_start: 0x%x", mmac_fm_cluster_start);\
    KRNL_LOG_INFO(LOG_DEBUG, "mmac_fm_cluster_end: 0x%x", mmac_fm_cluster_end);\
}while(0)

void fc_core(paramTableFc_Entry_t *p_fc_param_entry)
{   
    define_init_local_variables(p_fc_param_entry);
    // init_static_var(p_fc_param_entry);

    KRNL_LOG_INFO(LOG_SYSTEM, "fc_core");
    load_bias_shift(p_fc_param_entry);

    int iterations = p_fc_param_entry->fc_param.conv2d.cshape.ofm_c / VEC_SCALE;
    uint32 wt_size = p_fc_param_entry->fc_param.conv2d.cshape.ifm_c * p_fc_param_entry->fc_param.conv2d.cshape.ofm_c;
    uint32 one_time_mvmac_wt_size = p_fc_param_entry->fc_param.conv2d.cshape.ifm_c * VEC_SCALE;
    uint32 wt_size_for_each_ndma = (MMB_BYTE_SIZE / one_time_mvmac_wt_size) * one_time_mvmac_wt_size;
    uint32 last_time_wt_size_for_mvmac = wt_size % wt_size_for_each_ndma;
    uint32 ndma_wt_load_iterations = wt_size / wt_size_for_each_ndma;
    uint32 ndma_wt_load_time = 0;

    KRNL_LOG_INFO(LOG_DEBUG, "total wt_size: %d, one_time_mvmac_wt_size: %d wt_size_for_each_ndma: %d last_time_wt_size_for_mvmac: %d ndma_wt_load_iterations: %d", \
                    wt_size, one_time_mvmac_wt_size, wt_size_for_each_ndma, last_time_wt_size_for_mvmac, ndma_wt_load_iterations);
    uint32 remote_wt_lcaddr_offset = p_fc_param_entry->fc_param.wt_addr.lcaddr;

    uint32 wt_start = 0, ofm_start = 0, bias_start = 0, shift_start = 0, ifm_inner_start = 0, wt_offset;
    uint32 wt_addr = p_fc_param_entry->fc_param.wt_addr.lcaddr;
    
    uint32 relu = p_fc_param_entry->fc_param.conv2d.relu;

    // initially load ifm to start conv
    LIBHIKL_NASSERT(__rd_from_remote_chunk_blocking((uint32 *)ifm_local_mem_start, p_fc_param_entry->fc_param.ifm_addr.x_pos, p_fc_param_entry->fc_param.ifm_addr.y_pos, p_fc_param_entry->fc_param.ifm_addr.lcaddr, GMEM_ALIGN(ifm_rsize)));

    if (p_fc_param_entry->b_connect_with_conv)
    {
        KRNL_LOG_INFO(LOG_DEBUG, "connect_with_conv: ifm_h %d ifm_w %d ", p_fc_param_entry->pre_conv2d.cshape.ifm_h, p_fc_param_entry->pre_conv2d.cshape.ifm_w);
        __intrinsic_func_layout_for_fc_input_with_c8__(ByteToW64(ifm_local_mem_start - MEM_LCMEM_ADDR_S),                                           \
                                                        ByteToW64(fc_ifm_lcaddr - MEM_LCMEM_ADDR_S),                                                \
                                                        p_fc_param_entry->pre_conv2d.cshape.ifm_h, p_fc_param_entry->pre_conv2d.cshape.ifm_w);
    }
    else{

        KRNL_LOG_INFO(LOG_DEBUG, "connect_with fc");
    }
    vmu_poll();
    KRNL_LOG_INFO(LOG_DEBUG, "fc_ifm_lcaddr: ");
    // buf_print(fc_ifm_lcaddr, GMEM_ALIGN(ifm_rsize));
    
    _set_mmac_param_fc_(p_fc_param_entry);
    _set_mmac_vfadd_param_();
    int mvmac_times_with_each_ndma = wt_size_for_each_ndma / one_time_mvmac_wt_size;
    for(int i=0; i < iterations; i++)
    {
        KRNL_LOG_INFO(LOG_DEBUG, "line number is %d", i);
        //wt load to MMB, can't paralize due to hardware limitation
        if (i % (mvmac_times_with_each_ndma) == 0)
        {
            if (ndma_wt_load_time < ndma_wt_load_iterations)
            {
                KRNL_LOG_INFO(LOG_DEBUG, "ndma_wt_load_time is %d, remote_wt_lcaddr_offset: 0x%x", ndma_wt_load_time, remote_wt_lcaddr_offset);
                //wt load to MMB, can't paralize due to hardware limitation
                LIBHIKL_NASSERT(__rd_from_remote_chunk_blocking((uint32 *)(wt_local_mem_start), p_fc_param_entry->fc_param.wt_addr.x_pos, p_fc_param_entry->fc_param.wt_addr.y_pos, remote_wt_lcaddr_offset, GMEM_ALIGN(wt_size_for_each_ndma)));
                remote_wt_lcaddr_offset += GMEM_ALIGN(wt_size_for_each_ndma);
                ndma_wt_load_time ++;
                //@@@hardware issue, ndma_poll() not work
                // wait(WAIT_COUNT_FOR_FC_WEIGHT);
            }
            else if (last_time_wt_size_for_mvmac != 0)
            {
                LIBHIKL_NASSERT(__rd_from_remote_chunk_blocking((uint32 *)(wt_local_mem_start), p_fc_param_entry->fc_param.wt_addr.x_pos, p_fc_param_entry->fc_param.wt_addr.y_pos, remote_wt_lcaddr_offset, GMEM_ALIGN(last_time_wt_size_for_mvmac)));
                // wait(WAIT_COUNT_FOR_FC_WEIGHT);
            }
        }
        wt_offset = i % (mvmac_times_with_each_ndma) * one_time_mvmac_wt_size;
        wt_start = ByteToW64(wt_local_mem_start + wt_offset - MEM_LCMEM_ADDR_S);

        // ifm_inner_start = ByteToW64(ifm_local_mem_start - MEM_LCMEM_ADDR_S + i * VEC_SCALE);
        ifm_inner_start = ByteToW64(fc_ifm_lcaddr - MEM_LCMEM_ADDR_S);
        ofm_start = ByteToW64(ofm_local_mem_start - MEM_LCMEM_ADDR_S + i * VEC_SCALE);
        bias_start = ByteToW64(bias_local_mem_start - MEM_LCMEM_ADDR_S + i * VEC_SCALE * sizeof(int32_t)) ;
        shift_start = ByteToW64(shift_local_mem_start - MMA_START_ADDR + i * VEC_SCALE);

        // KRNL_LOG_INFO(LOG_DEBUG, "ifm_inner_start: ");
        // buf_print(W64ToByte(ifm_inner_start) + MEM_LCMEM_ADDR_S, GMEM_ALIGN(64));
        // KRNL_LOG_INFO(LOG_DEBUG, "bias_start: ");
        // buf_print(W64ToByte(bias_start) + MEM_LCMEM_ADDR_S, GMEM_ALIGN(64));
        // KRNL_LOG_INFO(LOG_DEBUG, "shift_start: ");
        // buf_print(W64ToByte(shift_start) + MEM_LCMEM_ADDR_S, GMEM_ALIGN(64));

        __intrinsic_func__(ifm_inner_start, wt_start, ofm_start, bias_start, shift_start, relu, CONV_TYPE_FC, 1);

    }
    LIBHIKL_NASSERT(__wr_to_remote_chunk_blocking((uint32 *)ofm_local_mem_start, p_fc_param_entry->fc_param.ofm_addr.x_pos, p_fc_param_entry->fc_param.ofm_addr.y_pos, p_fc_param_entry->fc_param.ofm_addr.lcaddr, ofm_rsize*2));
	KRNL_LOG_INFO(LOG_SYSTEM, "write to ddr: 0x[%d%d]%x with length: %d", p_fc_param_entry->fc_param.ofm_addr.x_pos, p_fc_param_entry->fc_param.ofm_addr.y_pos, p_fc_param_entry->fc_param.ofm_addr.lcaddr, ofm_rsize);
}

void fc_multi_layers()
{
    KRNL_LOG_INFO(LOG_SYSTEM, " fc_multi_layers");
#ifdef QEMU_ENV
    qemu_arch_setup();
    printf("=============================\n\r");
    printf("YOU ARE USING A QEMU INSTANCE\n\r");
    printf("=============================\n\r");
#endif
    paramTableFc_t *_pParamTable = *((paramTableFc_t **)HIPU200_KNL_PTABLE_ADDR); /* get kernel param table from runtime */
    hirtKernelParamTableBase_t *base_table = &_pParamTable->infoBase;

    unsigned int *_flags = ( unsigned int *)HIPU200_MEM_ATOMIC_START;
    paramTableFc_Entry_t *fc_entry_param = &_pParamTable->param;

    fc_core(fc_entry_param);

#ifdef QEMU_ENV
    qemu_fprint(QEMU_LOG_MMAB, 0x38000>>6, 1024);
	qemu_fprint(QEMU_LOG_MMAB, 0x38400>>6, 256);
    qemu_fprint(QEMU_LOG_MEM, 0xc0200000, 327680);
#endif
}
