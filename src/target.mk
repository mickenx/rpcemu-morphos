TARGET = RISCOS
#QT5_PORT_LIBS += libQt5Core libQt5Gui libQt5Network
SRC_CC = test.cc mem.c ArmDynarec.c  codegen_amd64.c  cdrom-iso.c  cmos.c arm_common.c   podulerom.c  cp15.c iomd.c podules.c fpa.c rpc-linux.c ide.c keyboard.c keyboard_x.c hostfs.c hostfs-unix.c superio.c fdc.c i8042.c vidc20.c romload.c 
 LIBS  += base   cxx libc   stdcxx libpng blit  
 #$(QT5_PORT_LIBS)
#QMAKE_PROJECT_FILE = $(PRG_DIR)/qt5/rpcemu.pro

#MAKE_TARGET_BINARIES = RISCOS

#QT5_PORT_LIBS += libQt5Core libQt5Gui libQt5Network  libQt5Widgets

#QT5_PORT_LIBS += libQt5Qml libQt5Quick

#LIBS = libc libm mesa qt5_component stdcxx $(QT5_PORT_LIBS)

#include $(call select_from_repositories,lib/import/import-qt5_qmake.mk)

