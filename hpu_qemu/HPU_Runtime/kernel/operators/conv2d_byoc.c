

#include "hihw.h"
#include "libconv.h"
#include "dma.h"
#include "lock.h"
#include "int.h"
#include "hpu200/operators/hi_krnl_param_conv2d_hpu200.h"
#include "hi_addr_def.h"
#include "krnl_log.h"
#include "hpu_util.h"


// #define LOCAL_IFM_BLK   MMA_BANK0_START_ADDR
// #define LOCAL_OFM_BLK   MMA_BANK5_START_ADDR
// #define LOCAL_VAR_BLK   MMA_BANK4_START_ADDR
// #define BIAS_SHIFT_BLK  MMA_BANK7_START_ADDR
// #define REMOT_IFM_BLK   MMA_BANK0_START_ADDR
// #define VAR_BLK_IN_REMOTE_NODE   MMA_BANK6_START_ADDR

// #define IFM_PREFETCH_ROW_NUM 1
// #define OFM_OUTPUT_SLOTS_NUM 2

// extern void wait(int);


#define IFM_BANK_NUMBER 6
#define LOCAL_IFM_BLOCK MMA_BANK0_START_ADDR
#define LOCAL_OFM_BLOCK IFM_BANK_NUMBER*MMA_BANK_SIZE+LOCAL_IFM_BLOCK
#define LOCAL_BIAS_SHIFT LOCAL_OFM_BLOCK+MMA_BANK_SIZE
#define LOCAL_WEIGHT_BLOCK MMB_START_ADDR


static PTConv2dEntryHPU200 *conv_table;
static hirtKernelParamTableBase_t *base_table;

static uint32_t ifm_rm_ptr;
static uint32_t ofm_rm_ptr;
static int bank_index;	



static hikl_addr_t last_bias_rmt_addr;
static uint32_t last_bias_mem_size;
static uint32_t last_bias_lcmem_addr;
static hikl_addr_t last_shift_rmt_addr;
static uint32_t last_shift_mem_size;
static uint32_t last_shift_lcmem_addr;
static hikl_addr_t last_wt_rmt_addr;
static uint32_t last_wt_mem_size;
static uint32_t last_wt_lcmem_addr;
										
										


static void log_conv_param(PTConv2dEntryHPU200 *p_conv2d_entry)
{
	KRNL_LOG_INFO(LOG_SYSTEM, "ifm_h: %d", p_conv2d_entry->tiled_param_host.tiling_info.conv2d_for_block.cshape.ifm_h);
	KRNL_LOG_INFO(LOG_SYSTEM, "ifm_w: %d", p_conv2d_entry->tiled_param_host.tiling_info.conv2d_for_block.cshape.ifm_w);
	KRNL_LOG_INFO(LOG_SYSTEM, "ifm_c: %d", p_conv2d_entry->tiled_param_host.tiling_info.conv2d_for_block.cshape.ifm_c);
	KRNL_LOG_INFO(LOG_SYSTEM, "ofm_c: %d", p_conv2d_entry->tiled_param_host.tiling_info.conv2d_for_block.cshape.ofm_c);
	KRNL_LOG_INFO(LOG_SYSTEM, "k_h: %d", p_conv2d_entry->tiled_param_host.tiling_info.conv2d_for_block.cshape.k_h);
	KRNL_LOG_INFO(LOG_SYSTEM, "k_w: %d", p_conv2d_entry->tiled_param_host.tiling_info.conv2d_for_block.cshape.k_w);
	KRNL_LOG_INFO(LOG_SYSTEM, "pshape_top: %d",  p_conv2d_entry->tiled_param_host.tiling_info.conv2d_for_block.pshape.top);
	KRNL_LOG_INFO(LOG_SYSTEM, "pshape_bottom: %d", p_conv2d_entry->tiled_param_host.tiling_info.conv2d_for_block.pshape.bottom);
	KRNL_LOG_INFO(LOG_SYSTEM, "pshape_left: %d pshape_right: %d", p_conv2d_entry->tiled_param_host.tiling_info.conv2d_for_block.pshape.left, p_conv2d_entry->tiled_param_host.tiling_info.conv2d_for_block.pshape.right);
	KRNL_LOG_INFO(LOG_SYSTEM, "h_strd: %d", p_conv2d_entry->tiled_param_host.tiling_info.conv2d_for_block.strd_shape.h_strd);
	KRNL_LOG_INFO(LOG_SYSTEM, "w_strd: %d", p_conv2d_entry->tiled_param_host.tiling_info.conv2d_for_block.strd_shape.w_strd);
	KRNL_LOG_INFO(LOG_SYSTEM, "relu: %d", p_conv2d_entry->tiled_param_host.tiling_info.conv2d_for_block.relu);
}


static void init_static_var()
{
	conv_table = NULL;
	base_table = NULL;
	bank_index=0;
	ifm_rm_ptr=0;
	ofm_rm_ptr=0;
	last_bias_rmt_addr.lcaddr=0;
	last_bias_rmt_addr.x_pos=0;
	last_bias_rmt_addr.y_pos=0;
	last_bias_mem_size=0;
	last_bias_lcmem_addr=0;
	last_shift_rmt_addr.lcaddr=0;
	last_shift_rmt_addr.x_pos=0;
	last_shift_rmt_addr.y_pos=0;
	last_shift_mem_size=0;
	last_shift_lcmem_addr=0;
	last_wt_rmt_addr.lcaddr=0;
	last_wt_rmt_addr.x_pos=0;
	last_wt_rmt_addr.y_pos=0;
	last_wt_mem_size=0;
	last_wt_lcmem_addr=0;
}

