/*
 * @Descripttion: 
 * @version: 
 * @Author: AlonzoChen
 * @Date: 2020-12-14 16:47:43
 * @LastEditors: AlonzoChen
 * @LastEditTime: 2021-05-12 19:28:03
 */
#ifndef __HISDK_UTILS_H__
#define __HISDK_UTILS_H__

#include "hisdk_type.h"

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

void loadFromCAGParamFileBuf(char* input_file_buf, size_t file_size, char *host_memBuf, int buf_size);
void loadFromCAGParamFileBufForBiasAndShift(char* input_file_buf, size_t file_size, char *out_host_memBS, char *out_host_memShift);
void OutputHexBufToCAGParamFileFormat(const unsigned char* input_hex_buf, size_t buf_size, const char *output_file_name);
void hisdkPrintHpuBuf(const char* host_mem_buf, int buf_len);

int getOriginsResult(int ofm_c_idx, char *host_mem_ifm, char *host_mem_wt, const int ifm_c);
void printOriginsResultForFC(char *host_mem_ifm, char *host_mem_wt, const int ofm_c, const int ifm_c);
// new function
void loadFromCAGParamFileBufForBiasAndShiftForFC(char* input_file_buf, size_t file_size, char *out_host_memBS, char *out_host_memShift);

//为仿真而dump文件
void hisdkDumpData2FileForSimulation(uint64_t addr, uint64_t size, void *data);
//为QEMU而dump文件
void hisdkDumpData2FileForQemu(const char* src_file_path, uint64_t size, void *data, uint64_t dst_physical_gaddr, uint32_t dst_hpu_local_addr, const char* extra_tag_name = "");
void hisdkDumpData2OtherFileForQemu(uint64_t size, void *data, uint64_t dst_physical_gaddr, uint32_t dst_hpu_local_addr, const char* extra_tag_name);

void initProfiling();
void hisdkPrintProfiling();

#ifdef __cplusplus
}
#endif
#endif /*__HISDK_UTILS_H__*/

