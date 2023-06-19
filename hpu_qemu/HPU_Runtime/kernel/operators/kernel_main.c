// ---------------------------------------------------------------------------------------------------------------------
// Copyright (c) 1986 - 2020, CAG team, Institute of AI and Robotics, Xi'an Jiaotong University. Proprietary and
// Confidential All Rights Reserved.
// ---------------------------------------------------------------------------------------------------------------------
// NOTICE: All information contained herein is, and remains the property of CAG team, Institute of AI and Robotics,
// Xi'an Jiaotong University. The intellectual and technical concepts contained herein are proprietary to CAG team, and
// may be covered by P.R.C. and Foreign Patents, patents in process, and are protected by trade secret or copyright law.
//
// This work may not be copied, modified, re-published, uploaded, executed, or distributed in any way, in any time, in
// any medium, whether in whole or in part, without prior written permission from CAG team, Institute of AI and
// Robotics, Xi'an Jiaotong University.
//
// The copyright notice above does not evidence any actual or intended publication or disclosure of this source code,
// which includes information that is confidential and/or proprietary, and is a trade secret, of CAG team.
// ---------------------------------------------------------------------------------------------------------------------
// FILE NAME  : main.c
// DEPARTMENT : Architecture
// AUTHOR     : wenzhe
// AUTHOR'S EMAIL : wenzhe@xjtu.edu.cn
// ---------------------------------------------------------------------------------------------------------------------
// Ver 1.0  2020--08--01 initial version.
// ---------------------------------------------------------------------------------------------------------------------
#include "hpu_def.h"
#include "hpu_api.h"
#include "hihw.h"
#include "int.h"
#include "hi_addr_def.h"
#include "hi_krnl_param.h"
#include "hisdk_config.h"
#include "krnl_log.h"
// #include "qemu.h"
#include "operators/hi_krnl_param_conv2d.h"
#include "hpu200/operators/hi_krnl_param_conv2d_hpu200.h"

// #ifdef __cplusplus
// extern "C" {
// #endif // __cplusplus

//#define COREMARK

typedef void (*op_func)();
typedef struct
{
  u32_t op_type;
  char op_name[50];
  op_func op_func;
} HiKernelOpEntry;

// #ifdef __cplusplus
// }
// #endif


// class HiKernelOpEntry
// {
// public:
//   u32_t op_type;
//   char op_name[50];
//   op_func op_func;
// };
extern void clear_profiling_log_offset(int core_idx);
extern void log_cycle(char *tag);
extern void flush_l2_cache();
//custom defined op_funcs
extern void conv2d_byoc();
extern void fc_multi_layers();
extern void kernel_conv3s1();
extern void vadd();
#ifdef QEMU_ENV
    extern void qemu_arch_setup();
#endif

#define BASE_INDEX 1000

static HiKernelOpEntry gOpEntryList[] = {

    {KERNEL_OP_CONV2D_BYOC,                             "conv2d_byoc",                             &conv2d_byoc},
    {KERNEL_OP_FC,                                      "fc_multi_layers",                         &fc_multi_layers},
    {KERNEL_OP_CONV3S1,                                 "conv3s1",                                 &kernel_conv3s1},
    {KERNEL_OP_VADD,                                    "vadd",                                    &vadd},
    
  //{add                             your                               op                                 here},

 
};

void _kernel_sync(int rootCoreNum, int idx);

