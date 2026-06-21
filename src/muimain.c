
#include <stdio.h>
#include <pthread.h>
//#include "arm.h"
//#include <time.h>
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
#include <proto/layers.h>
#include <proto/alib.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/icon.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include <proto/gadtools.h>
#include <proto/utility.h>
#include <proto/asl.h>
#include <proto/muimaster.h>
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

#define REG(x)

#ifndef DISPATCHER
#define DISPATCHER(Name) \
static ULONG Name##_Dispatcher(void); \
struct EmulLibEntry GATE ##Name##_Dispatcher = { TRAP_LIB, 0, (void (*)(void)) Name##_Dispatcher }; \
static ULONG Name##_Dispatcher(void) { struct IClass *cl=(struct IClass*)REG_A0; Msg msg=(Msg)REG_A1; Object *obj=(Object*)REG_A2;
#define DISPATCHER_REF(Name) &GATE##Name##_Dispatcher
#define DISPATCHER_END }
#endif

#if defined __MAXON__ || defined __GNUC__
	#define ASM
	#define SAVEDS
	#else
	#define ASM    __asm
	#define SAVEDS __saveds
#endif 

struct BitMap *bm;
static pthread_t time_thread;
static pthread_cond_t video_cond = PTHREAD_COND_INITIALIZER;
static pthread_cond_t video_cond2 = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t video_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t video_mutex2 = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t timer_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_t video_thread,video_thread2,video_thread3;
struct timerequest  *bd_TimerRequest;
	int mx, my;
void delete_timer(struct timerequest *);
struct timerequest *create_timer(ULONG);

#ifdef DEVICES_TIMER_H_TIMEVAL_CAMELCASE
 struct TimeVal time_delay(struct TimeVal *, LONG);
 void wait_for_timer(struct timerequest *, struct TimeVal *);
#else
 struct timeval time_delay(struct timeval *, LONG);
 void wait_for_timer(struct timerequest *, struct timeval *);
#endif

static uint64_t delaytime=0;
static uint64_t video_timer_next=0;
static uint64_t videodelay=0;
static void* 
vidcthreadrunner3(void *threadid);
static void *
vidcthreadrunner2(void *threadid);
extern struct Library     *TimerBase;
struct Library	*ExecBase;
struct Library *LayersBase = NULL;
int drawscrc = 0;
clock_t timerclock;
int running1,running2;
int stop1,stop2;
struct timerequest *tr;
BOOL working;
uint64_t iomdtimer=2000;
struct MsgPort *winport;
volatile ULONG videonext;

typedef enum {
	DONT_KNOW = -1,
	INSIDE_WINDOW,
	OUTSIDE_WINDOW
} POINTER_STATE;

struct Data {
	int x;
	int y;
};

	APTR app;
// Some code borrowed from E-UAE (Develin) =)

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

void Cleanup_Libs()
{
	if (LayersBase)
	{
		CloseLibrary (LayersBase);
		LayersBase = NULL;
	}
}

BOOL Init_Libs()
{
   LayersBase = OpenLibrary ("layers.library", 0L);
	if (!LayersBase)
	{
		printf ("No layers.library\n");
		return 0;
	}
	else
	{
      return 1;
	}
}


/***************************************************************************/
/* Here is the beginning of our new class...                               */
/***************************************************************************/

/*
** This is the instance data for our custom class.
*/


/*
** AskMinMax method will be called before the window is opened
** and before layout takes place. We need to tell MUI the
** minimum, maximum and default size of our object.
*/

SAVEDS ULONG mAskMinMax(struct IClass *cl,Object *obj,struct MUIP_AskMinMax *msg)
{
	/*
	** let our superclass first fill in what it thinks about sizes.
	** this will e.g. add the size of frame and inner spacing.
	*/

	DoSuperMethodA(cl,obj,msg);

	/*
	** now add the values specific to our object. note that we
	** indeed need to *add* these values, not just set them!
	*/

	msg->MinMaxInfo->MinWidth  += 100;
	msg->MinMaxInfo->DefWidth  += 800;
	msg->MinMaxInfo->MaxWidth  += 1920;

	msg->MinMaxInfo->MinHeight += 40;
	msg->MinMaxInfo->DefHeight += 600;
	msg->MinMaxInfo->MaxHeight += 1080;

	return(0);
}


