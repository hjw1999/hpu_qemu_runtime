/*
 * @Descripttion: 
 * @version: 
 * @Author: AlonzoChen
 * @Date: 2020-12-29 14:35:14
 * @LastEditors: AlonzoChen
 * @LastEditTime: 2021-05-28 11:03:01
 */
#ifndef LIBLOCK_H
#define LIBLOCK_H

#include "hisdk_type.h"
#include "hihw.h"
#include "libconv.h"
#include "dma.h"

void init_local_lock(local_fm* fm);

// void acquire_remote_lock(remote_fm* fm, uint32 row_num, uint8 x, uint8 y);
// void release_remote_lock(remote_fm* fm, uint32 row_num, uint8 x, uint8 y);
// void acquire_local_lock(local_fm* fm, uint32 row_num);
// void release_local_lock(local_fm* fm, uint32 row_num);

void acquire_remote_lock(uint32* val, uint32 lock_addr, uint8 x, uint8 y);
void release_remote_lock(uint32* val, uint32 lock_addr, uint8 x, uint8 y);
void acquire_local_lock(uint32 lock_addr);
void release_local_lock(uint32 lock_addr);

void wait(int input_var);
void sync(int lock_node_x, int lock_node_y,         \
          int sync_value_pool_idx,                  \
          int lc_value_to_or, int lshift_num, int foot_print_clear_bits, \
          uint32_t sync_value_pool_valid_idxs_num, uint32_t sync_finish_value_for_each_32bit);

void sync_with_core_12_mma(uint32_t lcaddr_start, uint32_t sync_value, int total_sync_cores_num);

#endif /*LIBLOCK_H*/