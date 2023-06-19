#include "hihw.h"
#include "libconv.h"
#include "dma.h"
#include "lock.h"
#include "int.h"
#include "operators/hi_krnl_param_conv2d.h"
#include "hi_addr_def.h"
#include "krnl_log.h"
#include "hpu_util.h"
// #include "qemu.h"

#define LCMEM_CONV_B_IN0   MMA_BANK0_START_ADDR
#define LCMEM_CONV_B_IN1   MMA_BANK1_START_ADDR
#define LCMEM_CONV_B_IN2   MMA_BANK2_START_ADDR
#define LCMEM_CONV_B_IN3   MMA_BANK3_START_ADDR
#define LCMEM_CONV_B_OUT0  MMA_BANK4_START_ADDR
#define LCMEM_BIAS_SHIFT   MMA_BANK6_START_ADDR

static const u32_t localmem_fm_table_conv_b[] =
{
    LCMEM_CONV_B_IN0,
    LCMEM_CONV_B_IN1,
    LCMEM_CONV_B_IN2,
	LCMEM_CONV_B_IN3
};
static const u32_t localmem_fm_conv_b_out[] =
{
    LCMEM_CONV_B_OUT0
};

inline void set_debug_flag(int debug,int cmd,int sys,int dma ){
	g_ulPrintDebugLogFlag = debug;
	g_ulPrintHardwareCmdLogFlag = cmd;
	g_ulPrintSYSLogFlag = sys;
	g_ulPrintNDMALogFlag = dma;
}

void _op_conv3s1(
	conv2d_params_t *conv1,

	hikl_addr_t *wt_addr_b,
	hikl_addr_t *bs_addr_b,
	hikl_addr_t *shift_addr_b,

	hikl_addr_t *ifm_addr,
	hikl_addr_t *ifm_addr_pleft,
	hikl_addr_t *ifm_addr_pright,

	hikl_addr_t *ofm_addr,

	uint32_t tiling_parttype);

void kernel_conv3s1()
{
	paramTableConv2d_t *_pParamTable = *((paramTableConv2d_t **)HIPU200_KNL_PTABLE_ADDR);/*get kernel param table from runtime*/
	paramTableConv2d_Entry_t *p_op_entry = &_pParamTable->param[0];
   	KRNL_LOG_INFO(LOG_SYSTEM, "%s","=================== kernel =========================");

	_op_conv3s1(
		&p_op_entry->conv2d,

		&p_op_entry->wt_addr,
		&p_op_entry->bias_addr,
		&p_op_entry->shift_addr,

		&p_op_entry->ifm_addr,
		&p_op_entry->ifm_addr_pleft,
		&p_op_entry->ifm_addr_pright,
		&p_op_entry->ofm_addr,

		p_op_entry->tiling_parttype);
};

