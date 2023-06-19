/*
 * @Descripttion: 
 * @version: 
 * @Author: AlonzoChen
 * @Date: 2020-12-30 14:31:23
 * @LastEditors: AlonzoChen
 * @LastEditTime: 2021-03-29 11:04:56
 */
#ifndef __LIBHI_KRNL_PARAM_NETWORK_OBSTACLE_H__
#define __LIBHI_KRNL_PARAM_NETWORK_OBSTACLE_H__

#include "hi_krnl_param.h"
#include "hi_krnl_param_conv2d.h"
#include "hi_krnl_param_conv3s1.h"
#include "hi_krnl_param_conv1s1_dwc3s1_conv1s1_add.h"
#include "hi_krnl_param_conv1s1_dwc3s2_conv1s1.h"
#include "hi_krnl_param_conv1s1_upsmp2x_add.h"
#include "hi_krnl_param_conv3s2_dwc3s1_conv1s1.h"
#include "hi_krnl_param_conv1s1_conv3s1_conv3s1.h"

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus


typedef struct
{
    paramTableConv3s2_dwc3s1_conv1s1_Entry_t        param_blk1;
    paramTableConv1s1_dwc3s2_conv1s1_Entry_t        param_blk2;
    paramTableConv1s1_dwc3s1_conv1s1_add_Entry_t    param_blk3;
    paramTableConv1s1_dwc3s2_conv1s1_Entry_t        param_blk4;
    paramTableConv1s1_dwc3s1_conv1s1_add_Entry_t    param_blk5;
    paramTableConv1s1_dwc3s2_conv1s1_Entry_t        param_blk6;
    paramTableConv1s1_dwc3s1_conv1s1_add_Entry_t    param_blk7;
    paramTableConv1s1_upsmp2x_add_Entry_t           param_blk8;
    paramTableConv1s1_upsmp2x_add_Entry_t           param_blk9;
    paramTableConv1s1_conv3s1_conv3s1_Entry_t       param_blk10;
    paramTableConv1_Entry_t                         param_blk10_1a_s2;
    paramTableConv1_Entry_t                         param_blk10_1a_c3;
    paramTableConv1_Entry_t                         param_blk10_1b_s2;
    paramTableConv1_Entry_t                         param_blk10_1b_r3;
    paramTableConv1s1_conv3s1_conv3s1_Entry_t       param_blk11;
    paramTableConv1_Entry_t                         param_blk11_1a_s2;
    paramTableConv1_Entry_t                         param_blk11_1a_c3;
    paramTableConv1_Entry_t                         param_blk11_1b_s2;
    paramTableConv1_Entry_t                         param_blk11_1b_r3;
    paramTableConv1s1_conv3s1_conv3s1_Entry_t       param_blk12;
    paramTableConv1_Entry_t                         param_blk12_1a_s2;
    paramTableConv1_Entry_t                         param_blk12_1a_c3;
    paramTableConv1_Entry_t                         param_blk12_1b_s2;
    paramTableConv1_Entry_t                         param_blk12_1b_r3;
} paramTable_net_obstacle_Entry_t;

typedef struct
{
    hirtKernelParamTableBase_t infoBase;
    int count;
    paramTable_net_obstacle_Entry_t param;
} paramTable_net_obstacle_t;

#define MULTICORE_PARALLEL      (4)
typedef struct
{
    paramTableConv3s2_dwc3s1_conv1s1_Entry_t        param_blk1[MULTICORE_PARALLEL];
    paramTableConv1s1_dwc3s2_conv1s1_Entry_t        param_blk2[MULTICORE_PARALLEL];
    paramTableConv1s1_dwc3s1_conv1s1_add_Entry_t    param_blk3[MULTICORE_PARALLEL];
    paramTableConv1s1_dwc3s2_conv1s1_Entry_t        param_blk4[MULTICORE_PARALLEL];
    paramTableConv1s1_dwc3s1_conv1s1_add_Entry_t    param_blk5[MULTICORE_PARALLEL];
    paramTableConv1s1_dwc3s2_conv1s1_Entry_t        param_blk6[MULTICORE_PARALLEL];
    paramTableConv1s1_dwc3s1_conv1s1_add_Entry_t    param_blk7[MULTICORE_PARALLEL];
    paramTableConv1s1_upsmp2x_add_Entry_t           param_blk8[MULTICORE_PARALLEL];
    paramTableConv1s1_upsmp2x_add_Entry_t           param_blk9[MULTICORE_PARALLEL];
    paramTableConv1s1_conv3s1_conv3s1_Entry_t       param_blk10[MULTICORE_PARALLEL];
    paramTableConv3s1_Entry_t                       param_blk10_1a_s2[MULTICORE_PARALLEL];
    paramTableConv3s1_Entry_t                       param_blk10_1a_c3[MULTICORE_PARALLEL];
    paramTableConv3s1_Entry_t                       param_blk10_1b_s2[MULTICORE_PARALLEL];
    paramTableConv3s1_Entry_t                       param_blk10_1b_r3[MULTICORE_PARALLEL];
    paramTableConv1s1_conv3s1_conv3s1_Entry_t       param_blk11[MULTICORE_PARALLEL];
    paramTableConv3s1_Entry_t                       param_blk11_1a_s2[MULTICORE_PARALLEL];
    paramTableConv3s1_Entry_t                       param_blk11_1a_c3[MULTICORE_PARALLEL];
    paramTableConv3s1_Entry_t                       param_blk11_1b_s2[MULTICORE_PARALLEL];
    paramTableConv3s1_Entry_t                       param_blk11_1b_r3[MULTICORE_PARALLEL];
    paramTableConv1s1_conv3s1_conv3s1_Entry_t       param_blk12[MULTICORE_PARALLEL];
    paramTableConv3s1_Entry_t                       param_blk12_1a_s2[MULTICORE_PARALLEL];
    paramTableConv3s1_Entry_t                       param_blk12_1a_c3[MULTICORE_PARALLEL];
    paramTableConv3s1_Entry_t                       param_blk12_1b_s2[MULTICORE_PARALLEL];
    paramTableConv3s1_Entry_t                       param_blk12_1b_r3[MULTICORE_PARALLEL];
} paramTable_net_obstacle_tiling_Entry_t;

typedef struct
{
    hirtKernelParamTableBase_t infoBase;
    int count;
    paramTable_net_obstacle_tiling_Entry_t param;
} paramTable_net_obstacle_tiling_t;

#ifdef __cplusplus
}
#endif
#endif /*__LIBHI_KRNL_PARAM_NETWORK_OBSTACLE_H__*/
