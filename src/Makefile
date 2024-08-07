CFLAGS=-noixemul -g -O2  -lpthread
LFLAGS=-noixemul -g -O2  -lpthread -ldebug -lauto
CC=gcc
OBJS=mem.o arm.o cdrom-iso.o cmos.o arm_common.o podulerom.o cp15.o iomd.o podules.o fpa.o rpc-linux.o ide.o keyboard.o keyboard_x.o hostfs.o hostfs-unix.o superio.o fdc.o i8042.o vidc20.o romload.o mosmain.o codegen_null.o icside.o

default: $(OBJS)
	$(CC) -o ../build/rpcemu.4 $(OBJS)  $(LFLAGS)
	