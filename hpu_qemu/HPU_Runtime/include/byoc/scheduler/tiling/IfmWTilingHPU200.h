/*
 * @Descripttion: 
 * @version: 
 * @Author: AlonzoChen
 * @Date: 2021-08-25 12:14:34
 * @LastEditors: AlonzoChen
 * @LastEditTime: 2021-08-25 12:18:25
 */

#ifndef __IFMWTILINGHPU200__H__
#define __IFMWTILINGHPU200__H__

#include "byoc/scheduler/tiling/TilingBase.h"

class IfmWTilingHPU200 : public TilingBase
{
public:
	void Tiling(int core_num, void *pt_in, void *pt_out);

};

#endif  //!__IFMWTILINGHPU200__H__