
/*
 * @Descripttion:
 * @version:
 * @Author: wangyz
 * @Date:
 * @LastEditors: AlonzoChen
 * @LastEditTime: 2021-08-31 15:35:33
 */
#ifndef __LIBHI_KRNL_PARAM_CONV1S1_DWC3S1_CONV1S1_ADD_H__
#define __LIBHI_KRNL_PARAM_CONV1S1_DWC3S1_CONV1S1_ADD_H__

#include "hi_krnl_param.h"
#include "hi_krnl_param_add.h"
#include "hi_krnl_param_conv2d.h"
#include "hpu200/operators/hi_krnl_param_conv2d_hpu200.h"

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

typedef struct {
    int core_id; // which hpu core this pt_table for
    conv2d_params_t conv_a_whole;
    conv2d_params_t conv_b_whole;
    conv2d_params_t conv_c_whole;
    add_params_t add1;

    PTConv2dTiledHPU200 tiled_param_conv_a;
    PTConv2dTiledHPU200 tiled_param_conv_b;
    PTConv2dTiledHPU200 tiled_param_conv_c;
    PTConv2dOneRowHPU200 one_ofm_row_param_conv_a[0];
    PTConv2dOneRowHPU200 one_ofm_row_param_conv_b[0];
    PTConv2dOneRowHPU200 one_ofm_row_param_conv_c[0];

} PTConv1s1Dwc3s1Conv1s1AddEntryHPU200;

typedef struct {
    hirtKernelParamTableBase_t infoBase;
    int count;
    PTConv1s1Dwc3s1Conv1s1AddEntryHPU200 param[0];
} PTConv1s1Dwc3s1Conv1s1AddHPU200;

#ifdef __cplusplus
}
#endif
#endif /*__LIBHI_KRNL_PARAM_CONV1S1_DWC3S1_CONV1S1_ADD_H__*/