/*
** Draw method is called whenever MUI feels we should render
** our object. This usually happens after layout is finished
** or when we need to refresh in a simplerefresh window.
** Note: You may only render within the rectangle
**       _mleft(obj), _mtop(obj), _mwidth(obj), _mheight(obj).
*/

SAVEDS ULONG mDraw(struct IClass *cl,Object *obj,struct MUIP_Draw *msg)
{
	struct Data *data = INST_DATA(cl,obj);

	/*
	** let our superclass draw itself first, area class would
	** e.g. draw the frame and clear the whole region. What
	** it does exactly depends on msg->flags.
	**
	** Note: You *must* call the super method prior to do
	** anything else, otherwise msg->flags will not be set
	** properly !
	*/

	DoSuperMethodA(cl,obj,msg);

	/*
	** if MADF_DRAWOBJECT isn't set, we shouldn't draw anything.
	** MUI just wanted to update the frame or something like that.
	*/

	return(0);
}


SAVEDS ULONG mSetup(struct IClass *cl,Object *obj,Msg msg)
{
	if (!(DoSuperMethodA(cl,obj,msg)))
		return(FALSE);

	MUI_RequestIDCMP(obj,IDCMP_MOUSEBUTTONS|IDCMP_RAWKEY|IDCMP_MOUSEMOVE);

	return(TRUE);
}


SAVEDS ULONG mCleanup(struct IClass *cl,Object *obj,Msg msg)
{
	MUI_RejectIDCMP(obj,IDCMP_MOUSEBUTTONS|IDCMP_RAWKEY|IDCMP_MOUSEMOVE);
	return(DoSuperMethodA(cl,obj,msg));
}


