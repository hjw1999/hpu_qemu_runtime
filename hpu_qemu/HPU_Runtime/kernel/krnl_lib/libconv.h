/*
 * @Descripttion:
 * @version:
 * @Author: AlonzoChen
 * @Date: 2020-12-29 14:35:14
 * @LastEditors: AlonzoChen
 * @LastEditTime: 2021-05-16 19:46:51
 */
#ifndef LIBCONV_H
#define LIBCONV_H

#define OPT_LIBCONV
#define OPT_ONE_ROW_CONV

#include "hisdk_type.h"
#include "operators/hi_krnl_param_conv2d.h"

typedef struct{
    uint32 value;
    uint32 reserved[7];
}slot_var_in_local_variable;

typedef struct{
    uint32 start_addr;
    uint32 end_addr;
    uint32 size;            // in 32-Bytes
    uint32 cur_cnt;
}local_variable;

typedef struct{
    uint32 start_addr;
    uint32 end_addr;
    uint32 row_slots_num;         // in rows
    uint32 row_size;        // in Bytes
    uint32 total_cnt;       // record the number of previous remote allocation
} base_fm;

typedef struct {
    base_fm bfm;
    uint32 *row_slot_available_flgs;
    uint32 cur_cnt;         // how many valid rows are in current buffer
    uint32 cur_valid_idx;   // the index of current first valid row (range: 0 -> row_slots_num-1)
}local_fm;

typedef struct {
    base_fm bfm;
    hikl_addr_t var_in_remote_node_saving_flags;  // base address of flags of remote node
    uint32 *local_var_for_interact_with_remote_vars;     // a local var to interact with var of remote node
}remote_fm;

typedef struct {
    base_fm bfm;
    uint8   x_pos;
    uint8   y_pos;
}ddr_fm;

uint32 is_ifm_bottom(uint32 i, conv_shape_t *cshape, pad_shape_t *pshape);
uint32 is_ifm_top(uint32 i, conv_shape_t *cshape, pad_shape_t *pshape);
uint32 get_ifm_row_size(conv_shape_t* p_cshape, stride_shape_t *p_strd_shape);
uint32 get_ofm_row_size(conv_shape_t* p_cshape, stride_shape_t *p_strd_shape);

void init_local_var(local_variable *lvar, uint32 start_addr, uint32 end_addr, uint32 size);
uint32 alloc_var(local_variable *v, uint32 var_num);
void init_base_fm(base_fm* fm, uint32 row_slots_num, uint32 row_size, uint32 start);
uint32 alloc_fm(base_fm *fm, uint32 *idx);
void init_ddr_fm(ddr_fm *fm, uint32 row_slots_num, uint32 row_size, uint32 start, uint8 x, uint8 y);
uint32 alloc_ddr_fm(ddr_fm *fm);
void init_local_fm(local_fm* fm, local_variable* lvar, uint32 local_row_slots, uint32 row_size, uint32 start);
uint32 alloc_local_fm(local_fm *fm, uint32 num);
void dealloc_local_fm(local_fm *fm, uint32 num);
uint32 get_cur_valid_local_fm_addr(local_fm *fm);
void update_local_fm(local_fm *fm);
void init_remote_fm(remote_fm* fm, local_variable* lvar, uint32 row_slots_num, uint32 row_size, uint32 start, uint8 x, uint8 y, uint32 flag_addr);
uint32 alloc_remote_fm(remote_fm *fm, uint32 num);
void notify_remote_fm_ready(remote_fm *fm, uint32 fm_addr);

void init_local_fm_lock(local_fm* fm);
void acquire_local_fm_lock(uint32 row_slots_num);
void release_local_fm_lock(uint32 row_slots_num);
void acquire_remote_fm_lock(remote_fm* fm, uint32 row_slots_num);
void release_remote_fm_lock(remote_fm* fm, uint32 row_slots_num);
void acquire_flag_in_remote_node_blocking(remote_fm *fm, uint32 row_slots_num);
int check_flag_in_remote_node(remote_fm *fm, uint32 row_slots_num);
int _ndma_one_fm_row_from_localmem_to_remote_localmem_blocking(local_fm *ofm, remote_fm *rfm);
int _ndma_one_fm_row_from_localmem_to_ddr_blocking(local_fm *ofm, ddr_fm *dfm);
int _ndma_remain_ifm_rows_from_localmem_to_ddr_blocking(local_fm *ofm, ddr_fm *dfm);


