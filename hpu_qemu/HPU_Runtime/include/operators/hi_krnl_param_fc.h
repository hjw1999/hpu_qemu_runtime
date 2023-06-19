/*
 * @Descripttion: 
 * @version: 
 * @Author: AlonzoChen
 * @Date: 2021-05-05 18:15:48
 * @LastEditors: AlonzoChen
 * @LastEditTime: 2021-05-10 18:34:11
 */
#ifndef __LIBHI_KRNL_PARAM_FC_H__
#define __LIBHI_KRNL_PARAM_FC_H__

#include "hi_krnl_param.h"
#include "hi_krnl_param_conv2d.h"

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

typedef struct
{
    paramTableConv2d_Entry_t fc_param;
    bool b_connect_with_conv;
    conv2d_params_t pre_conv2d;
} paramTableFc_Entry_t;

typedef struct
{
    hirtKernelParamTableBase_t infoBase;
    paramTableFc_Entry_t param;
} paramTableFc_t;


#ifdef __cplusplus
}
#endif
#endif /*__LIBHI_KRNL_PARAM_FC_H__*/
