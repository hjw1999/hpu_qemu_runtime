/*
 * @Descripttion: 
 * @version: 
 * @Author: AlonzoChen
 * @Date: 2021-08-25 12:14:34
 * @LastEditors: AlonzoChen
 * @LastEditTime: 2021-08-25 14:57:22
 */

#ifndef __IfmCTilingHPU200__H__
#define __IfmCTilingHPU200__H__

#include "byoc/scheduler/tiling/TilingBase.h"

class IfmCTilingHPU200 : public TilingBase
{
public:
	void Tiling(int core_num, void *pt_in, void *pt_out);

};

#endif  //!__IfmCTilingHPU200__H__