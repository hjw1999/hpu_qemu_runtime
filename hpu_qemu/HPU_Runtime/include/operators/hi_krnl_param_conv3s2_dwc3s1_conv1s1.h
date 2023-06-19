/*
 * @Descripttion:
 * @version:
 * @Author: AlonzoChen
 * @Date: 2020-12-30 14:31:23
 * @LastEditors: AlonzoChen
 * @LastEditTime: 2021-03-29 11:04:56
 */
#ifndef __LIBHI_KRNL_PARAM_CONV3S2_DWC3S1_CONV1S1_H__
#define __LIBHI_KRNL_PARAM_CONV3S2_DWC3S1_CONV1S1_H__

#include "hi_krnl_param.h"
#include "hi_krnl_param_conv2d.h"

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

typedef struct {
  conv2d_params_t conv1;
  conv2d_params_t conv2;
  conv2d_params_t conv3;

  hikl_addr_t wt_addr_conv1;
  hikl_addr_t bias_addr_conv1;
  hikl_addr_t shift_addr_conv1;
  hikl_addr_t ifm_addr_conv1;
  hikl_addr_t ifm_addr_pleft_conv1;
  hikl_addr_t ifm_addr_pright_conv1;
  uint32_t ifm_addr_linestrd_conv1;
  hikl_addr_t ofm_addr_conv1;

  hikl_addr_t wt_addr_conv2;
  hikl_addr_t bias_addr_conv2;
  hikl_addr_t shift_addr_conv2;
  hikl_addr_t ifm_addr_conv2;
  hikl_addr_t ofm_addr_conv2;

  hikl_addr_t wt_addr_conv3;
  hikl_addr_t bias_addr_conv3;
  hikl_addr_t shift_addr_conv3;
  hikl_addr_t ifm_addr_conv3;
  hikl_addr_t ofm_addr_conv3;
  uint32_t ofm_addr_linestrd_conv3;

  uint32_t tiling_parttype;
} paramTableConv3s2_dwc3s1_conv1s1_Entry_t;

typedef struct {
  hirtKernelParamTableBase_t infoBase;
  int count;
  paramTableConv3s2_dwc3s1_conv1s1_Entry_t param;
} paramTableConv3s2_dwc3s1_conv1s1_t;

#ifdef __cplusplus
}
#endif
#endif /*__LIBHI_KRNL_PARAM_Conv3s2_dwc3s1_CONV1S1_H__*/
