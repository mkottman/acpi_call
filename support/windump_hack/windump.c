#include <sys/ipc.h>
#include <sys/shm.h>
#include <X11/Xos.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/cursorfont.h>
#include <X11/extensions/XShm.h>
#include <X11/extensions/XTest.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

int format = ZPixmap;

XImage *
CaptRoot(Display * dpy, int screen)
{
    unsigned long swaptest = 1;
    XColor *colors;
    unsigned buffer_size;
    int win_name_size;
    int header_size;
    int ncolors, i;
    char *win_name;
    Bool got_win_name;
    XWindowAttributes win_info;
    XImage *image;
    int absx, absy, x, y;
    unsigned width, height;
    int dwidth, dheight;
    int bw;
    Window dummywin;

#if 0
    int                 transparentOverlays , multiVis;
    int                 numVisuals;
    XVisualInfo         *pVisuals;
    int                 numOverlayVisuals;
    OverlayInfo         *pOverlayVisuals;
    int                 numImageVisuals;
    XVisualInfo         **pImageVisuals;
    list_ptr            vis_regions;    /* list of regions to read from */
    list_ptr            vis_image_regions ;
    Visual		vis_h,*vis ;
#endif
    int			allImage = 0 ;

    Window window=RootWindow (dpy, screen);

    if (!XGetWindowAttributes(dpy, window, &win_info))
    { fprintf(stderr,"Can't get target window attributes."); exit(1); }

    absx=0; absy=0;
    win_info.x = 0;
    win_info.y = 0;
    width = win_info.width;
    height = win_info.height;
    bw = 0;

    dwidth = DisplayWidth (dpy, screen);
    dheight = DisplayHeight (dpy, screen);

    XFetchName(dpy, window, &win_name);
    if (!win_name || !win_name[0]) {
	win_name = "xwdump";
	got_win_name = False;
    } else {
	got_win_name = True;
    }

    /* sizeof(char) is included for the null string terminator. */
    win_name_size = strlen(win_name) + sizeof(char);

    /*
     * Snarf the pixmap with XGetImage.
     */

    x = absx - win_info.x;
    y = absy - win_info.y;
#if 0
    multiVis = GetMultiVisualRegions(dpy,RootWindow(dpy, screen),absx,absy, 
	       width,height,&transparentOverlays,&numVisuals,&pVisuals,
               &numOverlayVisuals,&pOverlayVisuals,&numImageVisuals,
               &pImageVisuals,&vis_regions,&vis_image_regions,&allImage) ;
    if (on_root || multiVis)
    {
	if (!multiVis)
	    image = XGetImage (dpy, RootWindow(dpy, screen), absx, absy, 
                    width, height, AllPlanes, format);
	else
	    image = ReadAreaToImage(dpy, RootWindow(dpy, screen), absx, absy, 
                width, height,
    		numVisuals,pVisuals,numOverlayVisuals,pOverlayVisuals,
                numImageVisuals, pImageVisuals,vis_regions,
                vis_image_regions,format,allImage);
    }
    else
#endif
	image = XGetImage (dpy, window, x, y, width, height,
			   AllPlanes, format);
    if (!image) {
	fprintf (stderr, "unable to get image at %dx%d+%d+%d\n",
		 width, height, x, y);
	exit (1);
    }

    //if (add_pixel_value != 0) XAddPixel (image, add_pixel_value);

    return image;
}

Window CreateWindow(Display * dpy, int screen, int width, int height)
{
    Window win = 0;
    win = XCreateWindow(dpy, RootWindow(dpy, screen),
                        0, 0, width, height,
                        0,
                        24,
                        InputOutput,
                        DefaultVisual(dpy, screen),
                        //CWBorderPixel|CWBackPixel|CWColormap|CWEventMask|CWBitGravity,
                        0,
                        0);

    XSelectInput(dpy, win, StructureNotifyMask|ButtonPressMask|ButtonReleaseMask);

    XMapWindow(dpy, win);
    return win;
}

void DrawImage(Display * dpy, Window win, XImage * image)
{
    static GC gc;
    if(win!=0)
    {
        if(gc==0)
        {
            XGCValues gc_val;
            gc = XCreateGC (dpy, win, GCForeground|GCBackground, &gc_val);
        }
        /*for(i=0;i<100;i++)
        {
            image->data*/
        XPutImage (dpy, win, gc, image, 0, 0, 0, 0, 1024, 768);
    }
}

void createShmImage(int w, int h, XShmSegmentInfo * sinfo, XShmSegmentInfo * tinfo, Display * sdpy, Display * tdpy, int sscr, int tscr, XImage ** simage, XImage ** timage)
{
    sinfo->shmid=tinfo->shmid=shmget(IPC_PRIVATE,w*h*sizeof(unsigned),IPC_CREAT|0666 );
    sinfo->shmaddr=tinfo->shmaddr=(char*)shmat(sinfo->shmid,0,0);
    sinfo->readOnly=tinfo->readOnly=False;
    printf("%d %d\n",DefaultDepth(sdpy,sscr),DefaultDepth(tdpy,tscr));
    *simage = XShmCreateImage(
                              sdpy, DefaultVisual(sdpy,sscr), DefaultDepth(sdpy,sscr),
                              ZPixmap/*format*/,
                              (char *)sinfo->shmaddr, sinfo,
                              w,h /*width,height*/
                             );
    *timage = XShmCreateImage(
                              tdpy, DefaultVisual(tdpy,tscr), DefaultDepth(tdpy,tscr),
                              ZPixmap/*format*/,
                              (char *)tinfo->shmaddr, tinfo,
                              w,h /*width,height*/
                             );
    XShmAttach(sdpy, sinfo);
    XShmAttach(tdpy, tinfo);
}