//this function is for loading weight,bias,shift params(only called once for every tiling block)
inline static void load_weight_bias_shift(hikl_addr_t *bias_rmt_addr,uint32_t bias_mem_size,uint32_t bias_lcmem_addr,
										hikl_addr_t *shift_rmt_addr,uint32_t shift_mem_size,uint32_t shift_lcmem_addr,
										hikl_addr_t *wt_rmt_addr,uint32_t wt_mem_size,uint32_t wt_lcmem_addr)
{
	// Load all weights
	// KRNL_LOG_INFO(LOG_DEBUG, "Load Weights: [%d%d%x]->[%x]\n\r", wt_rmt_addr->x_pos, wt_rmt_addr->y_pos, wt_rmt_addr->lcaddr, (uint32 *)wt_lcmem_addr);
	//MMB can't be access by HPU scalar ALU
	//judge if wt has been changed and need to be reloaded
	if(wt_lcmem_addr==last_wt_lcmem_addr&&wt_rmt_addr->x_pos==last_wt_rmt_addr.x_pos&&wt_rmt_addr->y_pos==last_wt_rmt_addr.y_pos&&wt_rmt_addr->lcaddr==last_wt_rmt_addr.lcaddr&&wt_mem_size==last_wt_mem_size)
	{
		KRNL_LOG_INFO(LOG_DEBUG, "skip load weight");
	}
	else
	{
		LIBHIKL_NASSERT(__rd_from_remote_chunk_blocking((uint32 *)wt_lcmem_addr, wt_rmt_addr->x_pos,wt_rmt_addr->y_pos, wt_rmt_addr->lcaddr,  GMEM_ALIGN(wt_mem_size)));
		last_wt_lcmem_addr=wt_lcmem_addr;
		last_wt_rmt_addr=*wt_rmt_addr;
		last_wt_mem_size=wt_mem_size;
	}
	// Load all bias
	// KRNL_LOG_INFO(LOG_DEBUG, "Load Bias: [%x]->[%x]\n\r",  bias_rmt_addr->lcaddr, (uint32 *)bias_lcmem_addr);
	//judge if bias has been changed and need to be reloaded
	if(bias_lcmem_addr==last_bias_lcmem_addr&&bias_rmt_addr->x_pos==last_bias_rmt_addr.x_pos&&bias_rmt_addr->y_pos==last_bias_rmt_addr.y_pos&&bias_rmt_addr->lcaddr==last_bias_rmt_addr.lcaddr&&bias_mem_size==last_bias_mem_size)
	{
		KRNL_LOG_INFO(LOG_DEBUG, "skip load bias");
	}
	else
	{
		LIBHIKL_NASSERT(__rd_from_remote_chunk_blocking((uint32 *)bias_lcmem_addr, bias_rmt_addr->x_pos,bias_rmt_addr->y_pos, bias_rmt_addr->lcaddr,  GMEM_ALIGN(bias_mem_size)));
		last_bias_lcmem_addr=bias_lcmem_addr;
		last_bias_rmt_addr=*bias_rmt_addr;
		last_bias_mem_size=bias_mem_size;
		
	}
	// Load all shift_num
	// KRNL_LOG_INFO(LOG_DEBUG, "Load Shift_mtx: [%x]->[%x]\n\r", shift_rmt_addr->lcaddr, (uint32 *)shift_lcmem_addr);
	//judge if shift has been changed and need to be reloaded
	if(shift_lcmem_addr==last_shift_lcmem_addr&&shift_rmt_addr->x_pos==last_shift_rmt_addr.x_pos&&shift_rmt_addr->y_pos==last_shift_rmt_addr.y_pos&&shift_rmt_addr->lcaddr==last_shift_rmt_addr.lcaddr&&shift_mem_size==last_shift_mem_size)
	{
		KRNL_LOG_INFO(LOG_DEBUG, "skip load shift");
	}
	else
	{
		LIBHIKL_NASSERT(__rd_from_remote_chunk_blocking((uint32 *)shift_lcmem_addr, shift_rmt_addr->x_pos,shift_rmt_addr->y_pos, shift_rmt_addr->lcaddr,  GMEM_ALIGN(shift_mem_size)));
		last_shift_lcmem_addr=shift_lcmem_addr;
		last_shift_rmt_addr=*shift_rmt_addr;
		last_shift_mem_size=shift_mem_size;
	}
}

//this function is for loading next few ifm lines params(called every ofm line) 
inline static void makesure_needed_ifm_rows_ready(PTConv2dOneRowHPU200 one_ofm_row_param,TilingIfmRowInfo ifm_row_tiled[])
{
	uint32_t prefetch_row_count=one_ofm_row_param.ifm_prefetch_info.prefetch_row_count;
	uint32_t local_addr,x_pos,y_pos,rm_addr,row_size;

	for(int i=0; i<prefetch_row_count; i++)
	{
		//loading continuous ifm params
		local_addr=ifm_row_tiled[i].ifm_continuous_row_tiled_info.lcmem_addr;
		x_pos=ifm_row_tiled[i].ifm_continuous_row_tiled_info.rmt_addr.x_pos;
		y_pos=ifm_row_tiled[i].ifm_continuous_row_tiled_info.rmt_addr.y_pos;
		rm_addr=ifm_row_tiled[i].ifm_continuous_row_tiled_info.rmt_addr.lcaddr;
		row_size=ifm_row_tiled[i].ifm_continuous_row_tiled_info.mem_size;
		LIBHIKL_NASSERT(__rd_from_remote_chunk_blocking((uint32 *)local_addr, x_pos, y_pos, rm_addr, GMEM_ALIGN(row_size)));
		//__ndma_poll();
		// KRNL_LOG_INFO(LOG_SYSTEM, "Load local_ifm row null prefetchline %d continuous: [%d%d%x]->[%x] size: %d Bytes\n\r", i,x_pos, y_pos, rm_addr, \
						local_addr, GMEM_ALIGN(row_size));
		if(ifm_row_tiled[i].has_supplement)
		{
			//loading supplement ifm params for left/right padding
			local_addr=ifm_row_tiled[i].ifm_row_supplement_info.lcmem_addr;
			x_pos=ifm_row_tiled[i].ifm_row_supplement_info.rmt_addr.x_pos;
			y_pos=ifm_row_tiled[i].ifm_row_supplement_info.rmt_addr.y_pos;
			rm_addr=ifm_row_tiled[i].ifm_row_supplement_info.rmt_addr.lcaddr;
			row_size=ifm_row_tiled[i].ifm_row_supplement_info.mem_size;
			LIBHIKL_NASSERT(__rd_from_remote_chunk_blocking((uint32 *)local_addr, x_pos, y_pos, rm_addr, GMEM_ALIGN(row_size)));
			//__ndma_poll();
			// KRNL_LOG_INFO(LOG_SYSTEM, "Load local_ifm row %d supplement: [%d%d%x]->[%x] size: %d Bytes\n\r", i, x_pos, y_pos, rm_addr, \
							local_addr, GMEM_ALIGN(row_size));
		}
	}
}
	
