CFLAGS=-noixemul  -g -O3 -D_RPCEMU_BIG_ENDIAN 
LDFLAGS=-noixemul -lpthread   -debug -lauto
CC=ppc-morphos-gcc-9
OBJS=mem.o arm.o cdrom-iso.o cmos.o arm_common.o podulerom.o cp15.o \
iomd.o podules.o fpa.o rpc-linux.o ide.o keyboard.o keyboard_x.o \
hostfs.o hostfs-unix.o superio.o fdc.o i8042.o vidc20.o romload.o \
muimain.o codegen_null.o icside.o

all: $(OBJS)
	$(CC) -o ../build/rpcemu.4 $(OBJS) $(LDFLAGS)