void drawMouse(XImage * img, int xm, int ym)
{
    int x,y,x1,y1,x2,y2,w,h;

    unsigned * data = (unsigned *)(img->data);

    w=img->width;
    h=img->height;

    x=xm; if(x<0) x=0; if(x>w) x=w-1;
    y=ym; if(y<0) y=0; if(y>h) y=h-1;
    x1=xm-5; if(x1<0) x1=0; if(x1>w) x1=w;
    x2=xm+6; if(x2<0) x2=0; if(x2>w) x2=w;
    for(x=x1;x<x2;x++) data[y*w+x] = 0xFFFFFF;
    //data[y*w+x] = 0xFFFFFF;

    x=xm; if(x<0) x=0; if(x>w) x=w-1;
    y=ym; if(y<0) y=0; if(y>h) y=h-1;
    y1=ym-5; if(y1<0) y1=0; if(y1>h) y1=h;
    y2=ym+6; if(y2<0) y2=0; if(y2>h) y2=h;
    for(y=y1;y<y2;y++) data[y*w+x] = 0xFFFFFF;

}

main(int argc, char * argv [])
{
    Display * sdpy = XOpenDisplay(argv[1]);
    Display * tdpy = XOpenDisplay(argv[2]);
    int sscr = XDefaultScreen(sdpy);
    int tscr = XDefaultScreen(tdpy);
    GC tgc = DefaultGC(tdpy,tscr);
    Window swin=RootWindow (sdpy,sscr);
    int width, height, dummy;
    XGetGeometry(sdpy, swin, (Window *)&dummy, &dummy, &dummy, &width, &height, &dummy, &dummy);
    Window twin=CreateWindow(tdpy,tscr,width,height);
    XSelectInput(sdpy, swin, PointerMotionMask);
    XImage * image;
    XImage * simage;
    XImage * timage;
    int use_shm=1;
    XShmSegmentInfo xshm_sinfo;
    XShmSegmentInfo xshm_tinfo;
    if(use_shm) createShmImage(width,height,&xshm_sinfo,&xshm_tinfo,sdpy,tdpy,sscr,tscr,&simage,&timage);
    int frame=0;
    for(;;) {
        XEvent e;
        XNextEvent(tdpy, &e);
        if (e.type == MapNotify)
            break;
    }

    int emulate_events=0;

    int xmouse, ymouse;
    while(1)
    {
        {
            XEvent e;
            //long mask=ButtonPressMask|ButtonReleaseMask|MotionNotifyMask;
            //while(XCheckWindowEvent(tdpy, twin, mask, &e)!=False)
            while(XCheckTypedWindowEvent(tdpy, twin, ButtonPress, &e)!=False ||
                  XCheckTypedWindowEvent(tdpy, twin, ButtonRelease, &e)!=False)
            {
                printf("button event\n");
                if(emulate_events)
                {
                    e.xbutton.display=sdpy;
                    e.xbutton.window=swin;
                    e.xbutton.root=swin;
                    e.xbutton.window=swin;
                    e.xbutton.x_root=e.xbutton.x;
                    e.xbutton.y_root=e.xbutton.y;
                    //XSendEvent( sdpy, swin, True, mask, &e );
                    XPutBackEvent( sdpy, &e );
                    //XTestFakeMotionEvent(sdpy,sscr,e.xbutton.x,e.xbutton.y,0);
                    XTestFakeButtonEvent(sdpy,e.xbutton.button,e.xbutton.type==ButtonPress,0);
                }
            }
            while(XCheckTypedWindowEvent(sdpy, swin, MotionNotify, &e)!=False)
            {
                //printf("motion event\n");
                xmouse=e.xbutton.x_root;
                ymouse=e.xbutton.y_root;
            }
        }
        //printf("frame %d\n", frame++);
        usleep(60000);
        if(!use_shm)
        {
            image=CaptRoot(sdpy,sscr);
            DrawImage(tdpy,twin,image);
            XDestroyImage(image);
        }
        else
        {
//            XShmAttach(sdpy, &xshm_sinfo);
            XShmGetImage (sdpy, swin, simage, 0, 0, AllPlanes);
//            XShmAttach(sdpy, &xshm_tinfo);
            //printf("simage: w:%d h:%d d:%d\n",simage->width, simage->height, simage->depth);
            //printf("timage: w:%d h:%d d:%d\n",timage->width, timage->height, timage->depth);
            if(!emulate_events)
            {
                Window rwin,cwin;
                int xmouse,ymouse,x,y,mask;
                XQueryPointer(sdpy,swin,&rwin,&cwin,&xmouse,&ymouse,&x,&y,&mask);
                drawMouse(timage, xmouse, ymouse);
            }

            XShmPutImage (tdpy, twin, tgc, timage, 0, 0, 0, 0, timage->width, timage->height, False);
            //XPutImage (tdpy, twin, tgc, timage, 0, 0, 0, 0, timage->width, timage->height);
        }
        //XFlush(sdpy);
        XFlush(tdpy);
//        getchar();
    }
}
