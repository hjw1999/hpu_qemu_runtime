/*
 * @Descripttion:
 * @version:
 * @Author: AlonzoChen
 * @Date: 2020-12-29 14:35:14
 * @LastEditors: AlonzoChen
 * @LastEditTime: 2021-05-12 20:44:07
 */
#include <string.h>
#include <assert.h>

#include "hpu_util.h"
#include "hisdk_config.h"
#include "krnl_log.h"
extern void flush_l2_cache();

static int profiling_log_offset = 0;
static int base_profiling_log_offset = 0;

void clear_profiling_log_offset(int core_idx)
{
    base_profiling_log_offset = HIRT_HIPU200_CORE_LOCAL_MEM_PROFILING_LENGTH_FOR_EACH_CORE * core_idx;
    profiling_log_offset = base_profiling_log_offset;
}

void inline log_time(char *tag)
{
    uint32_t time_tic_l = 0;
    uint32_t time_tic_h = 0;
    csrr(time_tic_l, CSR_ADDR_MCT_TIME_L);
    csrr(time_tic_h, CSR_ADDR_MCT_TIME_H);
    if (profiling_log_offset + sizeof(Log_time_struct) - base_profiling_log_offset > HIRT_HIPU200_CORE_LOCAL_MEM_PROFILING_LENGTH_FOR_EACH_CORE)
    {
        profiling_log_offset = base_profiling_log_offset;
    }
    ((Log_time_struct *)(profiling_log_offset + HIRT_HIPU200_CORE_LOCAL_MEM_PROFILING_OFFSET))->time_l = time_tic_l;
    ((Log_time_struct *)(profiling_log_offset + HIRT_HIPU200_CORE_LOCAL_MEM_PROFILING_OFFSET))->time_h = time_tic_h;
    memcpy(((Log_time_struct *)(profiling_log_offset + HIRT_HIPU200_CORE_LOCAL_MEM_PROFILING_OFFSET))->tag, tag, TAG_BUF_LENGTH);
    ((Log_time_struct *)(profiling_log_offset + HIRT_HIPU200_CORE_LOCAL_MEM_PROFILING_OFFSET))->tag[TAG_BUF_LENGTH - 1] = '\0';
    profiling_log_offset += sizeof(Log_time_struct);
}
void inline log_cycle(char *tag)
{
    uint32_t cycle_l = 0;
    uint32_t cycle_h = 0;
    csrr(cycle_l, CSR_ADDR_MCT_CYCLE_L);
    csrr(cycle_h, CSR_ADDR_MCT_CYCLE_H);
    char *tmp_tag = ( (Log_cycle_struct *)(profiling_log_offset + HIRT_HIPU200_CORE_LOCAL_MEM_PROFILING_OFFSET) )->tag;
    if (profiling_log_offset + sizeof(Log_cycle_struct) - base_profiling_log_offset > HIRT_HIPU200_CORE_LOCAL_MEM_PROFILING_LENGTH_FOR_EACH_CORE)
    {
        profiling_log_offset = base_profiling_log_offset;
    }
    ((Log_cycle_struct *)(profiling_log_offset + HIRT_HIPU200_CORE_LOCAL_MEM_PROFILING_OFFSET))->cycle_l = cycle_l;
    ((Log_cycle_struct *)(profiling_log_offset + HIRT_HIPU200_CORE_LOCAL_MEM_PROFILING_OFFSET))->cycle_h = cycle_h;
    // memcpy tag[]
    for ( int i = 0; i < TAG_BUF_LENGTH; ++i){
        tmp_tag[i] = tag[i];
    }
    ((Log_cycle_struct *)(profiling_log_offset + HIRT_HIPU200_CORE_LOCAL_MEM_PROFILING_OFFSET))->tag[TAG_BUF_LENGTH - 1] = '\0';
    profiling_log_offset += sizeof(Log_cycle_struct);
}

// static unsigned int *_amo_flags = (unsigned int *)AMO_ADDR_S;

void read_from_ddr(void* addr, unsigned int size, void *data)
{
    //flush_l2_cache
    flush_l2_cache();
    //update local cache
    memcpy(data, addr, size);
}

int u_i_mod(unsigned long value, unsigned long base)
{
    assert(value >= 0);
    assert(base > 0);
    while(value >= 0 && value >= base)
    {
        // _amo_flags[8] = value;
        // _amo_flags[9] = base;
        value = value - base;
    }
    return value;
}

uint32_t bitwise_div_32 (uint32_t dividend, uint32_t divisor)
{
    if(divisor==0){
         printf("³ýÊý²»ÄÜÎªÁã\n");
    }
    uint32_t quot, rem, t;
    int bits_left = 8 * sizeof (uint32_t); //CHAR_BIT *
    quot = dividend;
    rem = 0;
    do {
            // (rem:quot) << 1
            t = quot;
            quot = quot + quot;
            rem = rem + rem + (quot < t);

            if (rem >= divisor) {
                rem = rem - divisor;
                quot = quot + 1;
            }
            bits_left--;
    } while (bits_left);
    return quot;
}

static uint32_t memUsedMMA = 0;
static uint32_t memUsedMMB = 0;

