/*
 * @Descripttion:
 * @version:
 * @Author: AlonzoChen
 * @Date: 2021-01-14 18:21:52
 * @LastEditors: AlonzoChen
 * @LastEditTime: 2021-05-25 18:12:57
 */
#ifndef _HISDK_LOG_H__
#define _HISDK_LOG_H__

#include <stdio.h>
#include <string.h>
#include "hisdk_type.h"
#include "hisdk_config.h"

#ifdef __cplusplus
extern "C" {
#endif

#define STR_COMM_SIZE               128
#define STR_MAX_SIZE                1024

#define MAX_LOG_FILE_NUM		    (3)

#define NUMBER(type)                sizeof(type)/sizeof(type[0])

#define __FILENAME__                (strrchr(__FILE__, '/') ? (strrchr(__FILE__, '/') + 1) : __FILE__)

enum
{
    LOG_DEBUG = 0,  
    LOG_ERROR,      
    LOG_WARNING,    
    LOG_ACTION,     
    LOG_SYSTEM,     
    LOG_HARDWARE_CMD,
    BOTTOM
};

#define HISDK_LOG_TO_TERM           (0)
#define HISDK_LOG_TO_FILE           (1)

#define HISDK_LOG_DEBUG_DISABLE     (0)
#define HISDK_LOG_DEBUG_ENABLE      (1)


#define DEBUG_PRINT                 (1)
#define LOG_PRINT(fmt, ...) do{\
    	if(DEBUG_PRINT)\
    	{\
    		printf(fmt"  [line:%d] [%s]\n", ##__VA_ARGS__, __LINE__, __FUNCTION__);\
    	}\
    }while(0);

#define LOG_ERR(fmt, ...) do{\
	    printf("[ERROR]  "fmt"  [line:%d] [%s]\n", ##__VA_ARGS__, __LINE__, __FUNCTION__);\
    }while(0);



extern unsigned long g_ulPrintLogPlaceFlag;
extern unsigned long g_ulPrintDebugLogFlag;

extern unsigned long g_ulPrintHardwareCmdLogFlag;
unsigned long hisdkLOGPrintLog(unsigned char ucType, unsigned char *pucLogInfo);

#ifdef HISDK_DEBUG
#define HISDK_LOG_INFO(type, fmt, ...) do{\
	if(HISDK_LOG_TO_TERM == g_ulPrintLogPlaceFlag) \
	{ \
		if((0 == g_ulPrintDebugLogFlag) && (LOG_DEBUG == type)) \
		{\
			break;\
		}\
		if((0 == g_ulPrintHardwareCmdLogFlag) && (LOG_HARDWARE_CMD == type)) \
		{\
			break;\
		}\
    	unsigned char ucLogInfo[STR_MAX_SIZE] = {0}; \
		snprintf((char *)ucLogInfo, sizeof(ucLogInfo) - 1, fmt" [%s.%d][%s()]\n", ##__VA_ARGS__, __FILENAME__, __LINE__, __FUNCTION__); \
		hisdkLOGPrintLog(type, ucLogInfo); \
	} \
	else \
	{ \
		unsigned char ucLogInfo[STR_MAX_SIZE] = {0}; \
		snprintf((char *)ucLogInfo, sizeof(ucLogInfo) - 1, fmt" [%s.%d][%s()]\n", ##__VA_ARGS__, __FILENAME__, __LINE__, __FUNCTION__); \
		hisdkLOGPrintLog(type, ucLogInfo); \
	} \
    }while(0)
#else
	#define HISDK_LOG_INFO(type, fmt, ...)
#endif

extern void          hisdkLOGSetPrintDebugLogFlag(unsigned long flag);
extern void          hisdkLOGSetPrintLogPlaceFlag(unsigned long flag);
extern unsigned long hisdkLOGInit(const unsigned char *ucLogFileName, unsigned long ulFileSize);
extern void          hisdkLOGDestroy(void);


#ifdef __cplusplus
}
#endif
#endif //_HISDK_LOG_H__


