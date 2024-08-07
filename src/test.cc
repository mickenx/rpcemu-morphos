/*
 * \brief  Nitpicker test program
 * \author Norman Feske
 * \date   2006-08-23
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/env.h>
#include <util/list.h>
#include <base/component.h>
#include <base/log.h>
#include <gui_session/connection.h>
#include <timer_session/connection.h>
#include <input/event.h>
#include <os/pixel_rgb888.h>
#include <libc/component.h>
#include <stdio.h>
#include <unistd.h>
#include <base/component.h>
#include <base/heap.h>
#include <base/attached_rom_dataspace.h>
#include <gui_session/connection.h>
#include <util/misc_math.h>
#include <decorator/xml_utils.h>
#include <nitpicker_gfx/box_painter.h>
#include <nitpicker_gfx/texture_painter.h>
#include <base/attached_dataspace.h>
#include <util/reconstructible.h>
#include <os/texture_rgb888.h>

/* gems includes */
#include <gems/png_image.h>
#include <gems/file.h>
#include <gems/xml_anchor.h>
#include <gems/texture_utils.h>

extern "C" {
#include <pthread.h>
#include "arm.h"
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
#include "hostfs.h"
	Genode::Env *localenv;
	static pthread_t video_thread;
	static pthread_t time_thread;
static pthread_cond_t video_cond = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t video_mutex = PTHREAD_MUTEX_INITIALIZER;
//static pthread_mutex_t timer_mutex = PTHREAD_MUTEX_INITIALIZER;
//static pthread_cond_t timer_cond = PTHREAD_COND_INITIALIZER;

}
void * getram(int len,void *start)
{
	Genode::Dataspace_capability ds = localenv->pd().alloc(len);
         printf("ds len: %d\n",len);
  void *ret =
    localenv->rm().attach(ds,
                    0,         /* map entire dataspace */
                    0,         /* no offset within dataspace */
                    false,     /* ignore local addr */
                    (void*)0, /* local addr */
                    true,      /* executable */
                    true       /* writeable */);
  Genode::log("RAM dataspace locally mapped at ", ret);
//  	memcpy(ret,start,len);
//	start=ret;
	printf("start: %p\n",start);
/*	Genode::Dataspace_capability ds=Genode::Dataspace_capability();
	void * ret =(void*) localenv->rm().attach(ds,
	                          len, 0,
	                          false,
	                          (void *)0,
	                           true,
	                           true);
*/
			   	   return(ret);
        

}
int
vidctrymutex(void)
{
        int ret = pthread_mutex_trylock(&video_mutex);
if (ret == 16) {
		//printf("EBUSY\n");
                return 0;
        }
        if (ret) {
                fatal("Getting vidc mutex failed");
        }
        return 1;
}

