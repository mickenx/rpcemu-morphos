//#include <base/component.h>
// #include <base/log.h>
//	extern "C" {
#include <stdio.h>
#include <unistd.h>
#include "arm.h"
#include "rpcemu.h"
#include "romload.h"
#include "mem.h"
#include "cp15.h"
#include "mem.h"
#include "vidc20.h"
#include "keyboard.h"
#include "sound.h"
#include "mem.h"
#include "iomd.h"
#include "ide.h"
#include "arm.h"
#include "cmos.h"
#include "superio.h"
#include "i8042.h"
#include "romload.h"
#include "cp15.h"
#include "cdrom-iso.h"
#include "podulerom.h"
#include "podules.h"
#include "fdc.h"


		Config config = {
        0,                      /* mem_size */
        0,                      /* vram_size */
        NULL,                   /* username */
        NULL,                   /* ipaddress */
        NULL,                   /* macaddress */
        NULL,                   /* bridgename */
        0,                      /* refresh */
        1,                      /* soundenabled */
        1,                      /* cdromenabled */
        0,                      /* cdromtype  -- Only used on Windows build */
        "",                     /* isoname */
        1,                      /* mousehackon */
        0,                      /* mousetwobutton */
        NetworkType_Off,        /* network_type */
        0,                      /* cpu_idle */
        1,                      /* show_fullscreen_message */
        NULL,                   /* network_capture */
};


//	}
#if 0
 void Component::construct(Genode::Env &)
 {
/ Genode::Allocator xx = malloc(1024);
	::printf("hello");
   Genode::log("Hello world");
	dumpregs();
 }
#endif
int main()
{
	printf("Hello World\n");
	 //Genode::Allocator xx = malloc(1024);
	 //init_fd_alloc(xx);
	//cmos_init();
        fdc_init();
        initvideo();

	printf("Hello World\n");

	//cp15_init();
	printf("Hello World2\n");
	mem_init();
	printf("Hello World3\n");
	loadroms();
	printf("Hello World4\n");
	arm_init();
	printf("Hello World\n");
	resetarm(CPUModel_SA110);
	initcodeblocks();
	mem_reset(128,2);
	iomd_reset(IOMDType_IOMD);
	cmos_reset();
	printf("Hello World\n");
	dumpregs();
	execarm(200);
	dumpregs();
	while (1){
	execarm(200);
	dumpregs();
	//sleep(2);
	int i=0;
	for (i=0;i<99999999;i++)
	{}
	}
	return 0;
}

