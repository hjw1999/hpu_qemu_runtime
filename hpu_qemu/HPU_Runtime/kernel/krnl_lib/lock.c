/*
 * @Descripttion: 
 * @version: 
 * @Author: AlonzoChen
 * @Date: 2020-12-29 14:35:14
 * @LastEditors: AlonzoChen
 * @LastEditTime: 2021-05-28 11:10:22
 */
#include "hihw.h"
#include "libconv.h"
#include "dma.h"
#include "lock.h"

#define WAIT_COUNT 15

void acquire_remote_lock(uint32* val, uint32 lock_addr, uint8 x, uint8 y){

	// KRNL_LOG_INFO(LOG_DEBUG, "acquire_remote_lock: lcvar_addr: 0x%x rmt_lock_addr: 0x%x", val, lock_addr);
    *val = 1;
    do{  
        __swp_rmt_amo_var_blocking(val, x, y, lock_addr);
    }while(*val);
}

void release_remote_lock(uint32* val, uint32 lock_addr, uint8 x, uint8 y){
	// KRNL_LOG_INFO(LOG_DEBUG, " release_remote_lock: lcvar_addr: 0x%x rmt_lock_addr: 0x%x", val, lock_addr);
    *val = 0;
    __swp_rmt_amo_var_blocking(val, x, y, lock_addr);
}

void acquire_local_lock(uint32 lock_addr){
	// KRNL_LOG_INFO(LOG_DEBUG, " acquire_local_lock local_lock_addr: 0x%x", lock_addr);
    int state = 1;
    do{  
        asm volatile("amoswap.w.aq %0, %0, (%1)":"+r"(state):"r"(lock_addr));
    }while(state);
}

void release_local_lock(uint32 lock_addr){
	// KRNL_LOG_INFO(LOG_DEBUG, " release_local_lock local_lock_addr: 0x%x", lock_addr);
    int state = 0;
    asm volatile("amoswap.w.rl %0, %0, (%1)":"+r"(state):"r"(lock_addr));
}

void wait(int input_var){
    asm volatile(
                    "mv a0, %0\n"
                    "li a1, 0\n"
                    "ag: nop\n"
                    "addi a0, a0, -1\n"
                    "bne a0, a1, ag\n"
                    :
                    :"r"(input_var)
                    :
                );
}

static const uint8_t nocNodeXYTable[] =
{
    HIPU200_NOC_NODEADDR_COR(0),
    HIPU200_NOC_NODEADDR_COR(1),
    HIPU200_NOC_NODEADDR_COR(2),
    HIPU200_NOC_NODEADDR_COR(3),
    HIPU200_NOC_NODEADDR_COR(4),
    HIPU200_NOC_NODEADDR_COR(5),
    HIPU200_NOC_NODEADDR_COR(6),
    HIPU200_NOC_NODEADDR_COR(7),
    HIPU200_NOC_NODEADDR_COR(8),
    HIPU200_NOC_NODEADDR_COR(9),
    HIPU200_NOC_NODEADDR_COR(10),
    HIPU200_NOC_NODEADDR_COR(11),
    HIPU200_NOC_NODEADDR_COR(12)
};
//lc_value_to_or, 当前hpu core完成任务的执行后，要在lock_node 更新自己的状态所用的状态值
//sync_finish_value, 更新完当前hpu core的状态后，完成sync的总状态检查值
//lock_node_x, lock_node_y, 存储sync 状态的node 的noc 地址
#if 0
void sync(int lock_node_x, int lock_node_y, int lc_value_to_or, int lshift_num, int foot_print_clear_bits, int sync_finish_value){
    //judge whether lock_node is current core
	KRNL_LOG_INFO(LOG_DEBUG, "sync entered, lc_value_to_or: 0x%x << %d, cur_sync_value &= 0x%08x sync_finish_value: 0x%x", lc_value_to_or, lshift_num, foot_print_clear_bits, sync_finish_value);
    amo_struct *amo_mem = (amo_struct *)AMO_ADDR_S;
    unsigned int _coreid = amo_mem->_coreid;
    volatile int sync_value = 0;
    // amo_mem->sync_value = 0;
    bool b_sync_value_updated = false;
    if (nocNodeXYTable[_coreid] == HIPU200_NOC_MAKENMAP(lock_node_x, lock_node_y))
    {
        do{
            wait(WAIT_COUNT);
            acquire_local_lock((uint32)&amo_mem->lock);
	        KRNL_LOG_INFO(LOG_DEBUG, "local lock locked: amo_mem->sync_value 0x%x, with b_sync_value_updated: %d", amo_mem->sync_value,  b_sync_value_updated);
            //current core is the lock_node, use acquire_local_lock
            if (b_sync_value_updated == false){
                amo_mem->sync_value &= foot_print_clear_bits;
                // asm volatile("amoor.w %0, %1, (%2)":"+r"(sync_value), "+r"(lc_value_to_or):"r"(&amo_mem->sync_value));
                amo_mem->sync_value |= lc_value_to_or << lshift_num;
                b_sync_value_updated = true;
            }
            sync_value = amo_mem->sync_value;
            release_local_lock((uint32)&amo_mem->lock);
	        KRNL_LOG_INFO(LOG_DEBUG, "local lock released: sync_value: 0x%x", sync_value);
        }while(sync_value != sync_finish_value);
    }
    else 
    {
        int temp_value = 0;
        //use acquire_remote_lock
        do{
            temp_value = 0;
            wait(WAIT_COUNT);
            acquire_remote_lock((uint32 *)&amo_mem->lock, (uint32)&((amo_struct *)AMO_ADDR_S)->lock , lock_node_x, lock_node_y);
	        KRNL_LOG_INFO(LOG_DEBUG, "remote lock locked, b_sync_value_updated: %d", b_sync_value_updated);
            if (b_sync_value_updated == true){
                __swp_rmt_amo_var_blocking((uint32 *)&amo_mem->sync_value, lock_node_x, lock_node_y, (uint32)&((amo_struct *)AMO_ADDR_S)->sync_value);
                temp_value = amo_mem->sync_value;
                __swp_rmt_amo_var_blocking((uint32 *)&amo_mem->sync_value, lock_node_x, lock_node_y, (uint32)&((amo_struct *)AMO_ADDR_S)->sync_value);
            }
            else{
                __swp_rmt_amo_var_blocking((uint32 *)&amo_mem->sync_value, lock_node_x, lock_node_y, (uint32)&((amo_struct *)AMO_ADDR_S)->sync_value);
                amo_mem->sync_value &= foot_print_clear_bits;
                amo_mem->sync_value |= lc_value_to_or << lshift_num;
                temp_value = amo_mem->sync_value;
                __swp_rmt_amo_var_blocking((uint32 *)&amo_mem->sync_value, lock_node_x, lock_node_y, (uint32)&((amo_struct *)AMO_ADDR_S)->sync_value);
                b_sync_value_updated = true;
            }
            release_remote_lock((uint32 *)&amo_mem->lock, (uint32)&((amo_struct *)AMO_ADDR_S)->lock , lock_node_x, lock_node_y);
	        KRNL_LOG_INFO(LOG_DEBUG, "remote lock released, temp_value: 0x%x", temp_value);
        }while(temp_value != sync_finish_value);
    }
}

