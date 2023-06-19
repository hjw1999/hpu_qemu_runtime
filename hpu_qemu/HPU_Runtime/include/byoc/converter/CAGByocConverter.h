/*
 * @Descripttion: 
 * @version: 
 * @Author: AlonzoChen
 * @Date: 2021-08-25 12:00:40
 * @LastEditors: AlonzoChen
 * @LastEditTime: 2021-08-25 12:06:39
 */

#ifndef __CAGBYOCCONVERTER__H__
#define __CAGBYOCCONVERTER__H__


class CAGByocConverter
{
public:
	virtual void ConvertFromRelayIRToNetwork() = 0;
	virtual void Optimise() = 0;
	virtual void Schedule() = 0;


};

#endif  //!__CAGBYOCCONVERTER__H__