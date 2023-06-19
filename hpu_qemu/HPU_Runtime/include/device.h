//
// Created by jiashu on 10/31/19.
//

#ifndef LIBVFIO_DEVICE_H
#define LIBVFIO_DEVICE_H

#include <cstdint>
#include <vector>

struct device_private;

struct dma_addr {
    uint32_t index;
    uint64_t vaddr;            /* Process virtual address */
    uint64_t iova;                /* IO virtual address */
    size_t size;                /* Size of mapping (bytes) */
};

struct pcie_head {
    uint16_t vendor_id;
    uint16_t device_id;
    uint16_t command;
    uint16_t status;
    uint32_t head_0x08;
    uint32_t head_0x0C;
    uint32_t bar0_addr;
    uint32_t bar1_addr;
    uint32_t bar2_addr;
    uint32_t bar3_addr;
};

class device {
private:
    device_private *aPrivate;

public:
    device(int iommu_group, const char *bus_name);

    ~device();

    std::vector<dma_addr> pool;

    pcie_head head;

    void reg_write(uint32_t reg_32bit, uint32_t value_32bit);

    uint32_t reg_read(uint32_t reg);

    int wait_irq();
};

#endif //LIBVFIO_DEVICE_H