inline static void prepare_param(PTConv2dOneRowHPU200 *one_ofm_row_param,PTConv2dEntryHPU200 *p_conv2d_entry, conv2d_params_t conv2d_whole,int h_iter_num)
{
	//caculate prefetch_row_count for each line
	//top
	if(h_iter_num==0)
	{
		one_ofm_row_param->ifm_prefetch_info.prefetch_row_count=p_conv2d_entry->tiled_param_host.tiling_info.conv2d_for_block.cshape.k_h-p_conv2d_entry->tiled_param_host.tiling_info.conv2d_for_block.pshape.top;
	}
	//botton
	else if(h_iter_num*p_conv2d_entry->tiled_param_host.tiling_info.conv2d_for_block.strd_shape.h_strd-p_conv2d_entry->tiled_param_host.tiling_info.conv2d_for_block.pshape.top+p_conv2d_entry->tiled_param_host.tiling_info.conv2d_for_block.cshape.k_h>p_conv2d_entry->tiled_param_host.tiling_info.conv2d_for_block.cshape.ifm_h)
	{
		int botton_padding_h=h_iter_num*p_conv2d_entry->tiled_param_host.tiling_info.conv2d_for_block.strd_shape.h_strd-p_conv2d_entry->tiled_param_host.tiling_info.conv2d_for_block.pshape.top+p_conv2d_entry->tiled_param_host.tiling_info.conv2d_for_block.cshape.k_h-p_conv2d_entry->tiled_param_host.tiling_info.conv2d_for_block.cshape.ifm_h;
		if(p_conv2d_entry->tiled_param_host.tiling_info.conv2d_for_block.strd_shape.h_strd-botton_padding_h>=0)
		{
			one_ofm_row_param->ifm_prefetch_info.prefetch_row_count=p_conv2d_entry->tiled_param_host.tiling_info.conv2d_for_block.strd_shape.h_strd-botton_padding_h;
		}
		else
		{
			one_ofm_row_param->ifm_prefetch_info.prefetch_row_count=0;
		}
		// KRNL_LOG_INFO(LOG_SYSTEM, "prefetch_row_count:[%i]",one_ofm_row_param->ifm_prefetch_info.prefetch_row_count);
	}
	//middle
	else
	{
		one_ofm_row_param->ifm_prefetch_info.prefetch_row_count=p_conv2d_entry->tiled_param_host.tiling_info.conv2d_for_block.strd_shape.h_strd;
	}
	//caculate prefetch_row_count for each line end

	//caculate cluster info for each line
	uint32_t cluster_start;
	uint32_t cluster_num;
	//cluster start&end
	cluster_start=0;
	cluster_num=0;
	if(h_iter_num*p_conv2d_entry->tiled_param_host.tiling_info.conv2d_for_block.strd_shape.h_strd>=p_conv2d_entry->tiled_param_host.tiling_info.conv2d_for_block.pshape.top)
	{
		cluster_start=(h_iter_num*p_conv2d_entry->tiled_param_host.tiling_info.conv2d_for_block.strd_shape.h_strd-p_conv2d_entry->tiled_param_host.tiling_info.conv2d_for_block.pshape.top)%IFM_BANK_NUMBER;
	}
	one_ofm_row_param->param_mmac_cluster_start=LOCAL_IFM_BLOCK+cluster_start*MMA_BANK_SIZE-MMA_START_ADDR;
	if(p_conv2d_entry->tiled_param_host.tiling_info.tiling_ifm_w_partype==kTilingIfmWLeft)
	{
		if(p_conv2d_entry->tiled_param_host.tiling_info.w==-1)
		{
			one_ofm_row_param->param_mmac_cluster_end=\
			one_ofm_row_param->param_mmac_cluster_start+
			p_conv2d_entry->tiled_param_host.tiling_info.ifm_row_size_tiled;
		}
		else
		{
			one_ofm_row_param->param_mmac_cluster_end=\
				one_ofm_row_param->param_mmac_cluster_start+
				p_conv2d_entry->tiled_param_host.tiling_info.ifm_row_size_tiled+p_conv2d_entry->tiled_param_host.tiling_info.conv2d_for_block.cshape.ifm_c*p_conv2d_entry->tiled_param_host.tiling_info.conv2d_for_block.pshape.left*8+p_conv2d_entry->tiled_param_host.tiling_info.conv2d_for_block.cshape.ifm_c*(p_conv2d_entry->tiled_param_host.tiling_info.conv2d_for_block.cshape.k_w-1)/2*8;
		}

	}
	else if(p_conv2d_entry->tiled_param_host.tiling_info.tiling_ifm_w_partype==kTilingIfmWRight)
	{
		one_ofm_row_param->param_mmac_cluster_end=\
			one_ofm_row_param->param_mmac_cluster_start+
			p_conv2d_entry->tiled_param_host.tiling_info.ifm_row_size_tiled+p_conv2d_entry->tiled_param_host.tiling_info.conv2d_for_block.cshape.ifm_c*p_conv2d_entry->tiled_param_host.tiling_info.conv2d_for_block.pshape.right*8+p_conv2d_entry->tiled_param_host.tiling_info.conv2d_for_block.cshape.ifm_c*(p_conv2d_entry->tiled_param_host.tiling_info.conv2d_for_block.cshape.k_w-1)/2*8;
	}
	else
	{
		one_ofm_row_param->param_mmac_cluster_end=\
			one_ofm_row_param->param_mmac_cluster_start+
		    p_conv2d_entry->tiled_param_host.tiling_info.ifm_row_size_tiled+p_conv2d_entry->tiled_param_host.tiling_info.conv2d_for_block.cshape.ifm_c*(p_conv2d_entry->tiled_param_host.tiling_info.conv2d_for_block.cshape.k_w-1)*8;

	}
	//cluster number
	//top
	if(h_iter_num*p_conv2d_entry->tiled_param_host.tiling_info.conv2d_for_block.strd_shape.h_strd<p_conv2d_entry->tiled_param_host.tiling_info.conv2d_for_block.pshape.top)
	{
		cluster_num=p_conv2d_entry->tiled_param_host.tiling_info.conv2d_for_block.cshape.k_h-(p_conv2d_entry->tiled_param_host.tiling_info.conv2d_for_block.pshape.top-h_iter_num*p_conv2d_entry->tiled_param_host.tiling_info.conv2d_for_block.strd_shape.h_strd);
		// KRNL_LOG_INFO(LOG_SYSTEM, "cluster_num:[%i]",cluster_num);
	}
	//bottom
	else if(h_iter_num*p_conv2d_entry->tiled_param_host.tiling_info.conv2d_for_block.strd_shape.h_strd-p_conv2d_entry->tiled_param_host.tiling_info.conv2d_for_block.pshape.top+p_conv2d_entry->tiled_param_host.tiling_info.conv2d_for_block.cshape.k_h>p_conv2d_entry->tiled_param_host.tiling_info.conv2d_for_block.cshape.ifm_h)
	{
		cluster_num=p_conv2d_entry->tiled_param_host.tiling_info.conv2d_for_block.cshape.k_h-(h_iter_num*p_conv2d_entry->tiled_param_host.tiling_info.conv2d_for_block.strd_shape.h_strd-p_conv2d_entry->tiled_param_host.tiling_info.conv2d_for_block.pshape.top+p_conv2d_entry->tiled_param_host.tiling_info.conv2d_for_block.cshape.k_h-p_conv2d_entry->tiled_param_host.tiling_info.conv2d_for_block.cshape.ifm_h);
		// KRNL_LOG_INFO(LOG_SYSTEM, "cluster_num:[%i]",cluster_num);
	}
	//middle
	else
	{
		cluster_num=p_conv2d_entry->tiled_param_host.tiling_info.conv2d_for_block.cshape.k_h;
		// KRNL_LOG_INFO(LOG_SYSTEM, "cluster_num:[%i]",cluster_num);
	}
	//sub 1 for self-add when running on hardware
	one_ofm_row_param->param_mmac_cluster_num=cluster_num-1;
	//set other args in PTConv2dOneRowHPU200
	one_ofm_row_param->wt_start_addr_tiled_for_instructions=MMB_START_ADDR-MMA_START_ADDR;
	one_ofm_row_param->need_to_xfr_ofm=1;
	one_ofm_row_param->rmt_ofm_addr_tiled.x_pos=p_conv2d_entry->tiled_param_host.Tilingwithdev.ofm_rmt_addr.x_pos;
	one_ofm_row_param->rmt_ofm_addr_tiled.y_pos=p_conv2d_entry->tiled_param_host.Tilingwithdev.ofm_rmt_addr.y_pos;
	one_ofm_row_param->rmt_ofm_addr_tiled.lcaddr=ofm_rm_ptr;
	one_ofm_row_param->one_row_ofm_size=p_conv2d_entry->tiled_param_host.tiling_info.ofm_row_size_tiled;
	ofm_rm_ptr=ofm_rm_ptr+(conv2d_whole.cshape.ofm_c*conv2d_whole.cshape.ofm_w);
	
	return;
}

