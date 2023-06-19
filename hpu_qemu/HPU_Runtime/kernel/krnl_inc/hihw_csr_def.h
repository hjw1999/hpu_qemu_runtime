/*
 * @Description: csr
 * @version: 
 * @Author: AlonzoChen
 * @Date: 2020-12-29 14:33:45
 * @LastEditors: AlonzoChen
 * @LastEditTime: 2021-05-29 11:34:30
 */
#ifndef __HIPU200CLIB_CSR_REG_DEF_H__
#define __HIPU200CLIB_CSR_REG_DEF_H__

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

// global definition
#define CSR_HPU_ID                 0xf15

// -----
// Hardware Performance Monitor

// Instruction commit events, mhpeventX[7:0] = 0
// ------------------------------------------
// Bits | Meaning
// 8    | exception taken
// 9    | integer load instruction retired
// 10   | integer store instruction retired
// 11   | atomic memory operation retired
// 12   | system instruction retired
// 13   | integer arithmetic instruction retired
// 14   | conditional branch retired
// 15   | JAL instruction retired
// 16   | JALR instruction retired
// 17   | integer multiplication instruction retired
// 18   | integer division instruction retired
// 26   | vector instruction retired
// 27   | vector CSR instruction retired
// 28   | matrix instruction retired

// Micro-architectural events, mhpeventX[7:0] = 1
// ------------------------------------------
// Bits | Meaning
// 13   | Branch direction misprediction
// 14   | Branch/jump target misprediction
// 15   | Pipeline flush from CSR write
// 16   | Pipeline flush from other event

// Profiling
#define CSR_ADDR_MCT_CYCLE_L                        (0xC00)
#define CSR_ADDR_MCT_TIME_L                         (0xC01)
#define CSR_ADDR_MCT_INSTRET                        (0xC02)
#define CSR_ADDR_MCT_HPMCNT03                       (0xC03)
#define CSR_ADDR_MCT_HPMCNT04                       (0xC04)
#define CSR_ADDR_MCT_HPMCNT05                       (0xC05)
#define CSR_ADDR_MCT_HPMCNT06                       (0xC06)
#define CSR_ADDR_MCT_HPMCNT07                       (0xC07)
#define CSR_ADDR_MCT_CYCLE_H                        (0xC80)
#define CSR_ADDR_MCT_TIME_H                         (0xC81)
#define CSR_ADDR_MCT_INSTRET_H                      (0xC82)
#define CSR_ADDR_MCT_HPMCNT03_H                     (0xC83)
#define CSR_ADDR_MCT_HPMCNT04_H                     (0xC84)
#define CSR_ADDR_MCT_HPMCNT05_H                     (0xC85)
#define CSR_ADDR_MCT_HPMCNT06_H                     (0xC86)
#define CSR_ADDR_MCT_HPMCNT07_H                     (0xC87)

#define CSR_ADDR_MCS_MCNTINHIBIT                    (0x320)
#define CSR_ADDR_MCS_MHPMEVENT03                    (0x323)
#define CSR_ADDR_MCS_MHPMEVENT04                    (0x324)
#define CSR_ADDR_MCS_MHPMEVENT05                    (0x325)
#define CSR_ADDR_MCS_MHPMEVENT06                    (0x326)
#define CSR_ADDR_MCS_MHPMEVENT07                    (0x327)

// HPM

// -- Scalar region
// -- NoC DMA

#define CSR_ADDR_NDMA_CTRL                      (0x7C0)
#define CSR_ADDR_NDMA_STATUS                    (0x7C1)
#define CSR_ADDR_NDMA_LCADDR                    (0x7C2)
#define CSR_ADDR_NDMA_RTADDR                    (0x7C3)
#define CSR_ADDR_NDMA_SIZE                      (0x7C4)
#define CSR_ADDR_NDMA_DESTXY                    (0x7C5)

//标量指令，向量（矩阵）指令，ndma 之间没有严格的保序关系，向量（矩阵）指令必定落后于标量指令的执行，
//如果标量指令的执行（ndma也是由标量指令发起的），依赖于向量（矩阵）指令的输出，则必须查询：CSR_ADDR_VMU_STATUS（0x7cf）== 0x 7,如果成立，说明向量（矩阵）指令执行完成
#define CSR_ADDR_VMU_STATUS                     (0x7Cf)   //readonly, == 0x7: now vector or matrix instructions have finished
#define vmu_poll()              do{ uint32_t poll = 0;                      \
                                    do{                                     \
                                        csrr(poll, CSR_ADDR_VMU_STATUS);    \
                                    }while((poll) != (0x7));                \
                                }while(0)

// -- VMU
#define CSR_VMU_STATUS             0x7cf

// -- VCSR region
// -- Vec
#define CSR_ROUND_TYPE             0xbc0
#define CSR_FADD_SHIFT_NUM         0xbc1
#define CSR_FADD_PROT_HIGH         0xbc2
#define CSR_FADD_PROT_LOW          0xbc3
#define CSR_FSUB_SHIFT_NUM         0xbc4
#define CSR_FSUB_PROT_HIGH         0xbc5
#define CSR_FSUB_PROT_LOW          0xbc6
#define CSR_FMUL_SHIFT_NUM         0xbc7
#define CSR_FMUL_PROT_HIGH         0xbc8
#define CSR_FMUL_PROT_LOW          0xbc9

// -- Mtx
#define CSR_MTX_CLUSTER_START      0xbd0
#define CSR_MTX_CLUSTER_END        0xbd1
#define CSR_MTX_REGION_START       0xbd2
#define CSR_MTX_REGION_END         0xbd3
#define CSR_MTX_BLK_SIZE           0xbd4
#define CSR_MTX_BLK_NUM            0xbd5
#define CSR_MTX_CLS_NUM            0xbd6
#define CSR_MTXRW_BLK_STRIDE       0xbd7
#define CSR_MTXRW_CLS_STRIDE       0xbd8
#define CSR_MTXRO_BLK_STRIDE       0xbd9
#define CSR_MTXRO_CLS_STRIDE       0xbda
#define CSR_MTX_PAD_TYPE           0xbdb

#define CSR_WR_LUT_FIRST                        (0xBE0)
#define CSR_WR_LUT_INC                          (0xBE1)
#define CSR_FPRINT_ADDR                         (0xBE2)
#define CSR_FPRINT_LEN                          (0xBE3)

// -- LUT
#define CSR_VMU_WR_LUT_FIRST       CSR_WR_LUT_FIRST
#define CSR_VMU_WR_LUT_INC         CSR_WR_LUT_INC

#ifdef __cplusplus
}
#endif

#endif /*__HIPU200CLIB_CSR_REG_DEF_H__*/
