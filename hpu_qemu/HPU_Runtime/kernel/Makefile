CROSS_PREFIX = riscv32-unknown-elf
CC = $(CROSS_PREFIX)-gcc
G++ = $(CROSS_PREFIX)-g++
AS = $(CROSS_PREFIX)-as
LD = $(CROSS_PREFIX)-ld
NM = $(CROSS_PREFIX)-nm
OBJCOPY = $(CROSS_PREFIX)-objcopy
OBJDUMP = $(CROSS_PREFIX)-objdump
STRIP = $(CROSS_PREFIX)-strip

output_name = kernel_all
config = ./link.lds

elf = $(output_name).elf
bin = $(output_name).bin
data = $(output_name).dat
hex = $(output_name).hex
dump = $(output_name).asm

OUT_DIR			= output
RT_INC_PATH		= ../include
INC_PATH		= krnl_inc
KER_PATH		= operators
LIB_PATH		= krnl_lib
QEMU_PATH		= qemu
FMT_PATH		= qemu/libfemto/include
FMT_BIN			= qemu/libfemto/libfemto.a


COREMARK_PARAM=


OPTIMIZE		= -Os


INCLUDES 		= -I$(INC_PATH) -I$(KER_PATH) -I$(LIB_PATH) -I$(RT_INC_PATH) -I$(QEMU_PATH) -I$(FMT_PATH)
CFLAGS			= -march=rv32ima -mabi=ilp32 -mcmodel=medany -ffunction-sections -fdata-sections -g $(INCLUDES) $(PARAM) $(COREMARK_PARAM)
LDFLAGS			= -nostartfiles -static -lgcc -Wl,--nmagic -Wl,--gc-sections

ifdef QEMU
PARAM = -DQEMU_ENV
OTHER_OBJ = $(OUT_DIR)/qemu.co
else
PARAM =
OTHER_OBJ = $(OUT_DIR)/printf.co
endif




OBJ = $(OTHER_OBJ) \
		$(OUT_DIR)/start.o \
		$(OUT_DIR)/hpu_api.o \
		$(OUT_DIR)/dma.co \
		$(OUT_DIR)/lock.co \
		$(OUT_DIR)/libconv.co \
		$(OUT_DIR)/int.co \
		$(OUT_DIR)/krnl_log.co \
		$(OUT_DIR)/hpu_util.co \
		$(OUT_DIR)/kernel_main.co \
		$(OUT_DIR)/conv2d_byoc.co \
		$(OUT_DIR)/fc.co \
		$(OUT_DIR)/conv3s1.co \
		$(OUT_DIR)/vadd.co \


all: dir clean $(bin) 

dir:
	mkdir -p $(OUT_DIR)


$(bin): $(OBJ) 
	$(CC) $(CFLAGS) $(OBJ) $(LDFLAGS) -o $(OUT_DIR)/$(elf) -T $(config) $(FMT_BIN)
	$(OBJDUMP) -DSxtd $(OUT_DIR)/$(elf) > $(OUT_DIR)/$(dump)
	$(OBJCOPY) -O binary $(OUT_DIR)/$(elf) $(OUT_DIR)/$(bin)


$(OUT_DIR)/%.o: krnl_lib/%.s
	$(AS) -c $< -o $@ -march=rv32ima -mabi=ilp32

$(OUT_DIR)/%.co: krnl_lib/%.c
	$(CC) $(CFLAGS) $(OPTIMIZE) -c $< -o $@ 

$(OUT_DIR)/%.co: operators/%.c
	$(CC) $(CFLAGS) $(OPTIMIZE) -c $< -o $@ 

$(OUT_DIR)/%.co: qemu/%.c
	$(CC) $(CFLAGS) $(OPTIMIZE) -c $< -o $@ 

clean:
	rm -rf $(OBJ) $(OUT_DIR)/*