//this function is for setting prefetch line params ,prefetch_row_count is from 'prepare_param'
inline static void prepare_preline(TilingIfmRowInfo *ifm_row_tiled,PTConv2dOneRowHPU200 *one_ofm_row_param,PTConv2dEntryHPU200 *p_conv2d_entry, conv2d_params_t conv2d_whole,int h_iter_num)
{
	if(h_iter_num==0)
	{
		bank_index=0;
	}
	for(int profetch_row=0;profetch_row<one_ofm_row_param->ifm_prefetch_info.prefetch_row_count;profetch_row++)
	{
		if(p_conv2d_entry->tiled_param_host.tiling_info.tiling_ifm_w_partype==kTilingIfmWLeft)
		{
			if(p_conv2d_entry->tiled_param_host.tiling_info.w!=-1)
			{
				ifm_row_tiled[profetch_row].ifm_continuous_row_tiled_info.mem_size=p_conv2d_entry->tiled_param_host.tiling_info.conv2d_for_block.cshape.ifm_c*(p_conv2d_entry->tiled_param_host.tiling_info.conv2d_for_block.cshape.ifm_w+(p_conv2d_entry->tiled_param_host.tiling_info.conv2d_for_block.cshape.k_w-1)/2*8);
				ifm_row_tiled[profetch_row].ifm_row_supplement_info.mem_size=p_conv2d_entry->tiled_param_host.tiling_info.conv2d_for_block.cshape.ifm_c*p_conv2d_entry->tiled_param_host.tiling_info.conv2d_for_block.pshape.left*8;
			}
			else
			{
				ifm_row_tiled[profetch_row].ifm_continuous_row_tiled_info.mem_size=p_conv2d_entry->tiled_param_host.tiling_info.conv2d_for_block.cshape.ifm_c*p_conv2d_entry->tiled_param_host.tiling_info.conv2d_for_block.cshape.ifm_w;
				ifm_row_tiled[profetch_row].ifm_row_supplement_info.mem_size=0;
			}
			if(ifm_row_tiled[profetch_row].ifm_row_supplement_info.mem_size!=0)
			{
				ifm_row_tiled[profetch_row].has_supplement=1;
			}
			else
			{
				ifm_row_tiled[profetch_row].has_supplement=0;
			}
			ifm_row_tiled[profetch_row].ifm_continuous_row_tiled_info.lcmem_addr=LOCAL_IFM_BLOCK+bank_index*MMA_BANK_SIZE;
			ifm_row_tiled[profetch_row].ifm_row_supplement_info.lcmem_addr=LOCAL_IFM_BLOCK+bank_index*MMA_BANK_SIZE+
			ifm_row_tiled[profetch_row].ifm_continuous_row_tiled_info.mem_size;

			bank_index=((++bank_index)%IFM_BANK_NUMBER);

			ifm_row_tiled[profetch_row].ifm_continuous_row_tiled_info.rmt_addr.x_pos=p_conv2d_entry->tiled_param_host.Tilingwithdev.ifm_rmt_addr.x_pos;
			ifm_row_tiled[profetch_row].ifm_continuous_row_tiled_info.rmt_addr.y_pos=p_conv2d_entry->tiled_param_host.Tilingwithdev.ifm_rmt_addr.y_pos;
			ifm_row_tiled[profetch_row].ifm_continuous_row_tiled_info.rmt_addr.lcaddr=ifm_rm_ptr;
		
			ifm_row_tiled[profetch_row].ifm_row_supplement_info.rmt_addr.x_pos=p_conv2d_entry->tiled_param_host.Tilingwithdev.ifm_rmt_addr.x_pos;
			ifm_row_tiled[profetch_row].ifm_row_supplement_info.rmt_addr.y_pos=p_conv2d_entry->tiled_param_host.Tilingwithdev.ifm_rmt_addr.y_pos;
			ifm_row_tiled[profetch_row].ifm_row_supplement_info.rmt_addr.lcaddr=ifm_rm_ptr+conv2d_whole.cshape.ifm_w*p_conv2d_entry->tiled_param_host.tiling_info.conv2d_for_block.cshape.ifm_c-8*p_conv2d_entry->tiled_param_host.tiling_info.conv2d_for_block.cshape.ifm_c*p_conv2d_entry->tiled_param_host.tiling_info.conv2d_for_block.pshape.left;

			ifm_rm_ptr+=conv2d_whole.cshape.ifm_c*conv2d_whole.cshape.ifm_w;
		}
		
		else if(p_conv2d_entry->tiled_param_host.tiling_info.tiling_ifm_w_partype==kTilingIfmWRight)
		{
			ifm_row_tiled[profetch_row].ifm_continuous_row_tiled_info.mem_size=p_conv2d_entry->tiled_param_host.tiling_info.conv2d_for_block.cshape.ifm_c*(p_conv2d_entry->tiled_param_host.tiling_info.conv2d_for_block.cshape.ifm_w+(p_conv2d_entry->tiled_param_host.tiling_info.conv2d_for_block.cshape.k_w-1)/2*8);
			ifm_row_tiled[profetch_row].ifm_row_supplement_info.mem_size=p_conv2d_entry->tiled_param_host.tiling_info.conv2d_for_block.cshape.ifm_c*p_conv2d_entry->tiled_param_host.tiling_info.conv2d_for_block.pshape.right*8;
			if(ifm_row_tiled[profetch_row].ifm_row_supplement_info.mem_size!=0)
			{
				ifm_row_tiled[profetch_row].has_supplement=1;
			}
			else
			{
				ifm_row_tiled[profetch_row].has_supplement=0;
			}
			ifm_row_tiled[profetch_row].ifm_row_supplement_info.lcmem_addr=LOCAL_IFM_BLOCK+bank_index*MMA_BANK_SIZE;
			ifm_row_tiled[profetch_row].ifm_continuous_row_tiled_info.lcmem_addr=LOCAL_IFM_BLOCK+bank_index*MMA_BANK_SIZE+
			ifm_row_tiled[profetch_row].ifm_row_supplement_info.mem_size;

			bank_index=((++bank_index)%IFM_BANK_NUMBER);
			
			ifm_row_tiled[profetch_row].ifm_continuous_row_tiled_info.rmt_addr.x_pos=p_conv2d_entry->tiled_param_host.Tilingwithdev.ifm_rmt_addr.x_pos;
			ifm_row_tiled[profetch_row].ifm_continuous_row_tiled_info.rmt_addr.y_pos=p_conv2d_entry->tiled_param_host.Tilingwithdev.ifm_rmt_addr.y_pos;
			ifm_row_tiled[profetch_row].ifm_continuous_row_tiled_info.rmt_addr.lcaddr=ifm_rm_ptr-(p_conv2d_entry->tiled_param_host.tiling_info.conv2d_for_block.cshape.k_w-1)/2*8*p_conv2d_entry->tiled_param_host.tiling_info.conv2d_for_block.cshape.ifm_c;
		
			ifm_row_tiled[profetch_row].ifm_row_supplement_info.rmt_addr.x_pos=p_conv2d_entry->tiled_param_host.Tilingwithdev.ifm_rmt_addr.x_pos;
			ifm_row_tiled[profetch_row].ifm_row_supplement_info.rmt_addr.y_pos=p_conv2d_entry->tiled_param_host.Tilingwithdev.ifm_rmt_addr.y_pos;
			ifm_row_tiled[profetch_row].ifm_row_supplement_info.rmt_addr.lcaddr=ifm_rm_ptr+p_conv2d_entry->tiled_param_host.tiling_info.conv2d_for_block.cshape.ifm_c*p_conv2d_entry->tiled_param_host.tiling_info.conv2d_for_block.cshape.ifm_w-conv2d_whole.cshape.ifm_w*p_conv2d_entry->tiled_param_host.tiling_info.conv2d_for_block.cshape.ifm_c;

			ifm_rm_ptr+=conv2d_whole.cshape.ifm_c*conv2d_whole.cshape.ifm_w;
		}
		else
		{
			ifm_row_tiled[profetch_row].has_supplement=0;
			ifm_row_tiled[profetch_row].ifm_continuous_row_tiled_info.mem_size=p_conv2d_entry->tiled_param_host.tiling_info.conv2d_for_block.cshape.ifm_c*(p_conv2d_entry->tiled_param_host.tiling_info.conv2d_for_block.cshape.ifm_w+(p_conv2d_entry->tiled_param_host.tiling_info.conv2d_for_block.cshape.k_w-1)*8);
			ifm_row_tiled[profetch_row].ifm_continuous_row_tiled_info.lcmem_addr=LOCAL_IFM_BLOCK+bank_index*MMA_BANK_SIZE;

			bank_index=((++bank_index)%IFM_BANK_NUMBER);

			ifm_row_tiled[profetch_row].ifm_continuous_row_tiled_info.rmt_addr.x_pos=p_conv2d_entry->tiled_param_host.Tilingwithdev.ifm_rmt_addr.x_pos;
			ifm_row_tiled[profetch_row].ifm_continuous_row_tiled_info.rmt_addr.y_pos=p_conv2d_entry->tiled_param_host.Tilingwithdev.ifm_rmt_addr.y_pos;
			ifm_row_tiled[profetch_row].ifm_continuous_row_tiled_info.rmt_addr.lcaddr=ifm_rm_ptr-(p_conv2d_entry->tiled_param_host.tiling_info.conv2d_for_block.cshape.k_w-1)/2*8*p_conv2d_entry->tiled_param_host.tiling_info.conv2d_for_block.cshape.ifm_c;
			ifm_rm_ptr+=conv2d_whole.cshape.ifm_c*conv2d_whole.cshape.ifm_w;
		}
	}

	return;
}