void
vidcreleasemutex(void)
{
        if(pthread_mutex_unlock(&video_mutex)) {
                printf("+++++++++++++++++++Releasing vidc mutex failed++++++++++++++++\n");
        }
}
#if 1
static void *
timerunner(void *threadid)
{
        NOT_USED(threadid);
	struct timespec timetest1,timetest;
	//long ntime,oldtime = 0;
	//double timediff = 0;
	//timetest.tv_nsec=1000000;
	Genode::uint64_t timediff = 0;
	Genode::uint64_t ntime,oldtime = 0;
clock_gettime(CLOCK_MONOTONIC,&timetest1);
	ntime = timetest1.tv_nsec;
	timediff=0;	
       // if (pthread_mutex_lock(&timer_mutex)) {
         ///       printf("Cannot lock mutex\n");
        //}
	while (1)

	{
	//timetest.tv_nsec=2000000/4; 1000000;
		/*
clock_gettime(CLOCK_MONOTONIC,&timetest);
	ntime = timetest.tv_nsec;
	clock_gettime(CLOCK_MONOTONIC,&timetest2);
	oldtime = timetest2.tv_nsec;
	timediff+=oldtime-ntime;
	printf("timediff %lf\n",timediff);
			if (timediff>=200000){
				printf("timediff \n");
			gentimerirq();
			timediff=0;
			}
			else
			{
				drawscr(1);
			}
		*/
			execarm(20000);
			drawscr(1);
	clock_gettime(CLOCK_MONOTONIC,&timetest);
	oldtime = timetest.tv_nsec-ntime;
	//timediff+=oldtime-ntime;
			if (oldtime>=timediff){
			timediff=oldtime+2000000;
			//printf("IRQ22222222222222\n");
			gentimerirq();
			//timetest.tv_nsec=2000000/4; //1000000;
			//printf("IRQ1111111111111111\n");
			//usleep(1);
			printf("IRQ!!!!!!!!!!!!!!!\n");
			}
	

	}
 	return NULL;
}
#endif
static void *
vidcthreadrunner(void *threadid)
{
        NOT_USED(threadid);

        if (pthread_mutex_lock(&video_mutex)) {
                printf("Cannot lock mutex\n");
        }
        while (1) {

                if (pthread_cond_wait(&video_cond, &video_mutex)) {
                        printf("pthread_cond_wait failed\n");
                }
		//printf("launch vidcthread2\n");
                //if (!quited) {
                        vidcthread();
                //}
        

	}
        pthread_mutex_unlock(&video_mutex);
        return NULL;
}
void
vidcwakeupthread(void)
{
		//printf("launch vidcthread1\n");
        if (pthread_cond_signal(&video_cond)) {
                printf("Couldn't signal vidc thread\n");
        }
}
void
vidcstartthread(void)
{
        //printf("START THREAD\n");
	if (pthread_create(&video_thread, NULL, vidcthreadrunner, NULL)) {
                fatal("Couldn't create vidc thread");
        }
}

using namespace Genode;

class Test_view : public List<Test_view>::Element
{
	private:

		using View_handle = Gui::Session::View_handle;
		using Command     = Gui::Session::Command;

		Gui::Session_client &_gui;
		View_handle          _handle { };
		int                  _x, _y, _w, _h;
		const char          *_title;
		Test_view           *_parent_view;

	public:

		Test_view(Gui::Session_client *gui,
		          int x, int y, int w, int h,
		          const char *title,
		          Test_view *parent_view = 0)
		:
			_gui(*gui),
			_x(x), _y(y), _w(w), _h(h), _title(title), _parent_view(parent_view)
		{
			using namespace Gui;

			View_handle parent_handle;

			if (_parent_view)
				parent_handle = _gui.view_handle(_parent_view->view_cap());

			_handle = _gui.create_view(parent_handle);

			if (parent_handle.valid())
				_gui.release_view_handle(parent_handle);

			Gui::Rect rect(Gui::Point(_x, _y), Gui::Area(_w, _h));
			_gui.enqueue<Command::Geometry>(_handle, rect);
			_gui.enqueue<Command::To_front>(_handle, View_handle());
			_gui.enqueue<Command::Title>(_handle, _title);
			_gui.execute();
		}

		Gui::View_capability view_cap()
		{
			return _gui.view_capability(_handle);
		}

		void top()
		{
			_gui.enqueue<Command::To_front>(_handle, View_handle());
			_gui.execute();
		}

		/**
		 * Move view to absolute position
		 */
		void move(int x, int y)
		{
			/*
			 * If the view uses a parent view as corrdinate origin, we need to
			 * transform the absolute coordinates to parent-relative
			 * coordinates.
			 */
			_x = _parent_view ? x - _parent_view->x() : x;
			_y = _parent_view ? y - _parent_view->y() : y;

			Gui::Rect rect(Gui::Point(_x, _y), Gui::Area(_w, _h));
			_gui.enqueue<Command::Geometry>(_handle, rect);
			_gui.execute();
		}

		/**
		 * Accessors
		 */
		const char *title() const { return _title; }
		int         x()     const { return _parent_view ? _parent_view->x() + _x : _x; }
		int         y()     const { return _parent_view ? _parent_view->y() + _y : _y; }
		int         w()     const { return _w; }
		int         h()     const { return _h; }
};


