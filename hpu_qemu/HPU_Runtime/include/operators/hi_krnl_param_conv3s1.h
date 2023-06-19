/*
 * @Descripttion: 
 * @version: 
 * @Author: AlonzoChen
 * @Date: 2020-12-30 14:31:23
 * @LastEditors: AlonzoChen
 * @LastEditTime: 2021-03-29 11:04:56
 */
#ifndef __LIBHI_KRNL_PARAM_CONV3S1_H__
#define __LIBHI_KRNL_PARAM_CONV3S1_H__

#include "hi_krnl_param.h"
#include "hi_krnl_param_conv2d.h"

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus


//                       /                       
//                       |                       
//                       |                       
//                       |                       
//                /------\--------\              
//                |               |              
//                |               |              
//                |     conv1     |              
//                |      1s1      |              
//                |               |              
//                \------.--------/              
//                     .`  `'                    
//                   ,'      `.                  
//                 ,'          `.                
//               ,-              `,              
//              -                  ',            
//            ,'                     ',          
//  /--------`-------\        /--------'-------\ 
//  |                |        |                | 
//  |                |        |                | 
//  |    conv1a_c1   |        |   conv1b_r1    | 
//  |      3s1       |        |       3s1      | 
//  |                |        |                | 
//  \----------------/        \----------------/ 

typedef struct
{
    conv2d_params_t conv1;

    hikl_addr_t wt_addr_conv1;
    hikl_addr_t bias_addr_conv1;
    hikl_addr_t shift_addr_conv1;
    
    hikl_addr_t ifm_addr_conv1;
    hikl_addr_t ifm_addr_pleft_conv1;
    hikl_addr_t ifm_addr_pright_conv1;
    uint32_t ifm_addr_linestrd_conv1;

    hikl_addr_t ofm_addr_conv1;
    uint32_t ofm_addr_linestrd_conv1;

    uint32_t tiling_parttype;
}paramTableConv3s1_Entry_t;


#ifdef __cplusplus
}
#endif
#endif /*__LIBHI_KRNL_PARAM_CONV3S1_H__*/