void _set_mmac_vfadd_param_();
void __intrinsic_func_layout_for_fc_input_with_c8__(uint32 ifm, uint32 ofm, uint32 fm_h, uint32 fm_w);
void __intrinsic_func__(uint32 ifm, uint32 wt, uint32 ofm, uint32 bias_start, uint32 shift_start, uint32 relu, int conv_type, uint32_t b_with_bias_shift);
// void clear_static_var();
#ifdef OPT_ONE_ROW_CONV
void one_row_conv(
    int h_iter,                             // if h_iter == 0, should padding zero line on top.
    uint32_t tiling_parttype,
    uint32_t delta_w_iter, //得从外面传一个3s1还是3s2的信息进去，3s2的话是只+1的
    conv2d_params_t* conv2d_param,          // kernel shape.
    uint32_t wt_lcmem_start_addr,           // weight start address, unit is 64B.
    uint32_t ofm_row_lcmem_start_addr,      // output feature map start address, unit is 64B.
    uint32_t bias_lcmem_start_addr,         // biase start address, unit is 64B.
    uint32_t shift_lcmem_start_addr,        // shift data start address, unit is 64B.
    uint32_t param_mmac_region_start,       // region start address, unit is 64B.
    uint32_t param_mmac_region_end,         // region end address, unit is 64B.
    uint32_t param_mmac_ifm_cluster_stride, // cluster_stride, unit is 64B.
    uint32_t param_mmac_cluster_start,      // cluster_start, unit is 64B.
    uint32_t param_mmac_cluster_end,        // cluster_end, unit is 64B.
    uint32_t param_mmac_cluster_num,        // if h_iter == 0, cluster num should -1.
    uint32_t input_conv_type,               // 3 processing types: 1: CONV, 2: DWC, 3: FC
    uint32_t b_with_bias_shift              // intermediate MAC result, just for debug
);

#else
void one_row_conv(int h_iter, conv2d_params_t *conv2d_param,        	  	    \
						uint32_t wt_lcmem_start_addr,							\
						uint32_t ofm_row_lcmem_start_addr,						\
						uint32_t bias_lcmem_start_addr,							\
						uint32_t shift_lcmem_start_addr,						\
                    	uint32_t param_mmac_region_start,                     	\
                    	uint32_t param_mmac_region_end,                       	\
                    	uint32_t param_mmac_ifm_cluster_stride,               	\
                    	uint32_t param_mmac_cluster_start,                    	\
                    	uint32_t param_mmac_cluster_end,                      	\
                    	uint32_t param_mmac_cluster_num,                        \
						uint32_t input_conv_type,                               \
						uint32_t b_with_bias_shift);

#endif
void one_row_conv_tiling(
	int h_iter,
	uint32_t tiling_part,
	conv2d_params_t *conv2d_param,
	uint32_t wt_lcmem_start_addr,
	uint32_t ofm_row_lcmem_start_addr,
	uint32_t bias_lcmem_start_addr,
	uint32_t shift_lcmem_start_addr,
	uint32_t param_mmac_region_start,
	uint32_t param_mmac_region_end,
	uint32_t param_mmac_ifm_cluster_stride,
	uint32_t param_mmac_cluster_start,
	uint32_t param_mmac_cluster_end,
	uint32_t param_mmac_cluster_num,
	uint32_t input_conv_type,
	uint32_t b_with_bias_shift);

void one_row_conv_tiling_1s1(
	int h_iter,
	uint32_t tiling_part,
	uint32_t delta_w_iter,
	conv2d_params_t *conv2d_param,
	uint32_t wt_lcmem_start_addr,
	uint32_t ofm_row_lcmem_start_addr,
	uint32_t bias_lcmem_start_addr,
	uint32_t shift_lcmem_start_addr,
	uint32_t param_mmac_region_start,
	uint32_t param_mmac_region_end,
	uint32_t param_mmac_ifm_cluster_stride,
	uint32_t param_mmac_cluster_start,
	uint32_t param_mmac_cluster_end,
	uint32_t param_mmac_cluster_num,
	uint32_t input_conv_type,
	uint32_t b_with_bias_shift);