class Test_view_stack : public List<Test_view>
{
	private:

		unsigned char *_input_mask;
		unsigned _input_mask_w;
	public:

		Test_view_stack(unsigned char *input_mask, unsigned input_mask_w)
		: _input_mask(input_mask), _input_mask_w(input_mask_w) { }

		Test_view *find(int x, int y)
		{
			for (Test_view *tv = first(); tv; tv = tv->next()) {

				if (x < tv->x() || x >= tv->x() + tv->w()
				 || y < tv->y() || y >= tv->y() + tv->h())
					continue;

				if (!_input_mask)
					return tv;

				if (_input_mask[(y - tv->y())*_input_mask_w + (x - tv->x())])
					return tv;
			}

			return 0;
		}

		void top(Test_view *tv)
		{
			remove(tv);
			tv->top();
			insert(tv);
		}
};
extern "C" void *malloc(::size_t size);
struct Main
{
        Genode::Env &_env;

        Genode::Attached_rom_dataspace  _config_rom { _env, "config" };
Main(Genode::Env &env) : _env(env)
        {
//void  Libc::Component::construct(Libc::Env &env)//Genode::Env &env)
//{
        localenv=&env;
	/*
	 * Init sessions to the required external services
	 */
		Libc::with_libc([&] () {
	//printf("Hello World\n");
         //Genode::Allocator xx = malloc(1024);
         //init_fd_alloc(xx);
        //cmos_init();
	
	setjittable();
	int *p = (int*)malloc(8);
	perror("malloc\n");
	p[0]=1;
        fdc_init();
        initvideo();

        //::printf("Hello World\n");

        cp15_init();
        //::printf("Hello World2\n");
        mem_init();
        //::printf("Hello World3\n");
        loadroms();
        //::printf("Hello World4\n");
        arm_init();
	cmos_init();
	//hostfs_init();
        //::printf("Hello World\n");
        resetarm(CPUModel_SA110);//CPUModel_ARM610); CPUModel_SA110);
	printf("1\n");
	keyboard_reset();
	printf("2\n");
        initcodeblocks();
	initpodulerom();
	printf("3\n");
        mem_reset(128,2);
	printf("4\n");
        iomd_reset(IOMDType_IOMD);
	printf("5\n");
        //cmos_reset();
	cp15_reset(CPUModel_SA110);//machine.cpu_model);
	printf("6\n");
	reseti2c(I2C_PCF8583);
        //resetide();
	printf("7\n");
        superio_reset(SuperIOType_FDC37C665GT);
	printf("8\n");
        i8042_reset();
	printf("9\n");
        //cmos_reset();
	printf("10\n");
        podules_reset();
	printf("11\n");
        podulerom_reset(); 
	printf("12\n");
	hostfs_reset();
	// must be called after podules_reset()

	if (pthread_create(&time_thread, NULL, timerunner, NULL)) {
                fatal("Couldn't create timer thread");
        }
        //::printf("Hello World\n");
        //dumpregs();
//execarm(20000);
//dumpregs();
        //execarm(20000);
	printf("2\n");
        //dumpregs();
	enum { CONFIG_ALPHA = false };
	static Gui::Connection   gui   { env, "RISCOS" }; //testnit" };
	static Timer::Connection timer { env };

	Framebuffer::Mode const mode { .area = { 640, 480 } };
	gui.buffer(mode, false);

	int const scr_w = mode.area.w(), scr_h = mode.area.h();

	log("screen is ", mode);
	if (!scr_w || !scr_h) {
		error("got invalid screen - sleeping forever");
		return;
	}

	/* bad-case test */
	{
		/* issue #3232 */
		Gui::Session::View_handle handle { gui.create_view() };
		gui.destroy_view(handle);
		gui.destroy_view(handle);
	}

	Genode::Attached_dataspace fb_ds(
		env.rm(), gui.framebuffer()->dataspace());

	typedef Genode::Pixel_rgb888 PT;
	PT *pixels = fb_ds.local_addr<PT>();
	unsigned char *alpha = (unsigned char *)&pixels[scr_w*scr_h];
	unsigned char *input_mask = CONFIG_ALPHA ? alpha + scr_w*scr_h : 0;

	/*
	 * Paint into pixel buffer, fill alpha channel and input-mask buffer
	 *
	 * Input should refer to the view if the alpha value is more than 50%.
	 */
	//int *rofb2 = (int*)rofb;
#if 0
	for (int i = 0; i < scr_h; i++)
		for (int j = 0; j < scr_w; j++) {
			pixels[i*scr_w + j] = PT((3*i)/8, j, i*j/32);
			if (CONFIG_ALPHA) {
				alpha[i*scr_w + j] = (i*2) ^ (j*2);
				input_mask[i*scr_w + j] = alpha[i*scr_w + j] > 127;
			}
		}

	/*
	 * Create views to display the buffer
	 */
	Test_view_stack tvs(input_mask, scr_w);

	{
		/*
		 * View 'v1' is used as coordinate origin of 'v2' and 'v3'.
		 */
		static Test_view v1(&gui, 150, 100, 230, 200, "Eins");
		static Test_view v2(&gui,  20,  20, 230, 210, "Zwei", &v1);
		static Test_view v3(&gui,  40,  40, 230, 220, "Drei", &v1);

		tvs.insert(&v1);
		tvs.insert(&v2);
		tvs.insert(&v3);
	}
#endif
	/*
	 * Handle input events
	 */

//	struct timespec timetest1; timetest2;

	int mx = 0, my = 0, key_cnt = 0;
	int dwc = 0;
	Test_view *tv = nullptr;
	//rofb = malloc(1024*768*4);
			//drawscr(1);
	//Genode::uint64_t timediff = 0;
	//Genode::uint64_t ntime; oldtime = 0;
//clock_gettime(CLOCK_MONOTONIC,&timetest1);
//	ntime = timetest1.tv_nsec;
	//timediff = ntime;
	while (1) {
//		Framebuffer::Mode const mode2 { .area = { (uint32_t)vidc_get_xsize(), (uint32_t)vidc_get_ysize()} };
  //      gui.buffer(mode2, false);
//	printf("time %ld\n",ntime);

	//timetest.tv_nsec=2000000;
	

			//printf("after execarm111111111111\n");
			//kcallback=1;
			//mcallback=1;
			//fdccallback=1;
			//idecallback=1;
//			printf("bedfore exec\n");
			//pthread_mutex_unlock(&timer_mutex);
		//	gentimerirq();
//			printf("after exec\n");
			//dumpregs();
//			printf("333\n");
#if 0
		clock_gettime(CLOCK_MONOTONIC,&timetest1);
	oldtime = timetest1.tv_nsec-ntime;
	//timediff+=oldtime-ntime;
			if (oldtime>=timediff){
			timediff=oldtime+2000000;
			gentimerirq();
			
			}
#endif	

#if 0
			if (!armirq)
			{
			if (idecallback) {
			printf("IDECALLBACK: %d\n",idecallback);
			idecallback -= 10;
                        if (idecallback <= 0) {
                                idecallback = 0;
                                callbackide();
			printf("IDECALLBACK:  END\n");
                        }
			}
			}
#endif

	//		execarm(2000);
//			printf("before drawscr\n");
//			printf("after drawscr\n");
			//execarm(20000);
	//		printf("execarm2\n");
					//	execarm(20000);
//drawscr(1);
			dwc++;
			if (dwc>5)
				dwc=0;
			//vidcthread();
			//printf("after vidcthr\n");
			//void * tfb = malloc(100*100*4);
			//::memcpy(fb_ds.local_addr<void>(),rofb,640*480*4);
			//printf("xsize %d\n",vidc_get_xsize());
			//printf("beofre blit\n");
			blit(rofb, 640*4,
                           fb_ds.local_addr<void>(), 640*4,640*4,480); //scr_w*4, 1024*4, 768);
			gui.framebuffer()->refresh(0,0,640,480);
			//printf("after blit\n");
			//sleep(1);
#if 0
//	if (!armirq) {
			if (idecallback) {
                        idecallback -= 10;
                        idecallback =0;
                        if (idecallback <= 0) {
                                idecallback = 0;
                                callbackide();
                        }

			}
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

//	}
#endif

#if 1
	Test_view_stack tvs(input_mask, scr_w);

	{
		/*
		 * View 'v1' is used as coordinate origin of 'v2' and 'v3'.
		 */
		//static Test_view v1(&gui, 150, 100, 230, 200, "Eins");
		static Test_view v1(&gui, 300, 100, 640, 480, "Eins");

		//static Test_view v2(&gui,  20,  20, 230, 210, "Zwei", &v1);
		//static Test_view v3(&gui,  40,  40, 230, 220, "Drei", &v1);

		tvs.insert(&v1);
		//tvs.insert(&v2);
		//tvs.insert(&v3);
	}
#endif
//		printf("1\n");
		int oldx,oldy;
		oldx=0;
		oldy=0;
	//	while (!gui.input()->pending()) timer.msleep(20);
		gui.input()->for_each_event([&] (Input::Event const &ev) {

//		printf("1\n");
			if (ev.press())   key_cnt++;
			if (ev.release()) key_cnt--;

			if (ev.relative_motion()) {
			printf("relative genode\n");
			ev.handle_relative_motion([&] (int x, int y) {


                         mouse_mouse_move_relative(x,y);



					});
			}
			ev.handle_absolute_motion([&] (int x, int y) {
			if (tv) {
			int x2,y2=0;
			x2=(x-(tv->x()));
			y2=(y-(tv->y()));
			//x3 = x2-oldx;
			//y3 = y2-oldy;
			//printf("x3 %d y3 %d\n",x3,y3);
				mouse_mouse_move_relative(x2,y2);	
					/* move selected view */
			oldx = x2;
			oldy = y2;
			}	
	//		if (key_cnt > 0 && tv)
	//				tv->move(tv->x() + x - mx, tv->y() + y - my);

//		printf("3\n");
				mx = x; my = y;
			});

			/* find selected view and bring it to front */
			if (ev.press() /*&& key_cnt == 1*/) {
				ev.handle_press([&] (Input::Keycode key, Codepoint code) {
				//		printf("PRESS %d key 0x%x\n",key,key);
				NOT_USED(code);
				if (key!=0x110 && key!=0x111 && key!=0x112)
				{
				//printf("keyboard\n");
				keyboard_key_press(keyboard_map_key(key));	


				} else {
				tv = tvs.find(mx, my);
				if (tv)
				{
					tvs.top(tv);
				}
				//uint8_t set_2[8];
				//set_2[0]=0x76;
				if (key==0x110){
				//printf("left button\n");
				mouse_mouse_press(1);
				}
				if (key==0x111)
					mouse_mouse_press(2);
				if (key==0x112)
					mouse_mouse_press(4);
				//keyboard_key_press(set_2);
			}});
			}
			if (ev.release())
			{
				ev.handle_release([&] (Input::Keycode key) {

				if (key!=0x110 && key!=0x111 && key!=0x112)
				{
				printf("keyboard\n");
				keyboard_key_release(keyboard_map_key(key));	


				} else {

				if (key==0x110){
				printf("left button\n");
				mouse_mouse_release(1);
				}
				if (key==0x111)
					mouse_mouse_release(2);
				if (key==0x112)
					mouse_mouse_release(4);
					}
				});
			}
		});
//		printf("4\n");
		//oldtime=timetest.tv_nsec;
	}

	env.parent().exit(0);
	});
}
};
int main()
{
printf("MAIN\n");
//Libc::Component::construct(Libc::Env &env) { static Main main(env); }
while (1) {}
return (0);
}
//void Libc::Component::construct(Libc::Env &env) {
  //      with_libc([&env] () { static MainTestB application   (env); }); }
void Libc::Component::construct(Libc::Env &env) { static Main main(env); }