__R_HPU
int Krnl_hpu_malloc(uint32_t *pDevAddr, size_t nBytes, KrnlHPUMemType_t memType)
{
    int ret = 0;

    switch(memType)
    {
    case KERNEL_HPUMEM_TYPE_MMA:
        *pDevAddr = MMA_START_ADDR + memUsedMMA;
        memUsedMMA += GLOCAL_MEM_ALIGN(nBytes);
        break;
    case KERNEL_HPUMEM_TYPE_MMB:
        *pDevAddr = MMB_START_ADDR + memUsedMMB;
        memUsedMMB += GLOCAL_MEM_ALIGN(nBytes);
        break;
    default:
        ret = -1;
    }
    // HISDK_LOG_INFO(LOG_SYSTEM, "<GpuMalloc:size=%lu, addr=0x%lx", nBytes, *pDevAddr);
    return ret;
}

__R_HPU
void buf_print(uint32_t buf_addr, uint32_t buf_len)
{
#ifdef QEMU_ENV
    return;
#endif
#ifdef HIRT_DUMP_DATA_FOR_SIMULATION
    return;
#endif

#ifdef HPU_KERNEL_DEBUG
    char one_line_str[300] = {0};
    char temp[10] = {0};
    int line_num = buf_len / 64;
	KRNL_LOG_INFO(LOG_DEBUG, "buf_addr: 0x%x buf_len: %d line_num: %d", buf_addr, buf_len, line_num);
    for (int line = 0; line < line_num; line ++)
    // for (int line = 0; line < 3; line ++)
 	{
 	    for (int index = 63; index >= 0; index --)
 	    {
 	        if (index == 63)
 	        {
 	            snprintf(temp, 10, "line %d: ", line);
                memcpy(one_line_str, temp, strlen(temp));
                memset(temp, 0, 10);
 	        }
 	        snprintf(temp , 10, "%02x", ((unsigned char *)buf_addr)[index + line * 64]);
            memcpy(one_line_str + strlen(one_line_str), temp, strlen(temp));
 	    }
        memset(temp, 0, 10);
        memcpy(one_line_str + strlen(one_line_str), "\n", strlen("\n"));
	    KRNL_LOG_INFO(LOG_DEBUG, "buf: %s", one_line_str);
        memset(one_line_str, 0, 300);
 	}

#endif
}

void check_over_boundary_of_bank (uint32* var, uint32 size) {

#ifdef HPU_KERNEL_DEBUG

    uint32* start_addr_in_bank = var;
    uint32* end_addr_in_bank = start_addr_in_bank + size;

    if (start_addr_in_bank >= (uint32*)MMA_BANK0_START_ADDR && start_addr_in_bank < (uint32*)MMA_BANK1_START_ADDR && end_addr_in_bank >= (uint32*)MMA_BANK1_START_ADDR)
    {
        KRNL_LOG_INFO(LOG_SYSTEM, "WARNING: over the boundary of BANK0");
    }
    else if (start_addr_in_bank >= (uint32*)MMA_BANK1_START_ADDR && start_addr_in_bank < (uint32*)MMA_BANK2_START_ADDR && end_addr_in_bank >= (uint32*)MMA_BANK2_START_ADDR)
    {
        KRNL_LOG_INFO(LOG_SYSTEM, "WARNING: over the boundary of BANK1");
    }
    else if (start_addr_in_bank >= (uint32*)MMA_BANK2_START_ADDR && start_addr_in_bank < (uint32*)MMA_BANK3_START_ADDR && end_addr_in_bank >= (uint32*)MMA_BANK3_START_ADDR)
    {
        KRNL_LOG_INFO(LOG_SYSTEM, "WARNING: over the boundary of BANK2");
    }
    else if (start_addr_in_bank >= (uint32*)MMA_BANK3_START_ADDR && start_addr_in_bank < (uint32*)MMA_BANK4_START_ADDR && end_addr_in_bank >= (uint32*)MMA_BANK4_START_ADDR)
    {
        KRNL_LOG_INFO(LOG_SYSTEM, "WARNING: over the boundary of BANK3");
    }
    else if (start_addr_in_bank >= (uint32*)MMA_BANK4_START_ADDR && start_addr_in_bank < (uint32*)MMA_BANK5_START_ADDR && end_addr_in_bank >= (uint32*)MMA_BANK5_START_ADDR)
    {
        KRNL_LOG_INFO(LOG_SYSTEM, "WARNING: over the boundary of BANK4");
    }
    else if (start_addr_in_bank >= (uint32*)MMA_BANK5_START_ADDR && start_addr_in_bank < (uint32*)MMA_BANK6_START_ADDR && end_addr_in_bank >= (uint32*)MMA_BANK6_START_ADDR)
    {
        KRNL_LOG_INFO(LOG_SYSTEM, "WARNING: over the boundary of BANK5");
    }
    else if (start_addr_in_bank >= (uint32*)MMA_BANK6_START_ADDR && start_addr_in_bank < (uint32*)MMA_BANK7_START_ADDR && end_addr_in_bank >= (uint32*)MMA_BANK7_START_ADDR)
    {
        KRNL_LOG_INFO(LOG_SYSTEM, "WARNING: over the boundary of BANK6");
    }
    else if (start_addr_in_bank >= (uint32*)MMA_BANK7_START_ADDR && start_addr_in_bank < (uint32*)MMB_START_ADDR && end_addr_in_bank >= (uint32*)MMB_START_ADDR)
    {
        KRNL_LOG_INFO(LOG_SYSTEM, "WARNING: over the boundary of BANK7");
    }

#endif
}