static void conv2d_core(PTConv2dEntryHPU200 *p_conv2d_entry, uint32_t input_conv_type,conv2d_params_t conv2d_whole)
{
	// KRNL_LOG_INFO(LOG_SYSTEM, "into conv2d_core");
	
#ifdef QEMU_ENV
	printf("func: conv2d_core\n");
#endif
	//log_conv_param(p_conv2d_entry);
 	load_weight_bias_shift(&p_conv2d_entry->tiled_param_host.Tilingwithdev.bias_rmt_addr,p_conv2d_entry->tiled_param_host.Tilingwithdev.bias_mem_size,p_conv2d_entry->tiled_param_dev.bias_lcmem_addr,
	                        &p_conv2d_entry->tiled_param_host.Tilingwithdev.shift_rmt_addr,p_conv2d_entry->tiled_param_host.Tilingwithdev.shift_mem_size,p_conv2d_entry->tiled_param_dev.shift_lcmem_addr,
							&p_conv2d_entry->tiled_param_host.Tilingwithdev.wt_rmt_addr,p_conv2d_entry->tiled_param_host.Tilingwithdev.wt_mem_size,p_conv2d_entry->tiled_param_dev.wt_lcmem_addr);
	
	 PTConv2dOneRowHPU200 one_ofm_row_param_for_prereading={0};
	//  one_ofm_row_param_for_prereading=prepare_param(p_conv2d_entry,conv2d_whole,0);
	//  makesure_needed_ifm_rows_ready(one_ofm_row_param_for_prereading);//read new lines
	 PTConv2dOneRowHPU200 one_ofm_row_param_for_caculating={0};
	 
	prepare_param(&one_ofm_row_param_for_prereading,p_conv2d_entry,conv2d_whole,0);
	{
	TilingIfmRowInfo ifm_row_tiled[one_ofm_row_param_for_prereading.ifm_prefetch_info.prefetch_row_count];
	prepare_preline(ifm_row_tiled,&one_ofm_row_param_for_prereading,p_conv2d_entry,conv2d_whole,0);
	makesure_needed_ifm_rows_ready(one_ofm_row_param_for_prereading,ifm_row_tiled);//read new lines
	}
	one_ofm_row_param_for_caculating=one_ofm_row_param_for_prereading;
	// one_ofm_row_param_for_caculating=one_ofm_row_param_for_prereading;
	for(int h_iter_num=0;h_iter_num<p_conv2d_entry->tiled_param_host.tiling_info.conv2d_for_block.cshape.ofm_h;h_iter_num++)
	{

		if(one_ofm_row_param_for_caculating.ifm_prefetch_info.prefetch_enable&&h_iter_num<p_conv2d_entry->tiled_param_host.tiling_info.conv2d_for_block.cshape.ofm_h-1)
		{
			one_ofm_row_param_for_caculating=one_ofm_row_param_for_prereading;
			prepare_param(&one_ofm_row_param_for_prereading,p_conv2d_entry,conv2d_whole,h_iter_num+1);
			{
			TilingIfmRowInfo ifm_row_tiled[one_ofm_row_param_for_prereading.ifm_prefetch_info.prefetch_row_count];
			prepare_preline(ifm_row_tiled,&one_ofm_row_param_for_prereading,p_conv2d_entry,conv2d_whole,h_iter_num+1);
			makesure_needed_ifm_rows_ready(one_ofm_row_param_for_prereading,ifm_row_tiled);//read new lines
			}
		}
		else if(!one_ofm_row_param_for_caculating.ifm_prefetch_info.prefetch_enable&&h_iter_num>0)//h_iter_num>0&&h_iter_num<p_conv2d_entry->tiled_param.tiling_info.ifm_h_tiled)
		{
			prepare_param(&one_ofm_row_param_for_prereading,p_conv2d_entry,conv2d_whole,h_iter_num);
			{
			TilingIfmRowInfo ifm_row_tiled[one_ofm_row_param_for_prereading.ifm_prefetch_info.prefetch_row_count];
			prepare_preline(ifm_row_tiled,&one_ofm_row_param_for_prereading,p_conv2d_entry,conv2d_whole,h_iter_num);
			makesure_needed_ifm_rows_ready(one_ofm_row_param_for_prereading,ifm_row_tiled);//read new lines
			one_ofm_row_param_for_caculating=one_ofm_row_param_for_prereading;
			}
		}
		else if(one_ofm_row_param_for_caculating.ifm_prefetch_info.prefetch_enable&&h_iter_num==p_conv2d_entry->tiled_param_host.tiling_info.conv2d_for_block.cshape.ofm_h-1)
		{
			one_ofm_row_param_for_caculating=one_ofm_row_param_for_prereading;
		}
		
		//KRNL_LOG_INFO(LOG_SYSTEM,".rmt_ofm_addr_tiled.lcaddr:%x",one_ofm_row_param->rmt_ofm_addr_tiled.lcaddr);
		//buf_print((uint32_t)&one_ofm_row_param, ALIGN(sizeof(PTConv2dOneRowHPU200),64));
		//_pParamTable->param[tiling_block].one_ofm_row_param .h_iter_tiled=0;//not used
		
		
		//prefetch_row_count
		//top(first time)
		

	#ifdef QEMU_ENV
		// printf("[conv param] ifm_h: %d, ifm_w: %d, ifm_c: %d, ofm_c: %d, k_h: %d, k_w: %d\n", p_conv2d_entry->conv2d.cshape.ifm_h, p_conv2d_entry->conv2d.cshape.ifm_w, p_conv2d_entry->conv2d.cshape.ifm_c, p_conv2d_entry->conv2d.cshape.ofm_c, p_conv2d_entry->conv2d.cshape.k_h, p_conv2d_entry->conv2d.cshape.k_w);
		// printf("[pad param] top: %d, bottom: %d, side: %d\n", p_conv2d_entry->conv2d.pshape.top, p_conv2d_entry->conv2d.pshape.bottom, p_conv2d_entry->conv2d.pshape.side);
		// printf("[stride param] w_stride: %d, h_stride: %d\n", p_conv2d_entry->conv2d.strd_shape.w_strd, p_conv2d_entry->conv2d.strd_shape.h_strd);
	#endif
		// KRNL_LOG_INFO(LOG_SYSTEM, "into makesure_needed_ifm_ready");

	// KRNL_LOG_INFO(LOG_SYSTEM, "out makesure_needed_ifm_ready");

		// KRNL_LOG_INFO(LOG_SYSTEM, "***=====local_ifm H iter: %d / %d=====\n\r", h_iter_num, p_conv2d_entry->tiled_param_host.tiling_info.ifm_h_tiled);
		// __ndma_poll();
		// wait(100000);
		// if(h_iter_num+1<p_conv2d_entry->tiled_param.tiling_info.ifm_h_tiled&&one_ofm_row_param->ifm_prefetch_info.prefetch_enable)
		// {
		// 	makesure_needed_ifm_rows_ready(h_iter_num+1, &one_ofm_row_param);
		// }
		// else if(h_iter_num>0&&h_iter_num<p_conv2d_entry->tiled_param.tiling_info.ifm_h_tiled)
		// {
		// 	makesure_needed_ifm_rows_ready(h_iter_num, &one_ofm_row_param);
		// 	//__ndma_poll();
		// }
		
		//__ndma_poll();
	#ifdef OPT_ONE_ROW_CONV
		//KRNL_LOG_INFO(LOG_SYSTEM, "into onerowconv");

		// KRNL_LOG_INFO(LOG_SYSTEM, "wt_start_addr_tiled_for_instructions:[%x]",one_ofm_row_param->wt_start_addr_tiled_for_instructions);
		// KRNL_LOG_INFO(LOG_SYSTEM, "ofm_start_addr_tiled_for_instructions:[%x]",p_conv2d_entry->tiled_param.ofm_start_addr_tiled_for_instructions);
		// KRNL_LOG_INFO(LOG_SYSTEM, "bias_start_addr_tiled_for_instructions:[%x]",p_conv2d_entry->tiled_param.bias_start_addr_tiled_for_instructions);
		// KRNL_LOG_INFO(LOG_SYSTEM, "shift_start_addr_tiled_for_instructions:[%x]",p_conv2d_entry->tiled_param.shift_start_addr_tiled_for_instructions);
		// KRNL_LOG_INFO(LOG_SYSTEM, "param_mmac_region_start:[%x]",p_conv2d_entry->tiled_param.param_mmac_region_start);
		// KRNL_LOG_INFO(LOG_SYSTEM, "param_mmac_region_end:[%x]",p_conv2d_entry->tiled_param.param_mmac_region_end);
		// KRNL_LOG_INFO(LOG_SYSTEM, "param_mmac_ifm_cluster_stride:[%x]",p_conv2d_entry->tiled_param.param_mmac_ifm_cluster_stride);
		// KRNL_LOG_INFO(LOG_SYSTEM, "param_mmac_cluster_start:[%x]",one_ofm_row_param.param_mmac_cluster_start);
		// KRNL_LOG_INFO(LOG_SYSTEM, "param_mmac_cluster_end:[%x]",one_ofm_row_param.param_mmac_cluster_end);
		// KRNL_LOG_INFO(LOG_SYSTEM, "param_mmac_cluster_num:[%x]",one_ofm_row_param.param_mmac_cluster_num);
		// if(h_iter_num==p_conv2d_entry->tiled_param_host.tiling_info.conv2d_for_block.cshape.ofm_h-1)
		// {
		// 	g_ulPrintDebugLogFlag=1;
		// }
		// else
		// {
		// 	g_ulPrintDebugLogFlag=0;
		// }

		if(p_conv2d_entry->tiled_param_host.tiling_info.tiling_ifm_w_partype==kTilingIfmWRight)
		{
			one_row_conv(h_iter_num, TILING_PART_RIGHT, 0, &p_conv2d_entry->tiled_param_host.tiling_info.conv2d_for_block,\
			ByteToHPU64BytesWord(one_ofm_row_param_for_caculating.wt_start_addr_tiled_for_instructions),\
			ByteToHPU64BytesWord(p_conv2d_entry->tiled_param_dev.ofm_start_addr_tiled_for_instructions),\
			ByteToHPU64BytesWord(p_conv2d_entry->tiled_param_dev.bias_start_addr_tiled_for_instructions),\
			ByteToHPU64BytesWord(p_conv2d_entry->tiled_param_dev.shift_start_addr_tiled_for_instructions),\
			ByteToHPU64BytesWord(p_conv2d_entry->tiled_param_dev.param_mmac_region_start),\
			ByteToHPU64BytesWord(p_conv2d_entry->tiled_param_dev.param_mmac_region_end),\
			ByteToHPU64BytesWord(p_conv2d_entry->tiled_param_dev.param_mmac_ifm_cluster_stride),\
			ByteToHPU64BytesWord(one_ofm_row_param_for_caculating.param_mmac_cluster_start),\
			ByteToHPU64BytesWord(one_ofm_row_param_for_caculating.param_mmac_cluster_end),\
			(one_ofm_row_param_for_caculating.param_mmac_cluster_num),\
			input_conv_type,1);
		}
		else
		{
			one_row_conv(h_iter_num, TILING_NONE, 0, &p_conv2d_entry->tiled_param_host.tiling_info.conv2d_for_block,\
			ByteToHPU64BytesWord(one_ofm_row_param_for_caculating.wt_start_addr_tiled_for_instructions),\
			ByteToHPU64BytesWord(p_conv2d_entry->tiled_param_dev.ofm_start_addr_tiled_for_instructions),\
			ByteToHPU64BytesWord(p_conv2d_entry->tiled_param_dev.bias_start_addr_tiled_for_instructions),\
			ByteToHPU64BytesWord(p_conv2d_entry->tiled_param_dev.shift_start_addr_tiled_for_instructions),\
			ByteToHPU64BytesWord(p_conv2d_entry->tiled_param_dev.param_mmac_region_start),\
			ByteToHPU64BytesWord(p_conv2d_entry->tiled_param_dev.param_mmac_region_end),\
			ByteToHPU64BytesWord(p_conv2d_entry->tiled_param_dev.param_mmac_ifm_cluster_stride),\
			ByteToHPU64BytesWord(one_ofm_row_param_for_caculating.param_mmac_cluster_start),\
			ByteToHPU64BytesWord(one_ofm_row_param_for_caculating.param_mmac_cluster_end),\
			(one_ofm_row_param_for_caculating.param_mmac_cluster_num),\
			input_conv_type,1);
		}
		// qemu_fprint(QEMU_LOG_MMAB, ByteToHPU64BytesWord(p_conv2d_entry->tiled_param_dev.bias_start_addr_tiled_for_instructions), 1024);
		// qemu_fprint(QEMU_LOG_MMAB, ByteToHPU64BytesWord(p_conv2d_entry->tiled_param_dev.shift_start_addr_tiled_for_instructions), 256);
		// qemu_fprint(QEMU_LOG_MMAB, ByteToHPU64BytesWord(p_conv2d_entry->tiled_param_dev.ofm_start_addr_tiled_for_instructions), 4096);
		
	#else

		one_row_conv(h_iter,&p_conv2d_entry->conv2d_for_block,\
		            ByteToHPU64BytesWord(p_conv2d_entry->one_ofm_row_param[h_iter].wt_start_addr_tiled_for_instructions),\
					ByteToHPU64BytesWord(p_conv2d_entry->one_ofm_row_param[h_iter].ofm_start_addr_tiled_for_instructions),\
					ByteToHPU64BytesWord(p_conv2d_entry->one_ofm_row_param[h_iter].bias_start_addr_tiled_for_instructions),\
					ByteToHPU64BytesWord(p_conv2d_entry->one_ofm_row_param[h_iter].shift_start_addr_tiled_for_instructions),\
					ByteToHPU64BytesWord(p_conv2d_entry->tiled_param.param_mmac_region_start),\
					ByteToHPU64BytesWord(p_conv2d_entry->tiled_param.param_mmac_region_end),\
					ByteToHPU64BytesWord(p_conv2d_entry->tiled_param.param_mmac_ifm_cluster_stride),\
					ByteToHPU64BytesWord(p_conv2d_entry->one_ofm_row_param[h_iter].param_mmac_cluster_start),\
					ByteToHPU64BytesWord(p_conv2d_entry->one_ofm_row_param[h_iter].param_mmac_cluster_end),\
					ByteToHPU64BytesWord(p_conv2d_entry->one_ofm_row_param[h_iter].param_mmac_cluster_num),\
					input_conv_type,1);
	#endif		

		//if(p_conv2d_entry->one_ofm_row_param[h_iter].need_to_xfr_ofm){
		if(p_conv2d_entry->tiled_param_host.tiling_info.c==-1)
		{
			LIBHIKL_NASSERT(__wr_to_remote_chunk_blocking((uint32 *)(p_conv2d_entry->tiled_param_dev.ofm_start_addr_tiled_for_instructions+MMA_START_ADDR),\
															 one_ofm_row_param_for_caculating.rmt_ofm_addr_tiled.x_pos,\
															 one_ofm_row_param_for_caculating.rmt_ofm_addr_tiled.y_pos,\
															 one_ofm_row_param_for_caculating.rmt_ofm_addr_tiled.lcaddr, \
															 (one_ofm_row_param_for_caculating.one_row_ofm_size)));
		}
		else
		{
			for(int i=0;i<(p_conv2d_entry->tiled_param_host.tiling_info.conv2d_for_block.cshape.ofm_w>>3);i++)
			{
				(__wr_to_remote_chunk_blocking((uint32 *)(p_conv2d_entry->tiled_param_dev.ofm_start_addr_tiled_for_instructions+MMA_START_ADDR+i*p_conv2d_entry->tiled_param_host.tiling_info.conv2d_for_block.cshape.ofm_c*8),\
															 one_ofm_row_param_for_caculating.rmt_ofm_addr_tiled.x_pos,\
															 one_ofm_row_param_for_caculating.rmt_ofm_addr_tiled.y_pos,\
															 one_ofm_row_param_for_caculating.rmt_ofm_addr_tiled.lcaddr+i*conv2d_whole.cshape.ofm_c*8, \
															 (p_conv2d_entry->tiled_param_host.tiling_info.conv2d_for_block.cshape.ofm_c*8)));
			}
		}
		// KRNL_LOG_INFO(LOG_SYSTEM, "rmt_ofm_addr_tiled.lcaddr:[%x]",one_ofm_row_param_for_caculating.rmt_ofm_addr_tiled.lcaddr);
		// KRNL_LOG_INFO(LOG_SYSTEM, "now_output");
    	//buf_print(MMA_START_ADDR + p_conv2d_entry->tiled_param.ofm_start_addr_tiled_for_instructions, GMEM_ALIGN(p_conv2d_entry->one_ofm_row_param[h_iter].one_row_ofm_size));
		//KRNL_LOG_INFO(LOG_SYSTEM, "ddr_addr:%i%i%i,size:%i",p_conv2d_entry->one_ofm_row_param[h_iter].rmt_ofm_addr_tiled.x_pos,p_conv2d_entry->one_ofm_row_param[h_iter].rmt_ofm_addr_tiled.y_pos,p_conv2d_entry->one_ofm_row_param[h_iter].rmt_ofm_addr_tiled.lcaddr,p_conv2d_entry->one_ofm_row_param[h_iter].one_row_ofm_size);
		//}
	}
	return;
}

