/*
 * @Descripttion:
 * @version:
 * @Author: AlonzoChen
 * @Date: 2020-12-30 14:31:23
 * @LastEditors: AlonzoChen
 * @LastEditTime: 2021-08-30 16:02:33
 */
#ifndef __LIBHI_KRNL_PARAM_H__
#define __LIBHI_KRNL_PARAM_H__

#include "hi_addr_def.h"
#include "hisdk_config.h"
#include "hisdk_type.h"

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

#define TILING_NONE        (100)
#define TILING_PART_MIDDLE (0)
#define TILING_PART_LEFT   (1)
#define TILING_PART_RIGHT  (2)

typedef struct {
    u32_t op_type;
    u32_t table_size;
    u32_t task_dim; // the number of cores involved in this task
    u32_t task_cores[HIPU200_SOC_CORE_NUM]; // the ID of each core, for example:
                                            // multi layers conv: Table[0] is
                                            // head, Table[1] is tail,
                                            // Table[2,3,...] are body
} hirtKernelParamTableBase_t;

typedef struct {
    uint8  x_pos;
    uint8  y_pos;
    uint32 lcaddr;
} hikl_addr_t;

/*solo operator*/
#define KERNEL_OP_RELU                                    (1000)
#define KERNEL_OP_CONV2D                                  (1001)
#define KERNEL_OP_BN                                      (1034)
#define KERNEL_OP_VADD                                    (1003)
#define KERNEL_OP_DWCONV                                  (1004)
#define KERNEL_OP_CONV3S2_DWC3S1_CONV1S1                  (1005)
#define KERNEL_OP_CONV1S1_DWC3S2_CONV1S1                  (1006)
#define KERNEL_OP_CONV1S1_DWC3S1_CONV1S1_ADD              (1007)
#define KERNEL_OP_CONV1S1_UPSMP2X_ADD                     (1008)
#define KERNEL_OP_CONV1S1_CONV3S1_CONV3S1                 (1009)
#define KERNEL_OP_NET_TRAFFICLIGHT                        (1010)
#define KERNEL_OP_NET_OBSTACLE                            (1011)
#define KERNEL_OP_NET_LANEDET                             (1012)
#define KERNEL_OP_FC                                      (1013)
#define KERNEL_OP_CONV1S1_DWC3S1_CONV1S1_ADD_V2           (1014)
#define KERNEL_OP_CONV1S1_DWC3S2_CONV1S1_V2               (1015)
#define KERNEL_OP_CONV3S1_MULTICORE                       (1016)
#define KERNEL_OP_CONV3S1                                 (1017)
#define KERNEL_OP_CONV1S1_DWC3S1_CONV1S1_ADD_MULTICORE    (1018)
#define KERNEL_OP_CONV1S1_DWC3S1_CONV1S1_ADD_V2_MULTICORE (1019)
#define KERNEL_OP_CONV1S1_DWC3S2_CONV1S1_MULTICORE        (1020)
#define KERNEL_OP_CONV1S1_DWC3S2_CONV1S1_V2_MULTICORE     (1021)
#define KERNEL_OP_CONV3S2_DWC3S1_CONV1S1_MULTICORE        (1022)
#define KERNEL_OP_CONV1S1_UPSMP2X_ADD_MULTICORE           (1023)
#define KERNEL_OP_CONV1S1_CONV3S1_CONV3S1_MULTICORE       (1024)
#define KERNEL_OP_NET_OBSTACLE_1080P_SINGLECORE           (1025)
#define KERNEL_OP_NET_OBSTACLE_1080P_MULTICORE            (1026)
#define KERNEL_OP_GRAPH                                   (1027)
#define KERNEL_OP_NET_OBSTACLE_TILING_SINGLECORE          (1028)
#define KERNEL_OP_NET_OBSTACLE_TILING_MULTICORE           (1029)
#define KERNEL_OP_NET_LANEDET_TILING_SINGLECORE           (1030)
#define KERNEL_OP_NET_LANEDET_TILING_MULTICORE            (1031)
#define KERNEL_OP_NET_OBSTACLE_1080N_SINGLECORE           (1032)
#define KERNEL_OP_NET_OBSTACLE_1080N_MULTICORE            (1033)
#define KERNEL_OP_CONV2D_BYOC                             (1002)
#define KERNEL_OP_DWCONV_BYOC                             (1035)


#define CORE_MARK                                         (1036)

#ifdef __cplusplus
}
#endif
#endif /*__LIBHI_KRNL_PARAM_H__*/
