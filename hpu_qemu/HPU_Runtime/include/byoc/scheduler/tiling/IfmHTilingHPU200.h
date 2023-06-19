/*
 * @Descripttion: 
 * @version: 
 * @Author: AlonzoChen
 * @Date: 2021-08-25 12:14:34
 * @LastEditors: AlonzoChen
 * @LastEditTime: 2021-08-25 13:18:25
 */

#ifndef __IfmHTilingHPU200__H__
#define __IfmHTilingHPU200__H__

#include "byoc/scheduler/tiling/TilingBase.h"

class IfmHTilingHPU200 : public TilingBase
{
public:
	void Tiling(int core_num, void *pt_in, void *pt_out);

};

#endif  //!__IfmHTilingHPU200__H__