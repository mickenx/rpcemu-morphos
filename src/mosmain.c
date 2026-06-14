#include <stdio.h>
#include <pthread.h>
//#include "arm.h"
#include <time.h>
//#include "rpcemu.h"
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
#include <math.h>
#include "cmos.h"
#include "superio.h"
#include "i8042.h"
#include "romload.h"
#include "cp15.h"
#include "cdrom-iso.h"
#include "podulerom.h"
#include "podules.h"
#include "fdc.h"
#include "hostfs.h"
#include <clib/macros.h>
#include <intuition/intuition.h>
#include <intuition/extensions.h>
#include <intuition/monitorclass.h>
#include <intuition/pointerclass.h>
#include <cybergraphx/cybergraphics.h>
#include <exec/types.h>
#include <exec/memory.h>
#include <devices/timer.h>
#include <proto/dos.h>
#include <proto/exec.h>

#include <exec/io.h>

#include <clib/debug_protos.h>

#include <clib/exec_protos.h>
#include <clib/alib_protos.h>
#include <clib/dos_protos.h>

#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/cybergraphics.h>
#include <proto/timer.h>

#include <stdio.h>
#include <time.h>
#include <sys/time.h>
#include <stdlib.h>
#include <unistd.h>

#include "arm.h"


struct BitMap *bm;
static pthread_t time_thread;
static pthread_cond_t video_cond = PTHREAD_COND_INITIALIZER;
static pthread_cond_t video_cond2 = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t video_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t video_mutex2 = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t timer_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_t video_thread,video_thread2,video_thread3;
struct timerequest  *bd_TimerRequest;

void delete_timer(struct timerequest *);
struct timeval time_delay(struct timeval *, LONG);
struct timerequest *create_timer(ULONG);
void wait_for_timer(struct timerequest *, struct timeval *);
static uint64_t delaytime=0;
static uint64_t video_timer_next=0;
static uint64_t videodelay=0;
static void* 
vidcthreadrunner3(void *threadid);
static void *
vidcthreadrunner2(void *threadid);
extern struct Library     *TimerBase;
struct Library	*ExecBase;
int drawscrc = 0;
clock_t timerclock;
int running1,running2;
int stop1,stop2;
struct timerequest *tr;
BOOL working;
uint64_t iomdtimer=2000;
struct MsgPort *winport;
volatile ULONG videonext;
    /* get a pointer to an initialized timer request block */
    
void rpcemu_idle_process_events()
{
	const int32_t iomd_timer_interval = 2000000; // 2000000 ns = 2 ms (500 Hz)

	// Handle qt events and messages
	//QCoreApplication::processEvents();

	//const qint64 elapsed = elapsed_timer.nsecsElapsed();

	// If we have passed the time the IOMD timer event should occur, trigger it
	if (delaytime >= iomdtimer) {
		//iomd_timer_count.fetchAndAddRelease(1);
		gentimerirq();
		delaytime += (uint64_t) 2000; //iomd_timer_interval;
	}

	// If we have passed the time the Video timer event should occur, trigger it
	if (videodelay >= video_timer_next) {
		//video_timer_count.fetchAndAddRelease(1);
		//vblupdate();
		videodelay += (uint64_t) 1000/60;
	}
}
#if 1
void
rpcemu_idle(void)
{
	int hostupdate = 0;

	/* Loop while no interrupts pending */
	while (!armirq) {
		/* Run down any callback timers */
		if (kcallback) {
			kcallback--;
			if (kcallback <= 0) {
				kcallback = 0;
				keyboard_callback_rpcemu();
			}
		}
		if (mcallback) {
			mcallback -= 10;
			if (mcallback <= 0) {
				mcallback = 0;
				mouse_ps2_callback();
			}
		}
		if (fdccallback) {
			fdccallback -= 10;
			if (fdccallback <= 0) {
				fdccallback = 0;
				fdc_callback();
			}
		}
		if (idecallback) {
			idecallback -= 10;
			if (idecallback <= 0) {
				idecallback = 0;
				callbackide();
			}
		}
		if (motoron) {
			/* Not much point putting a counter here */
			iomd.irqa.status |= IOMD_IRQA_FLOPPY_INDEX;
			updateirqs();
		}
		/* Sleep if no interrupts pending */
		if (!armirq) {
#ifdef RPCEMU_WIN
			Sleep(1);
#else
			struct timespec tm;

			tm.tv_sec = 0;
			tm.tv_nsec = 1000000;
			nanosleep(&tm, NULL);
#endif
		}
		/* Run other periodic actions */
		if (!armirq && !(++hostupdate > 20)) {
			hostupdate = 0;
			drawscr(drawscre);
			if (drawscre > 0) {
				drawscre--;
				if (drawscre > 5)
					drawscre = 0;
			}
			rpcemu_idle_process_events();
		}
	}
}
#endif