#endif

// #define SYNC_VALUE_POOL_SIZE                (8)
// typedef struct{
//     uint8 _coreid:4;
//     uint8 busy_ndma_channel_count:4;
//     uint8 reserved2;
//     uint16_t reserved3;
//     uint32_t lock;                      //lock by ndma swap
//     uint32_t sync_value_pool[SYNC_VALUE_POOL_SIZE];
//     exception_reserved exception_reserved;
//     uint32_t return_code;               //HIPU200_KNL_RTCODE_ADDR
//     uint32_t param_table;               //HIPU200_KNL_PTABLE_ADDR
// }amo_struct;;


void sync(int lock_node_x, int lock_node_y,         \
          int sync_value_pool_idx,                  \
          int lc_value_to_or, int lshift_num, int foot_print_clear_bits, \
          uint32_t sync_value_pool_valid_idxs_num, uint32_t sync_finish_value_for_each_32bit){
    //judge whether lock_node is current core
    amo_struct *amo_mem = (amo_struct *)AMO_ADDR_S;
    unsigned int _coreid = amo_mem->_coreid;
    bool b_sync_value_updated = false;
    bool b_sync_finished = false;
    if (nocNodeXYTable[_coreid] == HIPU200_NOC_MAKENMAP(lock_node_x, lock_node_y))
    {
        do{
	        KRNL_LOG_INFO(LOG_DEBUG, "sync entered, lc_value_to_or: 0x%x << %d, cur_sync_value &= 0x%08x sync_finish_value: 0x%x", lc_value_to_or, lshift_num, foot_print_clear_bits, sync_finish_value_for_each_32bit);
            wait((WAIT_COUNT + sync_value_pool_idx) * 4);
            b_sync_finished = true;
            acquire_local_lock((uint32)&amo_mem->lock);
	        KRNL_LOG_INFO(LOG_DEBUG, "local lock locked: amo_mem->sync_value_pool[sync_value_pool_idx] 0x%x, with b_sync_value_updated: %d", amo_mem->sync_value_pool[sync_value_pool_idx],  b_sync_value_updated);
            //current core is the lock_node, use acquire_local_lock
            if (b_sync_value_updated == false){
                amo_mem->sync_value_pool[sync_value_pool_idx] &= foot_print_clear_bits;
                amo_mem->sync_value_pool[sync_value_pool_idx] |= lc_value_to_or << lshift_num;
                b_sync_value_updated = true;
	            KRNL_LOG_INFO(LOG_DEBUG, "local update, temp_value: 0x%x", amo_mem->sync_value_pool[sync_value_pool_idx]);
            }
            for (int i = 0; i < sync_value_pool_valid_idxs_num; i++)
            {
                //check whether each sync_value meet the sync finish requirement
                if (amo_mem->sync_value_pool[i] != sync_finish_value_for_each_32bit)
                {
                    b_sync_finished = false;
                    break;
                }
            }

            release_local_lock((uint32)&amo_mem->lock);
	        KRNL_LOG_INFO(LOG_DEBUG, "local lock released");
        }while(!b_sync_finished);
    }
    else 
    {
        int temp_value = 0;
        do{
	        KRNL_LOG_INFO(LOG_DEBUG, "sync entered, lc_value_to_or: 0x%x << %d, cur_sync_value &= 0x%08x sync_finish_value: 0x%x", lc_value_to_or, lshift_num, foot_print_clear_bits, sync_finish_value_for_each_32bit);
            wait((WAIT_COUNT + sync_value_pool_idx) * 4);
            b_sync_finished = true;
            acquire_remote_lock((uint32 *)&amo_mem->lock, (uint32)&((amo_struct *)AMO_ADDR_S)->lock , lock_node_x, lock_node_y);
	        KRNL_LOG_INFO(LOG_DEBUG, "remote lock locked, b_sync_value_updated: %d", b_sync_value_updated);
            if (b_sync_value_updated == false){
                __swp_rmt_amo_var_blocking((uint32 *)&amo_mem->sync_value_pool[sync_value_pool_idx], lock_node_x, lock_node_y, (uint32)&((amo_struct *)AMO_ADDR_S)->sync_value_pool[sync_value_pool_idx]);
                amo_mem->sync_value_pool[sync_value_pool_idx] &= foot_print_clear_bits;
                amo_mem->sync_value_pool[sync_value_pool_idx] |= lc_value_to_or << lshift_num;
                temp_value = amo_mem->sync_value_pool[sync_value_pool_idx];
                __swp_rmt_amo_var_blocking((uint32 *)&amo_mem->sync_value_pool[sync_value_pool_idx], lock_node_x, lock_node_y, (uint32)&((amo_struct *)AMO_ADDR_S)->sync_value_pool[sync_value_pool_idx]);
                b_sync_value_updated = true;
	            KRNL_LOG_INFO(LOG_DEBUG, "remote update, temp_value: 0x%x", temp_value);
            }
            for (int i = 0; i < sync_value_pool_valid_idxs_num; i++)
            {
                __swp_rmt_amo_var_blocking((uint32 *)&amo_mem->sync_value_pool[i], lock_node_x, lock_node_y, (uint32)&((amo_struct *)AMO_ADDR_S)->sync_value_pool[i]);
                temp_value = amo_mem->sync_value_pool[i];
                __swp_rmt_amo_var_blocking((uint32 *)&amo_mem->sync_value_pool[i], lock_node_x, lock_node_y, (uint32)&((amo_struct *)AMO_ADDR_S)->sync_value_pool[i]);
                if (temp_value != sync_finish_value_for_each_32bit)
                {
                    b_sync_finished = false;
                    break;
                }
            }

            release_remote_lock((uint32 *)&amo_mem->lock, (uint32)&((amo_struct *)AMO_ADDR_S)->lock , lock_node_x, lock_node_y);
	        KRNL_LOG_INFO(LOG_DEBUG, "remote lock released, temp_value: 0x%x", temp_value);
        }while(!b_sync_finished);
    }
}

