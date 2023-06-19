/*
 * @Descripttion:
 * @version:
 * @Author: AlonzoChen
 * @Date: 2020-12-30 14:31:23
 * @LastEditors: AlonzoChen
 * @LastEditTime: 2021-08-30 15:58:26
 */
#ifndef __HI_KRNL_PARAM_NDMA_HPU200__H__
#define __HI_KRNL_PARAM_NDMA_HPU200__H__

#include "hi_krnl_param.h"
#ifdef __cplusplus
extern "C" {
#endif // __cplusplus
typedef struct {
    hikl_addr_t rmt_addr;
    uint32_t    mem_size; // memsize, start from rmt_ifm_start_addr_tiled
    uint32_t    lcmem_addr;
} NdmaParamRmtWithLcmem;

#ifdef __cplusplus
}
#endif

#endif //!__HI_KRNL_PARAM_NDMA_HPU200__H__