// Some code borrowed from E-UAE =)

static APTR blank_pointer;

/*
 * Initializes a pointer object containing a blank pointer image.
 * Used for hiding the mouse pointer
 */
static void init_pointer (void)
{
	static struct BitMap bitmap;
	static UWORD	 row[2] = {0, 0};

	InitBitMap (&bitmap, 2, 16, 1);
	bitmap.Planes[0] = (PLANEPTR) &row[0];
	bitmap.Planes[1] = (PLANEPTR) &row[1];

	blank_pointer = NewObject (NULL, POINTERCLASS,
										POINTERA_BitMap,	(ULONG)&bitmap,
										POINTERA_WordWidth,	1,
									   TAG_DONE);

	if (!blank_pointer)
		printf ("Warning: Unable to allocate blank mouse pointer.\n");
}

/*
 * Free up blank pointer object
 */
static void free_pointer (void)
{
	if (blank_pointer) {
		DisposeObject (blank_pointer);
		blank_pointer = NULL;
	}
}

/*
 * Hide mouse pointer for window
 */
static void hide_pointer (struct Window *w)
{
	SetWindowPointer (w, WA_Pointer, (ULONG)blank_pointer, TAG_DONE);
}

/*
 * Restore default mouse pointer for window
 */
static void show_pointer (struct Window *w)
{
	SetWindowPointer (w, WA_Pointer, 0, TAG_DONE);
}

typedef enum {
	DONT_KNOW = -1,
	INSIDE_WINDOW,
	OUTSIDE_WINDOW
} POINTER_STATE;

static POINTER_STATE pointer_state;

static POINTER_STATE get_pointer_state (const struct Window *w, int mousex, int mousey)
{
	POINTER_STATE new_state = OUTSIDE_WINDOW;

	/*
	 * Is pointer within the bounds of the inner window?
	 */
	if ((mousex >= w->BorderLeft)
		&& (mousey >= w->BorderTop)
		&& (mousex < (w->Width - w->BorderRight))
		&& (mousey < (w->Height - w->BorderBottom))) {
		/*
		 * Yes. Now check whetehr the window is obscured by
		 * another window at the pointer position
		 */
		struct Screen *scr = w->WScreen;
	struct Layer  *layer;

	/* Find which layer the pointer is in */
	LockLayerInfo (&scr->LayerInfo);
	layer = WhichLayer (&scr->LayerInfo, scr->MouseX, scr->MouseY);
	UnlockLayerInfo (&scr->LayerInfo);

	/* Is this layer our window's layer? */
	if (layer == w->WLayer) {
		/*
		 * Yes. Therefore, pointer is inside the window.
		 */
		new_state = INSIDE_WINDOW;
	}
		}
		return new_state;
}

