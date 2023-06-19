/*
 * @Descripttion:
 * @version:
 * @Author: AlonzoChen
 * @Date: 2020-12-30 14:31:23
 * @LastEditors: AlonzoChen
 * @LastEditTime: 2021-08-31 15:05:36
 */
#ifndef __LIBHI_KRNL_PARAM_CONV2D_V2_H__
#define __LIBHI_KRNL_PARAM_CONV2D_V2_H__

#include "hpu200/operators/hi_krnl_param_ndma_hpu200.h"
#include "hpu200/operators/hi_krnl_param_sync_hpu200.h"
#include "operators/hi_krnl_param_conv2d.h"




#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

enum TilingIfmWPartType {
    kTilingIfmWLeft   = 0,
    kTilingIfmWMiddle = 1,
    kTilingIfmWRight  = 2
};
enum TilingIfmHPartType {
    kTilingIfmHTop    = 0,
    kTilingIfmHMiddle = 1,
    kTilingIfmHBottom = 2
};
typedef struct {
    int w; // tiling coordinate, if == -1: no tiling
    int h; // tiling coordinate, if == -1: no tiling
    int c; // tiling coordinate, if == -1: no tiling
    enum TilingIfmWPartType tiling_ifm_w_partype;
    enum TilingIfmHPartType tiling_ifm_h_partype;
    // uint32_t ofm_h_tiled;        // ifm_h after tiling
    uint32_t ifm_row_size_tiled; // ifm_w * ifm_c 64B after tiling, maybe n + 1
                                 // + supplement_1
    uint32_t ofm_row_size_tiled;     
    conv2d_params_t conv2d_for_block;
    // uint32_t ifm_c_group8_num_tiled;  // ifm_c_group8_num after tiling
    // uint32_t kernel_group8_num_tiled; // ofm_c_group8_num after tiling
} TilingInfo;

typedef struct {
    hikl_addr_t bias_rmt_addr;
    uint32_t    bias_mem_size; 
    hikl_addr_t shift_rmt_addr;
    uint32_t    shift_mem_size; 
    hikl_addr_t wt_rmt_addr;
    uint32_t    wt_mem_size; 
    hikl_addr_t ifm_rmt_addr;
    uint32_t    ifm_mem_size; 
    hikl_addr_t ofm_rmt_addr;
    uint32_t    ofm_mem_size; 
}Tilingwithdev;

typedef struct {
    // for example: kernel 3 x 3
    // w_tiling_left: n + 1 + supplement_1
    // w_tiling_middle: 1 + n + 1
    // w_tiling_right: supplement_1 + 1 + n
    // n + 1 or 1 + n + 1 or 1 + n is one continuous block memory
    uint32_t has_supplement; // 0: no supplement, 1: has supplement
    NdmaParamRmtWithLcmem ifm_continuous_row_tiled_info;
    NdmaParamRmtWithLcmem ifm_row_supplement_info;
} TilingIfmRowInfo;//args for each prefetch row

typedef struct {
    uint32_t prefetch_row_count;
    uint32_t prefetch_enable;
    // maybe h_strd, for kernel 3x3, top row, it is 2
    // TilingIfmRowInfo ifm_row_tiled[2]; // the account is  prefetch_row_count
} TilingIfmPrefetchInfo;//prefetch row args for each row

typedef struct {
    uint32_t h_iter_tiled;                              // cur ifm h_iter in tiling area
    TilingIfmPrefetchInfo ifm_prefetch_info;
    uint32_t param_mmac_cluster_start;                  // cluster_start, unit is 64B. set with
                                                        // tiling info
    uint32_t param_mmac_cluster_end;                    // cluster_end, unit is 64B. set with
                                                        // tiling info
    uint32_t param_mmac_cluster_num;                    // k_h - 1, set with tiling info
    uint32_t ifm_start_addr_tiled_for_instructions;     // set with tiling info
    uint32_t wt_start_addr_tiled_for_instructions;      // set with tiling info
    uint32_t need_to_xfr_ofm;                           // 0: no need to xfer ofm to rmt, 1: need
    hikl_addr_t rmt_ofm_addr_tiled;
    uint32_t    one_row_ofm_size;
    SyncPrimitive sync_primitive;

    // uint32_t ifm_w_offset;           // tiling_left: ifm_c >> 3 * pad_l
    // tiling_right: - ifm_c >> c * pad_r
    // uint32_t bias_start_addr_tiled_for_instructions;  // set with tiling info
    // uint32_t shift_start_addr_tiled_for_instructions; // set with tiling info
    // uint32_t ofm_start_addr_tiled_for_instructions; // output feature map start
    //                                                 // address, unit is 64B.
    // uint32_t st_ofm_w_64b; // hw_ofm_w + delta_w_iter / stride_w; //!!!now
    // only 1s1 has delta_w_iter == 1 or 2, other conv delta_w_iter = 0
} PTConv2dOneRowHPU200;//for each row in one tiling block