// static void kernel_conv2d_single_layer()
// {
// 	init_static_var();

// 	PTConv2ds *_pParamTable = *((PTConv2ds **)HIPU200_KNL_PTABLE_ADDR);/*get kernel param table from runtime*/
// 	PTConv2dEntryHPU200 *p_op_entry = &_pParamTable->param[0];
// 	conv2d_core(p_op_entry, CONV_TYPE_CLASSIC);

// }


void conv2d_byoc()
{
	// KRNL_LOG_INFO(LOG_DEBUG, "Successfulling into conv2d_byoc!");
	init_static_var();


#ifdef QEMU_ENV
    qemu_arch_setup();
	printf("=============================\n\r");
    printf("YOU ARE USING A QEMU INSTANCE\n\r");
    printf("=============================\n\r");
#endif

	// get paramTable for conv2d
	PTConv2ds *_pParamTable = *((PTConv2ds **)HIPU200_KNL_PTABLE_ADDR);/*get kernel param table from runtime*/
	base_table = &_pParamTable->infoBase;
	unsigned int *_flags = ( unsigned int *)HIPU200_MEM_ATOMIC_START;
	LIBHIKL_NASSERT(base_table->task_dim > HIPU200_SOC_CORE_NUM);
	LIBHIKL_NASSERT(base_table->task_dim == 0);
	// Get the core ID
	unsigned int _coreid = _flags[0];
	// unsigned int _coreid = get_core_id();

    int kernel_id = -1;
	// Get the position in the kernel and find the right param table
	unsigned int _taskid = 255;
	int *_rtcode = (int *)HIPU200_KNL_RTCODE_ADDR;
	uint32_t input_conv_type;
	if(1)//_pParamTable->count == 1)
	{
		for(int tiling_block=0;tiling_block<_pParamTable->count;tiling_block++)
		{

			switch(base_table->op_type)
			{
				case KERNEL_OP_DWCONV_BYOC:
					input_conv_type = CONV_TYPE_DEPTH_WISE;
				break;
				case KERNEL_OP_CONV2D_BYOC:
					// KRNL_LOG_INFO(LOG_DEBUG, "into KERNEL_OP_CONV2D");
					input_conv_type = CONV_TYPE_CLASSIC;
				break;
			}

			//init_static_var();
			conv_table = &_pParamTable->param[tiling_block];
			//only support w_stride=1 or 2
			ifm_rm_ptr=_pParamTable->param[tiling_block].tiled_param_host.Tilingwithdev.ifm_rmt_addr.lcaddr;
			ofm_rm_ptr=_pParamTable->param[tiling_block].tiled_param_host.Tilingwithdev.ofm_rmt_addr.lcaddr;
			
			_pParamTable->param[tiling_block].tiled_param_dev.param_mmac_mtx_pad_type=1;//not used
			_pParamTable->param[tiling_block].tiled_param_dev.param_mmac_region_start=LOCAL_IFM_BLOCK-MMA_START_ADDR;
			_pParamTable->param[tiling_block].tiled_param_dev.param_mmac_region_end=LOCAL_IFM_BLOCK+IFM_BANK_NUMBER*MMA_BANK_SIZE-MEM_LCMEM_ADDR_S;
			_pParamTable->param[tiling_block].tiled_param_dev.param_mmac_ifm_cluster_stride=MMA_BANK_SIZE;
			_pParamTable->param[tiling_block].tiled_param_dev.bias_start_addr_tiled_for_instructions=LOCAL_BIAS_SHIFT-MMA_START_ADDR;
			_pParamTable->param[tiling_block].tiled_param_dev.shift_start_addr_tiled_for_instructions=LOCAL_BIAS_SHIFT+_pParamTable->param[tiling_block].tiled_param_host.Tilingwithdev.bias_mem_size-MMA_START_ADDR;
			_pParamTable->param[tiling_block].tiled_param_dev.ofm_start_addr_tiled_for_instructions=LOCAL_OFM_BLOCK-MMA_START_ADDR;
			_pParamTable->param[tiling_block].tiled_param_dev.wt_lcmem_addr=LOCAL_WEIGHT_BLOCK;
			_pParamTable->param[tiling_block].tiled_param_dev.bias_lcmem_addr=LOCAL_BIAS_SHIFT;
			_pParamTable->param[tiling_block].tiled_param_dev.shift_lcmem_addr=LOCAL_BIAS_SHIFT+_pParamTable->param[tiling_block].tiled_param_host.Tilingwithdev.bias_mem_size;
			// KRNL_LOG_INFO(LOG_SYSTEM, "ifm_rm_ptr:%x",ifm_rm_ptr);
			// KRNL_LOG_INFO(LOG_SYSTEM, "ofm_rm_ptr:%x",ofm_rm_ptr);
			// KRNL_LOG_INFO(LOG_DEBUG, "conv_table: 0x%x", conv_table);
			conv2d_core(conv_table, input_conv_type,_pParamTable->conv2d_whole);
		}
    	
	}
	// else{
	// 	//new
 	//    for(int i=0; i <1;i++){//_pParamTable->count; i++){
 	//        if(base_table->task_cores[i] == _coreid){
 	//            kernel_id = i;
 	//            conv_table = &_pParamTable->param[i];
 	//            break;
 	//         }
 	//     }
	// }

#ifdef QEMU_ENV
	qemu_fprint(QEMU_LOG_MMAB, 0x38000>>6, 1024);
	qemu_fprint(QEMU_LOG_MMAB, 0x38400>>6, 256);
	qemu_fprint(QEMU_LOG_MEM, conv_table->tiled_param_host.Tilingwithdev.ofm_rmt_addr.lcaddr, 327680);
#endif
}




void timer_handler(){
}