void kernel_entry(void) {
  // while(1)
  {
    volatile amo_struct *amo_mem = (amo_struct *)AMO_ADDR_S;
    // amo_struct *amo_mem = (amo_struct *)AMO_ADDR_S;
    // while(!amo_mem->b_start) {;}
    // amo_mem->b_start = 0;
    #ifdef QEMU_ENV
    qemu_arch_setup();
    printf("func: kernel_entry\n");
    #endif
    amo_mem->busy_ndma_channel_count = 0;
    amo_mem->lock = 0;
    // amo_mem->sync_value = 0;
    // initialize the interrupt configuration
    // init_intr();         //set at the beginning of _init
    enable_intr();

    int event3 = 0x00000200;
    int event4 = 0x00000400;
    int event5 = 0x00000200;
    int event6 = 0x00000200;
    int event7 = 0x00000200;
    int count = 0;

    csrw(CSR_ADDR_MCS_MHPMEVENT03, event3);
    csrw(CSR_ADDR_MCS_MHPMEVENT04, event4);
    csrw(CSR_ADDR_MCS_MHPMEVENT05, event5);
    csrw(CSR_ADDR_MCS_MHPMEVENT06, event6);
    csrw(CSR_ADDR_MCS_MHPMEVENT07, event7);
    csrr(count, CSR_ADDR_MCT_HPMCNT03_H);


    // KRNL_LOG_INFO(LOG_SYSTEM, "fire sw interrupt");
    // *(unsigned int *)HIPU200_MEM_SSIG_ADDR = 1;
    // int _coreid = get_hpuid(); /*get core id number*/
    unsigned int _coreid = amo_mem->_coreid;
    clear_profiling_log_offset(_coreid);
    KRNL_LOG_INFO(LOG_OP_CMD, "enter into kernel_entry for core: %d\n", _coreid);
    log_cycle((char *)"kernel start");

    // unsigned int _taskid;
    // int *_rtcode = (int *)HIPU200_KNL_RTCODE_ADDR; /*kernel return code to host
    // runtime*/
    // *_rtcode = HIPU200_KNL_RTCODE_BEGIN;
    amo_mem->return_code = HIPU200_KNL_RTCODE_BEGIN;
  #ifdef QEMU_ENV
    uint32_t PTABLE_ADDR = 0x8ce10600;
    // *(int *)HIPU200_KNL_PTABLE_ADDR = PTABLE_ADDR;
    // *((paramTableConv2d_t **)HIPU200_KNL_PTABLE_ADDR) = PTABLE_ADDR;
    // paramTableConv2d_t *_pParamTable = *((paramTableConv2d_t
    // **)HIPU200_KNL_PTABLE_ADDR);
    amo_mem->param_table = PTABLE_ADDR;
    PTConv2ds *_pParamTable =
        (hirtKernelParamTableBase_t *)amo_mem->param_table;
    hikl_addr_t ddr_ifm_addr = {0, 3, 0xc0000000};
    hikl_addr_t ddr_wt_addr = {0, 3, 0xc0100000};
    hikl_addr_t ddr_ofm_addr = {0, 3, 0xc0200000};
    hikl_addr_t ddr_bias_addr = {0, 3, 0xc01f0000};
    hikl_addr_t ddr_shift_addr = {0, 3, 0xc01fc000};
    _pParamTable->param[0].tiled_param_host.Tilingwithdev.ifm_rmt_addr = ddr_ifm_addr;
    _pParamTable->param[0].tiled_param_host.Tilingwithdev.wt_rmt_addr = ddr_wt_addr;
    _pParamTable->param[0].tiled_param_host.Tilingwithdev.ofm_rmt_addr = ddr_ofm_addr;
    _pParamTable->param[0].tiled_param_host.Tilingwithdev.bias_rmt_addr = ddr_bias_addr;
    _pParamTable->param[0].tiled_param_host.Tilingwithdev.shift_rmt_addr = ddr_shift_addr;    
  #endif
    // hirtKernelParamTableBase_t *_ptable = *((hirtKernelParamTableBase_t
    // **)HIPU200_KNL_PTABLE_ADDR);/*get kernel param table from runtime*/
    hirtKernelParamTableBase_t *_ptable =
        (hirtKernelParamTableBase_t *)amo_mem->param_table;

    int paramTableLen = _ptable->table_size;
    int parallelCoreNum = _ptable->task_dim;
   

    // KRNL_LOG_INFO(LOG_SYSTEM, "task_dim: %d op_type %d", _ptable->task_dim, _ptable->op_type);
 
    /*judge if the parallelism is bigger than the maxcorenum*/
  #ifdef QEMU_ENV
    printf("the addr of table: %x\n", _ptable);
    printf("parallelCoreNum: %d\n", parallelCoreNum);
    printf("parallelLen: %d\n", _ptable->table_size);
    printf("op_type: %d\n", _ptable->op_type);
    // wchar_t *script = "../lanuch.py";
  #endif
    if ((parallelCoreNum > HIPU200_SOC_CORE_NUM) || (parallelCoreNum < 1)) {
      amo_mem->return_code = 0xdead0001;
      goto fail;
    }

    if (paramTableLen < 1) {
      amo_mem->return_code = 0xdead0003;
      goto fail;
    }
    // enter into op func
    KRNL_LOG_INFO(LOG_SYSTEM, "%s"," --------------into OP ------------ ");
    
    
    //----------------------------------set right op index here!!!!-------------------------------------------
    ((HiKernelOpEntry *)&gOpEntryList[1])->op_func();
    
    
    
    // synchronize all cores within current task group
    _kernel_sync(0, 0);
    KRNL_LOG_INFO(LOG_SYSTEM, "%s"," --------------kernel sync over ------------ ");
    log_cycle((char *)"kernel end");
    // return code = ok
    // *_rtcode = HIPU200_KNL_RTCODE_SUCCESS;
    amo_mem->return_code = HIPU200_KNL_RTCODE_SUCCESS;

    disable_intr();
  fail:
  #ifndef COREMARK
    flush_l2_cache();
  #endif
    // KRNL_LOG_INFO(LOG_OP_CMD, "fire wfi interrupt");
    // _end();
    KRNL_LOG_INFO(LOG_SYSTEM, "fire sw interrupt");
    *(unsigned int *)HIPU200_MEM_SSIG_ADDR = 1;
  }
}

// multicore synchronization with rootcore dtcm
void _kernel_sync(int rootCoreNum, int idx) {}

void timer_callback(void) { return; }

void exception_handle() {
  unsigned int *p_mcause = (unsigned int *)HIPU200_KNL_MCAUSE_ADDR;
  unsigned int *p_mepc = (unsigned int *)HIPU200_KNL_MEPC_ADDR;
  unsigned int *p_mtval = (unsigned int *)HIPU200_KNL_MTVAL_ADDR;
  unsigned int *p_mstatus = (unsigned int *)HIPU200_KNL_MSTATUS_ADDR;
  asm volatile("csrr %[mcause_dest], mcause\n"
               : [mcause_dest] "=r"(*p_mcause)::);
  asm volatile("csrr %[mepc_dest], mepc\n" : [mepc_dest] "=r"(*p_mepc)::);
  asm volatile("csrr %[mtval_dest], mtval\n" : [mtval_dest] "=r"(*p_mtval)::);
  asm volatile("csrr %[mstatus_dest], mstatus\n"
               : [mstatus_dest] "=r"(*p_mstatus)::);
 
  flush_l2_cache();
  _end();
}