void _op_conv3s1
(
    conv2d_params_t* conv1,

    hikl_addr_t *wt_addr_b,
    hikl_addr_t *bs_addr_b,
    hikl_addr_t *shift_addr_b,

    hikl_addr_t *ifm_addr,
    hikl_addr_t *ifm_addr_pleft,
    hikl_addr_t *ifm_addr_pright,

    hikl_addr_t *ofm_addr,

    uint32_t tiling_parttype
) 
{
    conv_shape_t *cshape_b = &(conv1->cshape);
    bool relu_b = conv1->relu;

    KRNL_LOG_INFO(LOG_SYSTEM, "enter into  _op_conv3s1");

    uint32 ndma_poll;
	uint32 h_iter_num_b;
	uint32 ifm_ptr;
	uint32 ofm_ptr;

	uint32 ifm_row_oneline_mrlen_b, ofm_row_oneline_mrlen_b = 0;
	uint32 ifm_row_oneline_ddrlen_b, ofm_row_oneline_ddrlen_b = 0;

	uint32 ifm_c_group8_num_b, wt_cluster_size_b, cluster_num_b = 0;
	uint32 w_iter_num_b, kernel_group8_num_b = 0;
	uint32 wt_offset_b,  cluster_start_b, cluster_end_b,wt_ptr_b, shift_ptr_b, bs_ptr_b, ifm_ptr_b,ofm_ptr_b = 0;

	uint8  round_type, shift_num, prot_high, prot_low =0;

	uint32 wt_sz_b, bs_sz_b, shift_sz_b;
	uint32 wt_start_b , bs_start_b, shift_start_b;

    // Calculate total weight size
	wt_sz_b = cshape_b->k_w * cshape_b->k_h * cshape_b->ifm_c * cshape_b->ofm_c; // in Bytes
	bs_sz_b = cshape_b->ofm_c * MTX_SCALE * 4;
	shift_sz_b = cshape_b->ofm_c * MTX_SCALE;
    wt_start_b = MMB_START_ADDR;
    bs_start_b = LCMEM_BIAS_SHIFT;
    shift_start_b = bs_start_b + bs_sz_b;

	// refined preprocess
	uint32 wt_start_logi_b , bs_start_logi_b, shift_start_logi_b;
	wt_start_logi_b = ByteToW64(wt_start_b-MMA_START_ADDR);
	bs_start_logi_b = ByteToW64(bs_start_b-MMA_START_ADDR);
	shift_start_logi_b = ByteToW64(shift_start_b-MMA_START_ADDR);

    // Load all weights
	//KRNL_LOG_INFO(LOG_DEBUG, "Load Weights: [%d%d%x]->[%x]\n\r", wt_addr->x_pos, wt_addr->y_pos, wt_addr->lcaddr, (uint32 *)MMB_ADDR);
	//MMB can't be access by HPU scalar ALU
    LIBHIKL_NASSERT(__rd_from_remote_chunk_blocking((uint32 *)wt_start_b, wt_addr_b->x_pos, wt_addr_b->y_pos, wt_addr_b->lcaddr,  GMEM_ALIGN(wt_sz_b)));
	// Load all bs
	//KRNL_LOG_INFO(LOG_DEBUG, "Load Bias: [%x]->[%x]\n\r", bias_addr->lcaddr, (uint32 *)(BIAS_SHIFT_BLK));
    LIBHIKL_NASSERT(__rd_from_remote_chunk_blocking((uint32 *)bs_start_b, bs_addr_b->x_pos, bs_addr_b->y_pos, bs_addr_b->lcaddr, GMEM_ALIGN(bs_sz_b)));
	// Load all shift_num
	//KRNL_LOG_INFO(LOG_DEBUG, "Load Shift_mtx: [%x]->[%x]\n\r", shift_addr->lcaddr, (uint32 *)(BIAS_SHIFT_BLK + bs_sz));
    LIBHIKL_NASSERT(__rd_from_remote_chunk_blocking((uint32 *)shift_start_b, shift_addr_b->x_pos, shift_addr_b->y_pos, shift_addr_b->lcaddr, GMEM_ALIGN(shift_sz_b)));

    //////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Calculate total iteration
	kernel_group8_num_b = cshape_b->ofm_c / MTX_SCALE;
	w_iter_num_b = cshape_b->ifm_w / MTX_SCALE;
	ifm_c_group8_num_b = cshape_b->ifm_c / MTX_SCALE;
	ifm_row_oneline_ddrlen_b = cshape_b->ifm_w * cshape_b->ifm_c;
	ofm_row_oneline_ddrlen_b = cshape_b->ifm_w * cshape_b->ofm_c;
	ifm_row_oneline_mrlen_b = ByteToW64(ifm_row_oneline_ddrlen_b);
	ofm_row_oneline_mrlen_b = ByteToW64(ofm_row_oneline_ddrlen_b);
	wt_cluster_size_b = ifm_c_group8_num_b * cshape_b->k_w;
	h_iter_num_b = cshape_b->ifm_h /*+ pshape->top + pshape->bottom 2 - cshape_b->k_h + 1*/;
	// set_debug_flag(1,1,1,0);
//////////////////////////////////////////////////////////////////////////////////////////////////////////////
	KRNL_LOG_INFO(LOG_SYSTEM, "%s","======01 pre compute start======");

	ifm_ptr = ifm_addr->lcaddr;
	ofm_ptr = ofm_addr->lcaddr;

	LIBHIKL_NASSERT(__rd_from_remote_chunk_non_blocking((uint32 *)localmem_fm_table_conv_b[0],
		ifm_addr->x_pos, ifm_addr->y_pos, ifm_ptr, GMEM_ALIGN(ifm_row_oneline_ddrlen_b)));
    __ndma_poll();
	ifm_ptr += ifm_row_oneline_ddrlen_b;

	LIBHIKL_NASSERT(__rd_from_remote_chunk_non_blocking((uint32 *)localmem_fm_table_conv_b[1],
		ifm_addr->x_pos, ifm_addr->y_pos, ifm_ptr, GMEM_ALIGN(ifm_row_oneline_ddrlen_b)));
    __ndma_poll();
	ifm_ptr += ifm_row_oneline_ddrlen_b;

	// set_debug_flag(0,0,0,0);
    for (int i = 0; i < h_iter_num_b; i++) 
	{
		KRNL_LOG_INFO(LOG_DEBUG, "=====ifm H iter: %d / %d=====\n\r", i, h_iter_num_b);
		if( i < h_iter_num_b - 2 ){ //只要不是最后一行
			//KRNL_LOG_INFO(LOG_DEBUG, "Load ifm row %d: [%x]->[%x]\n\r", (ifm->bfm.total_cnt - 1), ddr_mem_fm_addr, local_mem_fm_addr);
			LIBHIKL_NASSERT(__rd_from_remote_chunk_non_blocking((uint32 *)localmem_fm_table_conv_b[(i + 2) % 4], ifm_addr->x_pos, ifm_addr->y_pos,
				ifm_ptr, GMEM_ALIGN(ifm_row_oneline_ddrlen_b)));
			ifm_ptr += ifm_row_oneline_ddrlen_b;

		}
		// else{
		// 	ifm_ptr += ifm_row_oneline_ddrlen_a;
		// }
//////////////////////////////////////////////////////////////////////////////////////////////////////////////
		if(i < /*pshape->top*/1){   //如果当前是padding行 需要单独设置mmac参数 涉及到fm的跳转和wt的跳转
			// cluster_start = ByteToW64(MMA_BEGIN);
			// always start from the first ifm row, pad automatically increments inside mmac
			cluster_start_b = ByteToW64(localmem_fm_table_conv_b[0] - MMA_START_ADDR); //指定bank3为起点。只在3 4 跳，硬件自动识别padding
			cluster_end_b = cluster_start_b + ifm_row_oneline_mrlen_b;        // always the end of the first ifm row
			cluster_num_b = cshape_b->k_h - /*pshape->top*/conv1->pshape.top - 1;     // always less than the full kernel height
		}
		else{    //如果是最后一行，cluster需要特例设置；如果在中间，不需要
			cluster_start_b = ByteToW64(localmem_fm_table_conv_b[(i+3) % 4] - MMA_START_ADDR);
			cluster_end_b = cluster_start_b + ifm_row_oneline_mrlen_b;
			if((i + cshape_b->k_h) > (cshape_b->ifm_h + /*pshape->top*/1))   // at bottom
				cluster_num_b = cshape_b->ifm_h - i;   // always less than the full kernel height
			else // at middle
				cluster_num_b = cshape_b->k_h - 1;   // always equal the full kernel height
		}
		// if( i >= h_iter_num_b - 1 ) set_debug_flag(1,1,1,0);
		KRNL_LOG_INFO(LOG_DEBUG, "cluster_start: [%x] cluster_end: [%x]", cluster_start_b, cluster_end_b);

		KRNL_LOG_INFO(LOG_SYSTEM, "%s","======02 conv_b compute start======");
#ifdef OPT_ONE_ROW_CONV
	one_row_conv(i, TILING_NONE, 0, conv1,
					wt_start_logi_b,
					ByteToW64(localmem_fm_conv_b_out[0] - MMA_START_ADDR),
					bs_start_logi_b,
					shift_start_logi_b,
                    ByteToW64(localmem_fm_table_conv_b[0] - MEM_LCMEM_ADDR_S),
                    ByteToW64(localmem_fm_table_conv_b[0] + (sizeof(localmem_fm_table_conv_b)/sizeof(u32_t)) * MMA_BANK_SIZE - MEM_LCMEM_ADDR_S),
                   	ByteToW64(MMA_BANK_SIZE),
                    cluster_start_b,
                    cluster_end_b,
                    cluster_num_b,
					CONV_TYPE_CLASSIC, 1);
#else
		one_row_conv(i, conv1,
					wt_start_logi_b,
					ByteToW64(localmem_fm_conv_b_out[0] - MMA_START_ADDR),
					bs_start_logi_b,
					shift_start_logi_b,
                    ByteToW64(localmem_fm_table_conv_b[0] - MEM_LCMEM_ADDR_S),
                    ByteToW64(localmem_fm_table_conv_b[0] + (sizeof(localmem_fm_table_conv_b)/sizeof(u32_t)) * MMA_BANK_SIZE - MEM_LCMEM_ADDR_S),
                   	ByteToW64(MMA_BANK_SIZE),
                    cluster_start_b,
                    cluster_end_b,
                    cluster_num_b,
					CONV_TYPE_CLASSIC, 1);
#endif

		__ndma_poll();
        LIBHIKL_NASSERT(__wr_to_remote_chunk_non_blocking((uint32 *)localmem_fm_conv_b_out[0], ofm_addr->x_pos, ofm_addr->y_pos, ofm_ptr_b, (ofm_row_oneline_ddrlen_b)));
        ofm_ptr_b += ofm_row_oneline_ddrlen_b;
		__ndma_poll();
	}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////
	KRNL_LOG_INFO(LOG_DEBUG, "calculation is end, ndma begins...\n\r");
	return;
}