SAVEDS ULONG mHandleInput(struct IClass *cl,Object *obj,struct MUIP_HandleInput *msg)
{
	#define _between(a,x,b) ((x)>=(a) && (x)<=(b))
	#define _isinobject(x,y) (_between(_mleft(obj),(x),_mright(obj)) && _between(_mtop(obj),(y),_mbottom(obj)))
	struct IntuiMessage * imsg;
	struct Data *data = INST_DATA(cl,obj);

	if (msg->imsg)
	{
	imsg=msg->imsg;
		running1=1;
//			eventdone=TRUE;
			mx = imsg->IDCMPWindow->MouseX;
			my = imsg->IDCMPWindow->MouseY;
		switch (msg->imsg->Class)
		{
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
/*
				   POINTER_STATE new_state = get_pointer_state (win, mx, my);
					if (new_state != pointer_state)
					{
					   pointer_state = new_state;
						if (pointer_state == INSIDE_WINDOW)
						   hide_pointer (win);
						else
                     show_pointer (win);
                }
*/
                mouse_mouse_move(imsg->MouseX-_mleft(MyObj)/*win->BorderLeft*/, imsg->MouseY-_mtop(MyObj)/*win->BorderTop*/);
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
	return(DoSuperMethodA(cl,obj,msg));
}


/*
** Here comes the dispatcher for our custom class. 
** Unknown/unused methods are passed to the superclass immediately.
*/

DISPATCHER(MyClass)
{
	switch (msg->MethodID)
	{
		case MUIM_AskMinMax  : return(mAskMinMax  (cl,obj,(APTR)msg));
		case MUIM_Draw       : return(mDraw       (cl,obj,(APTR)msg));
		case MUIM_HandleInput: return(mHandleInput(cl,obj,(APTR)msg));
		case MUIM_Setup      : return(mSetup      (cl,obj,(APTR)msg));
		case MUIM_Cleanup    : return(mCleanup    (cl,obj,(APTR)msg));
	}

	return(DoSuperMethodA(cl,obj,msg));
}
DISPATCHER_END


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
		/*	struct timespec tm;

			tm.tv_sec = 0;
			tm.tv_nsec = 1000000;
			nanosleep(&tm, NULL);
*/
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


int main()
{
	clock_t start;
    clock_t endclock;
		long timercount;
#ifdef DEVICES_TIMER_H_TIMEVAL_CAMELCASE
	 struct TimeVal currentval,currentval2;
#else
	 struct timeval currentval,currentval2;
#endif

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
	APTR  button;
	
	//APTR app,window;
	struct MUI_CustomClass *mcc;

	//init();

	/* Create the new custom class with a call to MUI_CreateCustomClass(). */
	/* Caution: This function returns not a struct IClass, but a           */
	/* struct MUI_CustomClass which contains a struct IClass to be         */
	/* used with NewObject() calls.                                        */
	/* Note well: MUI creates the dispatcher hook for you, you may         */
	/* *not* use its h_Data field! If you need custom data, use the        */
	/* cl_UserData of the IClass structure!                                */

	if (!(mcc = MUI_CreateCustomClass(NULL,MUIC_Area,NULL,sizeof(struct Data),DISPATCHER_REF(MyClass))))
		printf("Could not create custom class.\n");

	app = ApplicationObject,
		MUIA_Application_Title      , "RPCEmu",
		//MUIA_Application_Version    , "$VER: Class3 20.164 (04.04.03)",
		//MUIA_Application_Copyright  , "© 1993 Stefan Stuntz",
		//MUIA_Application_Author     , "Stefan Stuntz",
		//MUIA_Application_Description, "Demonstrate the use of custom classes.",
		MUIA_Application_Base       , "RPCEMU",

		SubWindow, window = WindowObject,
			MUIA_Window_Title, "RPCEmu for MorphOS",
			MUIA_Window_ID   , MAKE_ID('R','P','C','E'),
			
			WindowContents, VGroup,

				
				Child,VSpace(2),
				Child, HGroup,
				MUIA_Weight,0,
				Child, button=SimpleButton("\33cReset your RiscPC ! "),
				End,
				Child, MyObj = NewObject(mcc->mcc_Class,NULL,
					TextFrame,
					TAG_DONE),

				End,
				/*Child, VSpace(0),
				Child, HGroup,*/
				
		

			End,
		End;


	set(button,MUIA_Text_SetVMax,FALSE);
	//set(window,MUIA_Window_ActiveObject,button);
	
	set(window,MUIA_Window_DefaultObject, MyObj);
	DoMethod(window,MUIM_Notify,MUIA_Window_CloseRequest,TRUE,
		app,2,MUIM_Application_ReturnID,MUIV_Application_ReturnID_Quit);
	set(window,MUIA_Window_Open,TRUE);

   if (Init_Libs())
	{
    printf("hello\n");
    winw=640;
    winh=480;
	int oldcputime=0;
	get(window,MUIA_Window_Window,win);
    /*win = OpenWindowTags(NULL,
						 WA_InnerWidth, winw, WA_InnerHeight, winh, WA_AutoAdjust, TRUE, WA_Title, "RPCEmu for MorphOS", WA_CloseGadget,TRUE,
                         WA_DepthGadget,TRUE,WA_DragBar,TRUE,WA_IDCMP,IDCMP_CLOSEWINDOW|IDCMP_REFRESHWINDOW|IDCMP_RAWKEY|IDCMP_MOUSEMOVE|IDCMP_MOUSEBUTTONS,
                         WA_SimpleRefresh,FALSE,WA_Activate,TRUE,WA_Flags, WFLG_REPORTMOUSE,TAG_DONE);

	*/
    winport=win->UserPort;
	char * vbuf=(char*)malloc(winw*winh*4);
	memset(vbuf,1,winw*winh*4);
	currentval2=time_delay(&currentval,0);
		t1=currentval2.tv_micro;
			
	WritePixelArray(vbuf, 0, 0, winw*4, _rp(MyObj)/*win->RPort*/, win->BorderLeft, win->BorderTop, winw, winh, RECTFMT_ARGB);
	
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
	
	//struct timespec start3, end3;
	
    globaltime=0x0;
	
	while(working) 
	{
		
        struct IntuiMessage *imsg;
	{
		ULONG sigs = 0;

		while (DoMethod(app,MUIM_Application_NewInput,&sigs) != MUIV_Application_ReturnID_Quit)
		{
			if (sigs)
			{
				sigs = Wait(sigs | SIGBREAKF_CTRL_C);
				if (sigs & SIGBREAKF_CTRL_C) break;
			}
		}
	working=FALSE;
	}	

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
//		set(window,MUIA_Window_Open,FALSE);
	
                
		 iomd_end();
        	//fdc_image_save(discname[0], 0);
        	//fdc_image_save(discname[1], 1);
        	free(vram);
        	free(ram00);
        	free(ram01);
        	free(rom);
        	//savecmos();
        	//config_save(&config);
			//free_pointer ();
    		
		//CloseWindow(win);
	printf("after free\n");
	set(window,MUIA_Window_Open,FALSE);
	printf("after win close\n");
	MUI_DisposeObject(app);     /* dispose all objects. */
	printf("after dispose \n");
	MUI_DeleteCustomClass(mcc); /* delete the custom class. */
	printf("after custclas\n");
	   closevideo();
	
		Cleanup_Libs();
	}
		printf("before return\n");
		return(0);
    

}

static void *
vidcthreadrunner3(void *threadid)
{

	struct timespec tv2,start8, end8,start4,end4;
	//uint64_t videodelay=0;
	//uint64_t iomdtimer=2000;
#ifdef DEVICES_TIMER_H_TIMEVAL_CAMELCASE
	struct TimeVal currentval,currentval2,currentval3;
#else
	struct timeval currentval,currentval2,currentval3;
#endif
	iomdtimer=2000;
//	tv2.tv_nsec=400000;
	//tv2.tv_sec=0;
	delaytime=0;
    while (working && running1!=0)
    {
		int exec_count=0;
		
		if (!running1)
		{
			printf("running10\n");
			return NULL;
		}
		
		GetSysTime(&currentval2);
		//for ( exec_count=0;exec_count<=20000;exec_count+=200)
		//{
		//GetSysTime(&currentval2);
			execarm(800);
			
			//drawscr(drawscre);
#if 1
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
	//}

	if (drawscre > 0) {
		drawscr(1);
		drawscre--;
		if (drawscre > 5) {
			drawscre = 0;
		}
}


		
		//}	
	//drawscr(1);		
		
		if (!running1)
		{
			printf("running1a\n");
			return NULL;
		}
		GetSysTime(&currentval);
		
		if ((currentval.tv_micro - currentval2.tv_micro)<18446744073UL)
		delaytime+=(currentval.tv_micro - currentval2.tv_micro);
		//drawscre++;
		if (delaytime>=iomdtimer)
		{
			//printf("delaytime: %d\n",delaytime/2000);
			gentimerirq();
			iomdtimer+=2000;
			//drawscre++;
			
		}
		GetSysTime(&currentval3);
		//printf("profile cpu: %d\n",currentval3.tv_micro - currentval2.tv_micro);
		videodelay += (currentval3.tv_micro - currentval2.tv_micro);
		if (videodelay >= videonext)
		{
			drawscre++;
			videonext+= 1660;


		}
	
		if (!running1)
		{
			printf("running1\n");
			return NULL;
		}
//}		
	
	

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
#ifdef DEVICES_TIMER_H_TIMEVAL_CAMELCASE
        struct TimeVal currentval;
#else
		  struct timeval currentval;
#endif
        currentval.tv_secs = 0;
        currentval.tv_micro = 200000/50;
      
           if (pthread_cond_wait(&video_cond, &video_mutex))
		   {      // && !pthread_cond_wait(&video_cond, &video_mutex)) {
              
				perror("wait");			  
			   printf("pthread_cond_wait failed\n");
           }
        
        //if (!quited) {
       
        vidcthread();
        gentimerirq();


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

#ifdef DEVICES_TIMER_H_TIMEVAL_CAMELCASE
struct TimeVal time_delay(struct TimeVal *tv, LONG unit)
#else
struct timeval time_delay(struct timeval *tv, LONG unit)
#endif
{
	struct timerequest tr2;
#ifdef DEVICES_TIMER_H_TIMEVAL_CAMELCASE
	struct TimeVal tv2;
#else
	struct timeval tv2;
#endif
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

#ifdef DEVICES_TIMER_H_TIMEVAL_CAMELCASE
void wait_for_timer(struct timerequest *tr, struct TimeVal *tv)
#else
void wait_for_timer(struct timerequest *tr, struct timeval *tv)
#endif
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
