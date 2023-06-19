/*
 * @Descripttion: 
 * @version: 
 * @Author: AlonzoChen
 * @Date: 2021-08-25 12:03:54
 * @LastEditors: AlonzoChen
 * @LastEditTime: 2021-08-25 12:06:54
 */

#ifndef __CAGBYOCSCHEDULER__H__
#define __CAGBYOCSCHEDULER__H__

class CAGByocScheduler
{
public:
	virtual void Tiling() = 0;
	virtual void Schedule() = 0;
};

#endif  //!__CAGBYOCSCHEDULER__H__