int main()
{
	clock_t start;
    clock_t endclock;
		long timercount;
	 struct timeval currentval,currentval2;
	ULONG extracpu=0;
    working=TRUE;
	struct Task * task1;
	struct Task * task2;
	struct timerequest tr3;
	//tr=&tr3;
	tr=NULL;
	ULONG cycles=0;
	stop1=0;
	stop2=0;
	int drawscrc=6;
	int videodelay=0;
	uint64_t t1=0,t2=0,t5=0,t6=0,normalcpu=0;
	volatile uint64_t iomdnext=(uint64_t)2000000;
	volatile uint64_t globaltime=(uint64_t)0;
	BOOL eventdone=FALSE;
	int mx, my;
	
	
    printf("hello\n");
    winw=640;
    winh=480;
	int oldcputime=0;
	
    win = OpenWindowTags(NULL,
						 WA_InnerWidth, winw, WA_InnerHeight, winh, WA_AutoAdjust, TRUE, WA_Title, "RPCEmu for MorphOS", WA_CloseGadget,TRUE,
                         WA_DepthGadget,TRUE,WA_DragBar,TRUE,WA_IDCMP,IDCMP_CLOSEWINDOW|IDCMP_REFRESHWINDOW|IDCMP_RAWKEY|IDCMP_MOUSEMOVE|IDCMP_MOUSEBUTTONS,
                         WA_SimpleRefresh,FALSE,WA_Activate,TRUE,WA_Flags, WFLG_REPORTMOUSE,TAG_DONE);

	
    winport=win->UserPort;
	char * vbuf=(char*)malloc(winw*winh*4);
	memset(vbuf,1,winw*winh*4);
	currentval2=time_delay(&currentval,0);
		t1=currentval2.tv_micro;
			
	WritePixelArray(vbuf, 0, 0, winw*4, win->RPort, win->BorderLeft, win->BorderTop, winw, winh, RECTFMT_ARGB);
	
		currentval2=time_delay(&currentval,0);
		t2=currentval2.tv_micro;
	videodelay=t2-t1;	
	float ggg =(800.0*600.0)/(640.0*480.0);

	
	running1=1;
	
    init_pointer ();
    fdc_init();
    initvideo();

    

    cp15_init();
    
    mem_init();
    
    loadroms();
    
    arm_init();
    cmos_init();
 
    resetarm(CPUModel_SA110); 

    keyboard_reset();
    
    initcodeblocks();
    initpodulerom();
   
    mem_reset(256,8);
    
    iomd_reset(IOMDType_IOMD);
   
    cmos_reset();
    cp15_reset(CPUModel_SA110);//SA110);//machine.cpu_model);
   
    reseti2c(I2C_PCF8583);
    resetide();
    
    superio_reset(SuperIOType_FDC37C665GT);
    
    i8042_reset();
   
    cmos_reset();
    
    podules_reset();
    
    podulerom_reset();
    
    hostfs_reset();
	
// must be called after podules_reset()


	    
	if (pthread_create(&video_thread3, NULL, vidcthreadrunner3, NULL))
    {
        printf("Couldn't create vidc thread");
    }
	

	
	
    dumpregs();
	currentval2=time_delay(&currentval,0);
		//start = (clock()*1000);// / CLOCKS_PER_SEC) / 1000;
		//DateStamp(&dstamp);
		t1=currentval2.tv_micro;	
		execarm(20000);
		drawscr(1);
		running2=1;
		currentval2=time_delay(&currentval,0);
		//start = (clock()*1000);// / CLOCKS_PER_SEC) / 1000;
		//DateStamp(&dstamp);
		t2=currentval2.tv_micro;
	normalcpu=t2-t1;
	//iomdnext=0;	
	
	struct timespec start3, end3;
	
    globaltime=0x0;
	
	while(working) 
	{
		
        struct IntuiMessage *imsg;
		
#if 1		
        WaitPort(win->UserPort);
	
        while ((imsg = (struct IntuiMessage *)GetMsg(win->UserPort)) && working == TRUE)
        {
		//printf("imsg class 0x%x\n",imsg->Class);
			running1=1;
			eventdone=TRUE;
			mx = imsg->IDCMPWindow->MouseX;
			my = imsg->IDCMPWindow->MouseY;

            switch (imsg->Class)
            {
            case IDCMP_CLOSEWINDOW:
				running1=0;
				running2=0;
			
				
                if (pthread_cond_signal(&video_cond))
                {
                    printf("Couldn't signal vidc thread\n");
                }
		//		pthread_join(video_thread3,NULL);
                pthread_cancel(video_thread2);
				pthread_cancel(video_thread3);
				pthread_cancel(video_thread);
				
                working = FALSE;
				running1=0;

                break;
            case IDCMP_RAWKEY:
            {
                printf("key: 0x%x\n",imsg->Code);
                if (imsg->Code & 0x80)
                {
                    int ttt = imsg->Code&~(0x80);
                    keyboard_key_release(keyboard_map_key(ttt));
                }
                else
                {
                    keyboard_key_press(keyboard_map_key(imsg->Code));
                }
                
                break;
            }
            case IDCMP_MOUSEMOVE:
            {
              //  printf("mousE\n");
				   POINTER_STATE new_state = get_pointer_state (win, mx, my);
					if (new_state != pointer_state)
					{
					   pointer_state = new_state;
						if (pointer_state == INSIDE_WINDOW)
						   hide_pointer (win);
						else
                     show_pointer (win);
                }
                mouse_mouse_move(imsg->MouseX-win->BorderLeft, imsg->MouseY-win->BorderTop);
                //printf("x: %d y: %d\n",imsg->MouseX,imsg->MouseY);

                break;
            }
            case IDCMP_MOUSEBUTTONS:
                switch (imsg->Code)
                {
                case SELECTDOWN:
                    mouse_mouse_press(1);
                    break;
                case SELECTUP:
                    mouse_mouse_release(1);
                    break;
				case MIDDLEDOWN:
                    mouse_mouse_press(4);
                    break;
                case MIDDLEUP:
                    mouse_mouse_release(4);
                    break;
                default:
                    break;
                }

            default:
                break;
            }

            ReplyMsg((struct Message *)imsg);

            imsg=NULL;
            
        }
		
		
		
#endif
		if (idecallback) {
			idecallback -= 10;
			if (idecallback <= 0) {
				idecallback = 0;
				callbackide();
			}
		}
		t2=currentval2.tv_micro;
		
        if (drawscre>0)
        {

                drawscre--;
                if (drawscre>5) drawscre=0;
        }
		
		extracpu=t2-t1;
		
		
		globaltime+=normalcpu*10;//(uint64_t)(end3.tv_nsec-start3.tv_nsec); //(end1-t1);
		



    }
		 iomd_end();
        	//fdc_image_save(discname[0], 0);
        	//fdc_image_save(discname[1], 1);
        	free(vram);
        	free(ram00);
        	free(ram01);
        	//free(rom);
        	savecmos();
        	//config_save(&config);
			free_pointer ();
    
		CloseWindow(win);
	closevideo();
		exit(0);
    

}