typedef struct {
    TilingInfo tiling_info;                 //this part contains args which are irrelavent to the dev

    Tilingwithdev Tilingwithdev;            //this part contains args which are relavent to the dev

    
    // uint32_t input_conv_type; // 3 processing types: 1: CONV, 2: DWC, 3: FC
    // uint32_t param_mmac_ifm_blk_stride;     // ifm_c_group8_num * w_dilat; 64B
    // // dwc: 0,  classic_conv: ifm_c_group8_num - 1
    // uint32_t param_mmac_ifm_blk_size;
    // uint32_t param_mmac_ifm_blk_num;       // k_w - 1
    // uint32_t param_mmac_wt_cluster_stride; // ifm_c * conv2d_param->cshape.k_w *
    //                                        // MTX_SCALE; 64B
    // uint32_t param_mmac_wt_blk_stride;     // ifm_c_group8_num
    // hikl_addr_t rmt_bias_addr; // remote bias addr, maybe in DDR, maybe in
    // other
    //                            // core's lcmem
    // uint32_t bias_lcmem_start_addr; // bias start address, unit is 64B.
    // hikl_addr_t rmt_shift_addr;     // remote shift addr, maybe in DDR, maybe
    // in
    //                                 // other core's lcmem
    // uint32_t shift_lcmem_start_addr; // shift data start address, unit is
    // 64B. hikl_addr_t rmt_wt_addr; // remote shift addr, maybe in DDR, maybe
    // in other
    //                          // core's lcmem
    // uint32_t wt_lcmem_start_addr; // weight start address, unit is 64B. set
    // with
    //                               // tiling info

} PTConv2dTiledHPU200_host;//common args in one tiling block
typedef struct {
    uint32_t param_mmac_mtx_pad_type;
    uint32_t param_mmac_region_start;                           // region start address, unit is 64B.
    uint32_t param_mmac_region_end;                             // region end address, unit is 64B.
    uint32_t param_mmac_ifm_cluster_stride;                     // cluster_stride, unit is 64B.

    uint32_t bias_start_addr_tiled_for_instructions;            // set with tiling info
    uint32_t shift_start_addr_tiled_for_instructions;           // set with tiling info
    uint32_t ofm_start_addr_tiled_for_instructions;             // output feature map start
                                                                // address, unit is 64B.
    uint32_t    bias_lcmem_addr;
    uint32_t    shift_lcmem_addr;
    uint32_t    wt_lcmem_addr;

    // uint32_t input_conv_type; // 3 processing types: 1: CONV, 2: DWC, 3: FC
    // uint32_t param_mmac_ifm_blk_stride;     // ifm_c_group8_num * w_dilat; 64B
    // // dwc: 0,  classic_conv: ifm_c_group8_num - 1
    // uint32_t param_mmac_ifm_blk_size;
    // uint32_t param_mmac_ifm_blk_num;       // k_w - 1
    // uint32_t param_mmac_wt_cluster_stride; // ifm_c * conv2d_param->cshape.k_w *
    //                                        // MTX_SCALE; 64B
    // uint32_t param_mmac_wt_blk_stride;     // ifm_c_group8_num

}PTConv2dTiledHPU200_dev;
typedef struct {
    int core_id;                                    // which hpu core this pt_table for
    PTConv2dTiledHPU200_host tiled_param_host;      //caculated on the host
    PTConv2dTiledHPU200_dev tiled_param_dev;        //caculated on the dev
    // hikl_addr_t ifm_addr;
    // hikl_addr_t ifm_addr_pleft;
    // hikl_addr_t ifm_addr_pright;
    // uint32_t ifm_addr_linestrd;
    // uint32_t ofm_addr_linestrd;

    // uint32_t tiling_parttype;
    // the array num is
    // tiled_param.tiling_info.ifm_h_tiled
    //PTConv2dOneRowHPU200 one_ofm_row_param[80];
} PTConv2dEntryHPU200;//for each tiling block

typedef struct {
    hirtKernelParamTableBase_t infoBase;
    conv2d_params_t conv2d_whole;
    int count; // tiling count tiling_info.w * tiling_info.h *  tiling_info.c or
               // cascade conv2d count

    PTConv2dEntryHPU200 param[0];
} PTConv2ds;

#ifdef __cplusplus
}
#endif
#endif /*__LIBHI_KRNL_PARAM_CONV2D_V2_H__*/