void one_row_conv_tiling_precalc(
	int h_iter,
	uint32_t tiling_part,
	uint32_t delta_w_iter,
	conv2d_params_t *conv2d_param,
	uint32_t wt_lcmem_start_addr,
	uint32_t ofm_row_lcmem_start_addr,
	uint32_t bias_lcmem_start_addr,
	uint32_t shift_lcmem_start_addr,
	uint32_t param_mmac_region_start,
	uint32_t param_mmac_region_end,
	uint32_t param_mmac_ifm_cluster_stride,
	uint32_t param_mmac_cluster_start,
	uint32_t param_mmac_cluster_end,
	uint32_t param_mmac_cluster_num,
	uint32_t input_conv_type,
	uint32_t b_with_bias_shift);
void one_row_conv_add(
    int h_iter, conv2d_params_t *conv2d_par,
    uint32_t wt_lcmem_start_addr,
    uint32_t ofm_row_lcmem_start_addr,
    uint32_t bias_lcmem_start_addr,
    uint32_t shift_lcmem_start_addr,
  	uint32_t param_mmac_region_start,
  	uint32_t param_mmac_region_end,
  	uint32_t param_mmac_ifm_cluster_stride,
  	uint32_t param_mmac_cluster_start,
  	uint32_t param_mmac_cluster_end,
  	uint32_t param_mmac_cluster_num,
    uint32_t input_conv_type,
    uint32_t b_with_bias_shift,uint32_t ifm_ptr_c_add, uint32_t add_shift, uint32_t add_clip);

#define __intrinsic_func_layout_for_fc_input_with_c8__(_ifm, _ofm, _fm_h, _fm_w) do {\
    int ifm = _ifm, ofm = _ofm, fm_h = _fm_h, fm_w = _fm_w;\
	KRNL_LOG_INFO(LOG_DEBUG, " __intrinsic_func_layout_for_fc_input_with_c8__ h: %d w: %d", fm_h, fm_w);\
	/*temporarily use ofm local memory for VPR*/\
	uint32 * lcmem_ofm_addr_scalar = (uint32 *)(W64ToByte(ofm) + MEM_LCMEM_ADDR_S);\
	/* initialize the VPR*/\
	for(int i=0; i<16; i+=4){\
		*(lcmem_ofm_addr_scalar+i) = 0x01010101;\
		*(lcmem_ofm_addr_scalar+i+1) = 0x01010101;\
		*(lcmem_ofm_addr_scalar+i+2) = 0xffffffff;\
		*(lcmem_ofm_addr_scalar+i+3) = 0xffffffff;\
	}\
	VLB(vr14, ofm, 0);\
	/*@@@hardware issue, VLB with vpge_vv*/\
	vadd_vv(VPR_NONE, vr0, vr0, vr0);\
	vpge_vv(vpr0, vr14, vr0);\
	vplt_vv(vpr1, vr14, vr0);\
	/* transfor feature map from HWC88 to HWC, where C = 8*/\
	uint32 para0 = 0x6d2240;\
	uint32 para1 = 0xff6b64;\
	for(int j=0; j<fm_h; j+=2) { \
        for(int i=0; i<fm_w; i++) {\
			VLB(vr1, ifm, 0);\
			VALH(VPR_0_EN, vr3, vr1, para0);\
			VALH(VPR_0_EN, vr4, vr1, para1);\
			VLB(vr2, ifm+1, 0);\
			VALH(VPR_1_EN, vr3, vr2, para0);\
			VALH(VPR_1_EN, vr4, vr2, para1);\
			VSB(vr3, ofm, 0);\
			VSB(vr4, ofm, 1);\
			ofm += 2;\
			ifm += 2;\
		}\
	}\
}while(0)
#endif /*LIBCONV_H*/