static void *
vidcthreadrunner3(void *threadid)
{

	struct timespec tv2,start8, end8,start4,end4;
	//uint64_t videodelay=0;
	//uint64_t iomdtimer=2000;
	struct timeval currentval,currentval2,currentval3;
	tv2.tv_nsec=400000;
	tv2.tv_sec=0;
    while (working && running1!=0)
    {
		int exec_count=0;
		
		if (!running1)
		{
			printf("running10\n");
			return NULL;
		}
		
		GetSysTime(&currentval2);
		//for ( exec_count=0;exec_count<1;exec_count++)
		//{
			execarm(800);
			//drawscr(1);
#if 0
			if (kcallback) {
			kcallback--;
			if (kcallback <= 0) {
				kcallback = 0;
				keyboard_callback_rpcemu();
			}
		}

		if (mcallback) {
			mcallback -= 10;
			if (mcallback <= 0) {
				mcallback = 0;
				mouse_ps2_callback();
			}
		}
		if (fdccallback) {
			fdccallback -= 100;
			if (fdccallback <= 0) {
				fdccallback = 0;
				fdc_callback();
			}
		}
		if (idecallback) {
			idecallback -= 10;
			if (idecallback <= 0) {
				idecallback = 0;
				callbackide();
			}
		}
		if (motoron) {
			//disc_poll();
		}
	
#endif

	if (drawscre > 0) {
		drawscr(1);
		drawscre--;
		if (drawscre > 5) {
			drawscre = 0;
		}
//}


		
		}	
	//drawscr(1);		
		
		if (!running1)
		{
			printf("running1a\n");
			return NULL;
		}
		GetSysTime(&currentval);
		if ((currentval.tv_micro - currentval2.tv_micro)<18446744073UL)
		delaytime+=(currentval.tv_micro - currentval2.tv_micro);
		if (delaytime>=iomdtimer)
		{
			gentimerirq();
			iomdtimer+=2000;
			
			
		}
		GetSysTime(&currentval3);
		videodelay += (currentval3.tv_micro - currentval2.tv_micro);
		if (videodelay/100 >= videonext)
		{
			drawscre++;
			videonext+= 1000000000/60;


		}
	
		if (!running1)
		{
			printf("running1\n");
			return NULL;
		}
		
	
	

	}
	printf("exit thread\n");
	return NULL;
}



