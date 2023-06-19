/*
 * @Descripttion:
 * @version:
 * @Author: AlonzoChen
 * @Date: 2020-12-30 14:31:23
 * @LastEditors: AlonzoChen
 * @LastEditTime: 2021-09-02 10:14:58
 */
#ifndef __HI_KRNL_PARAM_SYNC_HPU200__H__
#define __HI_KRNL_PARAM_SYNC_HPU200__H__

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus
// for sync hpu cores, not dependency in one core
typedef struct {
    uint32_t need_sync; // 0: no need sync; 1: need sync
    uint32_t lcaddr_start;
    uint32_t sync_value;
    uint32_t total_sync_cores_num;
} SyncPrimitive;

#ifdef __cplusplus
}
#endif

#endif //!__HI_KRNL_PARAM_SYNC_HPU200__H__