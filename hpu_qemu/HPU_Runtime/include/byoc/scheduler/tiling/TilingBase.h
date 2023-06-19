/*
 * @Descripttion: 
 * @version: 
 * @Author: AlonzoChen
 * @Date: 2021-08-25 12:11:42
 * @LastEditors: AlonzoChen
 * @LastEditTime: 2021-08-25 12:14:10
 */
#ifndef __TILINGBASE__H__
#define __TILINGBASE__H__

class TilingBase
{
public:
	virtual void Tiling(int core_num, void *pt_in, void *pt_out) = 0;

};

#endif  //!__TILINGBASE__H__