int
vidctrymutex(void)
{
    return 1;
    int ret = pthread_mutex_trylock(&video_mutex2);
    if (ret == 16)
    {
        printf("EBUSY\n");
        return 0;
    }
    if (ret)
    {
        printf("Getting vidc mutex failed");
		return 0 ;
    }
    return 1;
}

void
vidcreleasemutex(void)
{
    // printf("+++++++++++++++++++Releasing \n");
    if(pthread_mutex_unlock(&video_mutex2))
    {
        printf("+++++++++++++++++++Releasing vidc mutex failed++++++++++++++++\n");
    }
}

void
vidcwakeupthread(void)
{
    //printf("launch vidcthread1\n");
    if (pthread_cond_signal(&video_cond))
    {
        printf("Couldn't signal vidc thread\n");
    }
}

static void *
vidcthreadrunner2(void *threadid)
{


    if (pthread_mutex_lock(&video_mutex))
    {
        printf("Cannot lock mutex\n");
		return NULL;
		//volatile ddd=0;
    }
    while (working)
		
    {
		
        struct timeval currentval;
        currentval.tv_secs = 0;
        currentval.tv_micro = 200000/50;
      
           if (pthread_cond_wait(&video_cond, &video_mutex))
		   {      // && !pthread_cond_wait(&video_cond, &video_mutex)) {
              
				perror("wait");			  
			   printf("pthread_cond_wait failed\n");
           }
        
        //if (!quited) {
       
        vidcthread();
        


    }
    pthread_mutex_unlock(&video_mutex);
    return NULL;





}

void
vidcstartthread(void)
{
    printf("START THREAD\n");
    if (pthread_create(&video_thread2, NULL, vidcthreadrunner2, NULL))
    {
      //  printf("Couldn't create vidc thread");
    }
}

struct timerequest *create_timer(ULONG unit)
{
    /* return a pointer to a timer request.  If any problem, return NULL */
    LONG error2;
    struct MsgPort *timerport;
    struct timerequest *TimerIO;

    timerport = CreatePort(0, 0);
    if (timerport == NULL) {
		printf("timerport == NULL\n");
        return (NULL);
	}

    TimerIO = (struct timerequest *)
              CreateExtIO(timerport, sizeof(struct timerequest));
    if (TimerIO == NULL)
    {
        DeletePort(timerport); /* Delete message port */
		printf("no timerio\n");
        return (NULL);
    }

    error2 = OpenDevice(TIMERNAME, unit, (struct IORequest *)TimerIO, 0L);
    if (error2 != 0)
    {
		printf("opendevice fail\n");
        delete_timer(TimerIO);
        return (NULL);
    }
    return (TimerIO);
}

struct timeval time_delay(struct timeval *tv, LONG unit)
{
    struct timerequest tr2;
	struct timeval tv2;
	//printf("time_delay\n");
    /* any nonzero return says timedelay routine didn't work. */
 /*   if (tr == NULL) {
		printf("TR NULL\n");
		tr=&tr2;
	}*/
      //  return (*tv);
	//tr = create_timer(0);
	GetSysTime(&tv2);
    //wait_for_timer(tr, tv);
	//*tv = tr->tr_time;
    /* deallocate temporary structures */
    //delete_timer(tr);
    return (tv2);
}

void wait_for_timer(struct timerequest *tr, struct timeval *tv)
{

    tr->tr_node.io_Command = TR_GETSYSTIME; /* add a new timer request */

    /* structure assignment */
    //tr->tr_time = *tv;

    /* post request to the timer -- will go to sleep till done */
    DoIO((struct IORequest *)tr);
}

void delete_timer(struct timerequest *tr)
{
    struct MsgPort *tp;

    if (tr != 0)
    {
        tp = tr->tr_node.io_Message.mn_ReplyPort;

        if (tp != 0)
            DeletePort(tp);

        CloseDevice((struct IORequest *)tr);
        DeleteExtIO((struct IORequest *)tr);
    }
}
