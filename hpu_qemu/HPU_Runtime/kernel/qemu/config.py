class global_var:
    QEMU_PATH = "/new_home/jwhuang/Work/runtime_qemu/qemu-install/bin/qemu-system-riscv32"
    ifm_qemu_addr = "0xc0000000"
    wt_qemu_addr = "0xc0100000"
    bias_qemu_addr = "0xc01f0000"
    shift_qemu_addr = "0xc01fc000"

def get_QEMU_PATH():
    return global_var.QEMU_PATH

def get_ifm_qemu_addr():
    return global_var.ifm_qemu_addr

def get_wt_qemu_addr():
    return global_var.wt_qemu_addr

def get_bias_qemu_addr():
    return global_var.bias_qemu_addr

def get_shift_qemu_addr():
    return global_var.shift_qemu_addr