void sync_with_core_12_mma(uint32_t lcaddr_start, uint32_t sync_value, int total_sync_cores_num){
    //judge whether lock_node is current core
    amo_struct *amo_mem = (amo_struct *)AMO_ADDR_S;
    unsigned int _coreid = amo_mem->_coreid;
    bool b_sync_value_updated = false;
    bool b_sync_finished = false;
    uint32_t * lc_sync_addr = (uint32_t *)&((sync_struct_in_mma *)lcaddr_start)->sync_items[_coreid];
    *lc_sync_addr = sync_value;
    do{
        wait(WAIT_COUNT);
        b_sync_finished = true;
        if(b_sync_value_updated == false){
	        KRNL_LOG_INFO(LOG_NDMA, "update core12 by core%d at remote addr: 0x%x with value %d", _coreid, lc_sync_addr, sync_value);
            __wr_to_remote_chunk_blocking((uint32 *)lc_sync_addr, HIPU200_NOC_COR12_X, HIPU200_NOC_COR12_Y, (uint32)lc_sync_addr, GMEM_ALIGN_SIZE);
            b_sync_value_updated = true;
        }
        __rd_from_remote_chunk_blocking((uint32 *)lcaddr_start, HIPU200_NOC_COR12_X, HIPU200_NOC_COR12_Y, (uint32)lcaddr_start, sizeof(sync_struct_in_mma));
	    // KRNL_LOG_INFO(LOG_NDMA, "12th core sync mem with sync_value %d: ", sync_value);
        for(int i = 0; i < total_sync_cores_num; i ++)
        {
            uint32_t *lc_sync_addr_to_check = (uint32_t *)&((sync_struct_in_mma *)lcaddr_start)->sync_items[i];
	        // KRNL_LOG_INFO(LOG_NDMA, "core%d sync_value: %d", i, *lc_sync_addr_to_check);
            if (*lc_sync_addr_to_check < sync_value)
            {
                b_sync_finished = false;
                break;
            }
        }

    }while(!b_sync_finished);
}