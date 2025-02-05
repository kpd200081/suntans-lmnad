/*
 * File: sunplot.c
 * Author: Oliver B. Fringer
 * -------------------------
 * Plot the output of suntans using the X11 libraries.
 *
 * Copyright (C) 2005-2006 The Board of Trustees of the Leland Stanford Junior 
 * University. All Rights Reserved. 
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xos.h>
#include <X11/Xatom.h>
#include <X11/keysym.h>
#include "math.h"
#include "suntans.h"
#include "fileio.h"
#include <sys/stat.h>
#include <sys/errno.h>
#include <unistd.h>
#include "cmap.h"

#define AXESSLICETOP 0  // Change this to change the distance from the axes edge to the free-surface (percentage of axes height)
#define SMALLHEIGHT .001
#define VERSION "0.0.0"
#define WIDTH 500
#define HEIGHT 500
#define PI 3.141592654
#define EMPTY 999999
#define CMAPFILE "cmaps/jet.cmap"
#define NUMCOLORS 66
//#define DEFAULT_FONT "9x15"
//#define DEFAULT_FONT "xterm"
#define DEFAULT_FONT "fixed"
#define WINSCALE 0
#define WINLEFT .2
#define WINTOP .2
#define WINWIDTH .6
#define WINHEIGHT .7
#define WINWIDTHPIXELS 1024
#define WINHEIGHTPIXELS 768
#define AXESLEFT 0.25
#define AXESBOTTOM 0.05
#define AXESWIDTH 0.625
#define AXESHEIGHT 0.9
#define BUTTONHEIGHT 20
#define ZOOMFACTOR 2.0
#define MINZOOMRATIO 1/100.0
#define MAXZOOMRATIO 100.0
#define NUMBUTTONS 38
#define POINTSIZE 2
#define NSLICEMAX 8000
#define NSLICEMIN 2
#define AXESBIGRATIO 1e10
#define MINAXESASPECT 1e-2
#define MAX_SED_SIZES 5

typedef enum {
  in, out, box, pan, none
} zoomT;

typedef enum {
  false, true
} bool;

typedef enum {
  left, center, right
} hjustifyT;

typedef enum {
  bottom, middle, top
} vjustifyT;

typedef enum {
  left_button=1, middle_button=2, right_button=3
} buttonnumT;

typedef enum {
  prevwin, gowin, nextwin,
  kupwin, kdownwin,
  prevprocwin, allprocswin, nextprocwin,
  allsediwin, prevsediwin, nextsediwin, 
  layerwin, taubwin,
  saltwin, tempwin, preswin, fswin,
  uwin, vwin, wwin, vecwin, nutwin, ktwin,  
  depthwin, nonewin,
  edgewin, voronoiwin, delaunaywin,
  zoomwin, profwin, quitwin, reloadwin, axisimagewin, cmapholdwin,
  iskip_plus_win,iskip_minus_win,kskip_plus_win,kskip_minus_win
} buttonName;

typedef enum {
  oneproc, allprocs
} plotProcT;

typedef enum {
  noslice, value, vertp, slice, value_keyboard
} sliceT;

typedef enum {
  nomovie, forward, backward
} goT;

typedef enum {
  noplottype, freesurface, depth, h_d, salinity, temperature, pressure, saldiff, 
  sedic, allsedic, layer, taub,
  salinity0, u_velocity, v_velocity, w_velocity, u_baroclinic, v_baroclinic,
  nut, kt
} plottypeT;

typedef enum {
  two_d, three_d
} dimT;

typedef struct {
  char *string;
  char *mapstring;
  float l,b,w,h;
  Window butwin;
  bool status;
} myButtonT;

typedef struct {
  float *xc;
  float *yc;
  float **xv;
  float **yv;
  int **cells;
  int **nfaces;
  int **edges;
  int **mark;
  int **face;
  float **depth;

  float ***s;
  float ***sd;
  float ***s0;
  float ***T;
  float ***q;
  float ***u;
  float ***v;
  float ***nut;
  float ***kt;
  float **ubar;
  float **vbar;
  float ***wf;
  float ***w;
  float **h;
  float **h_d;

  float ****sediC;
  float ***allsediC;
  float **layerthickness;
  float **initial_layerthickness;
  float **taub;

  float *currptr;

  float **sliceData;
  float **sliceU;
  float **sliceW;
  float *sliceH;
  float *sliceD;
  float *z;
  float *sliceX;
  int *sliceInd;
  int *sliceProc;

  int *Ne, *Nc, Np, Nkmax, nsteps, numprocs, Nslice, noutput, ntout, maxfaces,sedinum,tbmax,sediplotnum;
  int timestep, klevel, sedi, Nsize;
  int *freesurface_step_loaded, *salinity_step_loaded, *temperature_step_loaded, *pressure_step_loaded,
    *velocity_step_loaded, *nut_step_loaded, *kt_step_loaded, *layer_step_loaded, **sedi_step_loaded, *taub_step_loaded,
    bg_salinity_loaded, initial_layer_loaded;

  float umagmax, dmin, dmax, hmax, hmin, rx, ry, Emagmax;
} dataT;

float GetDmaxSlice(float *depth, int N);
int GetStep(dataT *data);
void showloadstats(int step, plottypeT plottype);
void GetCAxes(REAL datamin, REAL datamax);
int GetNumber(int min, int max);
void AllButtonsFalse(void);
void Sort(int *ind1, int *ind2, float *data, int N);
void QuadSurf(float *h, float *D, 
	      float **data, float *x, float *z, int N, int Nk,plottypeT plottype);
void QuadContour(float *h, float *D, 
	      float **data, float *x, float *z, int N, int Nk,plottypeT plottype);
void GetSlice(dataT *data, int xs, int ys, int xe, int ye, 
	      int procnum, int numprocs, plottypeT plottype);
void GetDMax(dataT *data, int numprocs);
void GetDMin(dataT *data, int numprocs);
void GetHMax(dataT *data, int numprocs);
void GetUMagMax(dataT *data, int klevel, int numprocs);
void DrawEdgeLines(float *xc, float *yc, int *cells, plottypeT plottype, int N, int maxfaces, int *nfaces);
void DrawBoundaries(float *xc, float *yc, int *edges, plottypeT plottype, int *mark, int Ne);
void DrawDelaunayPoints(float *xc, float *yc, plottypeT plottype, int Np);
float *GetScalarPointer(dataT *data, plottypeT plottype, int klevel, int proc);
dataT *NewData(int numprocs);
void FreeGrid(void);
void FindNearest(dataT *data, int x, int y, int *iloc, int *procloc, int procnum, int numprocs);
void InitializeGraphics(void);
void MyDraw(dataT *data, plottypeT plottype, int procnum, int numprocs, int iloc, int procloc);
void LoopDraw(dataT *data, plottypeT plottype, int procnum, int numprocs);
void ReadGrid(int proc);
void ReadScalar(float *scal, int k, int Nk, int np, char *filename);
int GetFile(char *string, char *datadir, char *datafile, char *name, int proc);
void ReadVelocity(float *u, float *v, int kp, int Nk, int np , char *filename);
float Min(float *x, int N);
float Max(float *x, int N);
void AxisImage(float *axes, float *data);
void Fill(XPoint *vertices, int N, int cindex, int edges);
void CAxis(dataT *data, plottypeT plottype, int klevel, int procnum, int numprocs);
int LoadCAxis(void);
void ReadColorMap(char *str);
void UnSurf(float *xc, float *yc, int *cells, float *data, int N, int maxfaces, int *nfaces);
void SetDataLimits(dataT *data);
void SetAxesPosition(void);
void Clf(void);
void Cla(void);
void Text(Window window, float x, float y, int boxwidth, int boxheight,
	  char *str, int fontsize, int color, 
	  hjustifyT hjustify, vjustifyT vjustify);
void DrawControls(dataT *data, int procnum, int numprocs);
void DrawColorBar(dataT *data, int procnum, int numprocs, plottypeT plottype);
Window NewButton(Window parent, char *name, int x, int y, 
		 int buttonwidth, int buttonheight, bool motion, int bordercolor);
void DrawButton(Window button, char *str, int bcolor);
void MapWindows(void);
void RedrawWindows(void);
void DrawZoomBox(void);
void DrawSliceLine(int where);
void DrawHeader(Window leftwin,Window rightwin,char *str);
void SetUpButtons(void);
void DrawVoronoiPoints(float *xv, float *yv, int Nc);
void FillCircle(int xp, int yp, int r, int ic, Window mywin);
void UnQuiver(int *edges, float *xc, float *yc, float *xv, float *yv, 
	      float *u, float *v, float umagmax, int Ne, int Nc);
void Quiver(float *x, float *z, float *D, float *H,
	    float **u, float **v, float umagmax, int Nk, int Nc);
void DrawArrow(int xp, int yp, int ue, int ve, Window mywin, int ic);
void Rotate(XPoint *points, int N, int ue, int ve, int mag);
void ParseCommandLine(int N, char *argv[], int *numprocs, int *n, int *k, dimT *dimensions, goT *go);
void ShowMessage(void);
void ReadData(dataT *data, int nstep, int numprocs);
void FreeData(dataT *data, int numprocs);
void CloseGraphics(void);
void Usage(char *str);

/*
 * Linux users will need to add -ldl to the Makefile to compile 
 * this example.
 *
 */
Display *dis;
myButtonT controlButtons[NUMBUTTONS];
Window win, axeswin, messagewin, controlswin, cmapwin;
XEvent report;
GC gc, fontgc;
XColor color;
Screen *screen;
Pixmap pix, zpix, controlspix, cmappix;
Colormap colormap;
KeySym lookup;
XWindowAttributes windowAttributes;
XFontStruct *fontStruct;

int keyboardproc=0, keyboardcell=0;
int width=WIDTH, height=HEIGHT, newwidth, newheight, 
  Np0, Nc0, Ne0, n=1, k=0, keysym,
  xstart, ystart, xend, yend, lastgridread=0, iskip=1, kskip=1, iskipmax=5;
int DEBUG=false;
//int *cells, *edges;
//float caxis[2], *xc, *yc, *depth, *xp, axesPosition[4], dataLimits[4], buttonAxesPosition[4],
//  zoomratio, *xv, *yv, vlengthfactor=1.0;
float caxis[2], axesPosition[4], dataLimits[4], buttonAxesPosition[4], cmapAxesPosition[4],
  zoomratio, vlengthfactor=1.0;
int axisType, oldaxisType, white, black, red, blue, green, yellow, colors[NUMCOLORS];
bool edgelines, setdatalimits, pressed,   voronoipoints, delaunaypoints, vectorplot, goprocs,
  vertprofile, fromprofile, gridread, setdatalimitsslice, zooming, panning, cmaphold, getcmap, raisewindow, boundaries;
char str[BUFFERLENGTH], message[BUFFERLENGTH];
zoomT zoom;
plotProcT procplottype;
sliceT sliceType;
plottypeT plottype = salinity;
int plotGrid = false;
int mergeArrays;

int main(int argc, char *argv[]) {
  int procnum=0, numprocs, status;
  bool redraw, quit=false;
  buttonnumT mousebutton;
  dimT dims;
  setdatalimits = false;
  setdatalimitsslice = false;
  vectorplot = false;
  zoomratio = 1;
  zooming = true;
  panning = false;
  procplottype = oneproc;
  vertprofile = false;
  gridread = false;
  cmaphold = false;
  getcmap = false;
  raisewindow = false;
  goT go;

  ParseCommandLine(argc,argv,&numprocs,&n,&k,&dims,&go);  
  mergeArrays=(int)GetValue(DATAFILE,"mergeArrays",&status);
  if(!status)
    mergeArrays=1;
  if(mergeArrays) {
    if(numprocs!=1) printf("Since mergeArrays==1 then ignoring \"-np %d\" and setting numprocs=1\n",numprocs);
    numprocs=1;
  }

  dataT *data = NewData(numprocs);
    
  ReadData(data,-1,numprocs);

  InitializeGraphics();

  ReadColorMap(CMAPFILE);

  XSelectInput(dis, win, ExposureMask | KeyPressMask | ButtonPressMask | ButtonReleaseMask);
  /* get name for window */
  char windowtitle[BUFFERLENGTH];
  char cwd[1024];
  // get current directory and store name
  if (getcwd(cwd, sizeof(cwd)) != NULL)
    sprintf(windowtitle,"SunPlot: %s: %s", cwd,DATADIR);
  else
    sprintf(windowtitle,"Sunplot");
  XStoreName(dis, win, windowtitle);

  axisType='i';
  boundaries=true;
  edgelines=false;
  voronoipoints=false;
  delaunaypoints=false;

  if(plotGrid) {
    plottype = noplottype;
    edgelines=true;
    voronoipoints=true;
  }

  SetUpButtons();

  MapWindows();
  LoopDraw(data,plottype,procnum,numprocs);

  XMaskEvent(dis, ExposureMask, &report);
  pressed=false;
  goprocs=true;
  sprintf(message,"SUNPLOT v %s",VERSION);

  if(dims==two_d) {
    sliceType=slice;
    vertprofile=true;
    xstart=0;
    ystart=height/2*axesPosition[3];
    xend=width*axesPosition[2];
    yend=height/2*axesPosition[3];
    axisType='n';
  }

  while(true) {

    data->noutput=GetNumOutput(data->ntout);
    if(data->noutput==-1)
      data->noutput=data->nsteps;
    if(dims==two_d) {
      fromprofile=true;
      dims=three_d;
    } else
      fromprofile=false;

    redraw=false;
    zoom=none;
    if(go!=nomovie) {
      if(go==forward) {
	if(n<data->noutput) {
	  sprintf(message,"Stepping through...");
	  redraw=true;
	  n++;
	} else {
	  redraw=false;
	  go=nomovie;
	  sprintf(message,"Done!");
	}
      } else if(go==backward) {
	if(n>1) {
	  sprintf(message,"Stepping backwards...");
	  redraw=true;
	  n--;
       	} else {
	  redraw=false;
	  go=nomovie;
	  sprintf(message,"Done!");
	}
      }
    } else {
    XNextEvent(dis, &report);
    switch  (report.type) {
    case MotionNotify:
      xend=report.xbutton.x;
      yend=report.xbutton.y;
      XCopyArea(dis,pix,axeswin,gc,0,0,
		axesPosition[2]*width,
		axesPosition[3]*height,0,0);
      XCopyArea(dis,controlspix,controlswin,gc,0,0,
		buttonAxesPosition[2]*width,
		buttonAxesPosition[3]*height,0,0);
      XCopyArea(dis,cmappix,cmapwin,gc,0,0,
		cmapAxesPosition[2]*width,
		cmapAxesPosition[3]*height,0,0);
      if(zooming){
        //  added code to account for panning behavior
        if(panning){
          DrawArrow(xstart,ystart,(xend-xstart),-(yend-ystart),axeswin,white);
        }
        else
          DrawZoomBox();
      }
      else {
	DrawSliceLine(0);
	vertprofile=true;
      }
      break;
    case ButtonPress:
      if(report.xany.window==axeswin) {
	xstart=report.xbutton.x;
	ystart=report.xbutton.y;
	pressed=true;
      }
      break;
    case ButtonRelease:
      mousebutton = report.xbutton.button;
      if(mousebutton==middle_button) {
        // print location of cursor to console
	/*
        printf("Cursor location (%f,%f)\n",
	       dataLimits[0]+(float)report.xbutton.x*(dataLimits[1]-dataLimits[0])
	       /(axesPosition[2]*(float)width),
	       dataLimits[2]+(float)(height*axesPosition[3]-report.xbutton.y)
	       *(dataLimits[3]-dataLimits[2])/(axesPosition[3]*(float)height));
	       */
      }
      if(report.xany.window==controlButtons[prevwin].butwin) {
	if(mousebutton==left_button) {
	  if(n>1) { redraw = true; n--; }
	  else { redraw = false ; sprintf(message,"At n=1!"); }
	} else if(mousebutton==right_button)
	  if(n!=1) { redraw = true ; n=1; }
      } else if(report.xany.window==controlButtons[gowin].butwin) {
	if(mousebutton==left_button) {
	  go=forward;
	} else if(mousebutton==right_button) {
	  go=backward;
	} else if(mousebutton==middle_button) {
	  n=GetStep(data);
	  redraw=true;
	}
      } else if(report.xany.window==controlButtons[nextwin].butwin) {
	if(mousebutton==left_button) {
	  if(n<data->noutput) { redraw = true; n++; }
	  else { redraw = false ; sprintf(message,"At n=noutput!"); }
	} else if(mousebutton==right_button) 
	  if(n!=data->noutput) { redraw = true; n=data->noutput; }
      } else if(report.xany.window==controlButtons[kdownwin].butwin) {
	if(mousebutton==left_button) {
	  if(k>0) { redraw = true; k--; }
	  else { redraw = false ; sprintf(message,"At k=1!"); }
	} else if(mousebutton==right_button)
	  if(k!=0) { redraw = true ; k=0; }
      } else if(report.xany.window==controlButtons[kupwin].butwin) {
	if(mousebutton==left_button) {
	  if(k<data->Nkmax-1) { redraw = true; k++; }
	  else { redraw = false ; sprintf(message,"At k=Nkmax!"); }
	} else if(mousebutton==right_button) 
	  if(k!=data->Nkmax-1) { redraw = true; k=data->Nkmax-1; }
      } else if(report.xany.window==controlButtons[prevprocwin].butwin && numprocs>1) {
	if(mousebutton==left_button) {
	  if(procplottype==allprocs) {
	    procnum=0;
	    procplottype=oneproc;
	    redraw=true;
	  } else {
	    if(procnum==0) 
	      procnum = numprocs-1;
	    else
	      procnum--;
	    redraw = true; 
	    procplottype=oneproc; 
	  }
	} else if(mousebutton==right_button) 
	  if(procplottype==allprocs) {
	    procnum=0;
	    procplottype=oneproc;
	    redraw=true;
	  } else if(procnum!=0) { 
	    redraw = true; 
	    procnum=0; 
	    procplottype=oneproc; 
	  }
      } else if(report.xany.window==controlButtons[allprocswin].butwin
		&& mousebutton==left_button && numprocs>1) {
	sprintf(message,"Plotting all procs...");
	procplottype=allprocs;
	redraw=true;
      } else if(report.xany.window==controlButtons[nextprocwin].butwin && numprocs>1) {
	if(mousebutton==left_button) {
	  if(procplottype==allprocs) {
	    procnum=numprocs-1;
	    procplottype=oneproc;
	    redraw=true;
	  } else {
	    if(procnum==(numprocs-1)) 
	      procnum=0;
	    else
	      procnum++;
	    redraw = true;
	    procplottype=oneproc;
	  }
	} else if(mousebutton==right_button) 
	  if(procplottype==allprocs) {
	    procnum=numprocs-1;
	    procplottype=oneproc;
	    redraw=true;
	  } else if(procnum!=numprocs) { 
	    redraw = true; 
	    procnum=numprocs-1; 
	    procplottype=oneproc; 
	  }
      } else if(report.xany.window==controlButtons[saltwin].butwin) {
	if(mousebutton==left_button) {
	  if(plottype!=salinity) {
	    plottype=salinity;
	    sprintf(message,"Salinity selected...");
	    redraw=true;
	  } else 
	    sprintf(message,"Salinity is already being displayed...");
	} else if(mousebutton==middle_button) {
	  if(plottype!=saldiff) {
	    plottype=saldiff;
	    sprintf(message,"Perturbation salinity selected...");
	    redraw=true;
	  } else 
	    sprintf(message,"Perturbation salinity is already being displayed...");
	} else if(mousebutton==right_button) {
	  if(plottype!=salinity0) {
	    plottype=salinity0;
	    sprintf(message,"Background salinity selected...");
	    redraw=true;
	  } else 
	    sprintf(message,"Background salinity is already being displayed...");
	}
      } else if(report.xany.window==controlButtons[tempwin].butwin) {
	if(mousebutton==left_button) {
	  if(plottype!=temperature) {
	    plottype=temperature;
	    sprintf(message,"Temperature selected...");
	    redraw=true;
	  } else 
	    sprintf(message,"Temperature is already being displayed...");
	}
      } else if(report.xany.window==controlButtons[preswin].butwin) {
	if(mousebutton==left_button) {
	  if(plottype!=pressure) {
	    plottype=pressure;
	    sprintf(message,"Pressure selected...");
	    redraw=true;
	  } else 
	    sprintf(message,"Pressure is already being displayed...");
	}
      } else if(report.xany.window==controlButtons[fswin].butwin && mousebutton==left_button) {
	if(plottype!=freesurface) {
	  plottype=freesurface;
	  sprintf(message,"Free-surface selected...");
	  redraw=true;
	} else
	  sprintf(message,"Free-surface is already being displayed...");
      } else if(report.xany.window==controlButtons[vecwin].butwin) {
	if(vectorplot==false) {
	  vectorplot=true;
	  sprintf(message,"Vectors on...");
	  vlengthfactor=1;
	} else {
	  if(mousebutton==left_button) {
	    vectorplot=false;
	    sprintf(message,"Vectors off...");
	  } else if(mousebutton==middle_button) {
	    vlengthfactor/=2;
	    sprintf(message,"Halving the vector lengths (%.3fX)...",vlengthfactor);
	  } else if(mousebutton==right_button) {
	    vlengthfactor*=2;
	    sprintf(message,"Doubling the vector lengths (%.3fX)...",vlengthfactor);
	  }	
	}    
	redraw=true;
      } else if(report.xany.window==controlButtons[uwin].butwin) {
	if(mousebutton==left_button) {
	  if(plottype!=u_velocity) {
	    plottype=u_velocity;
	    sprintf(message,"U-velocity selected...");
	    redraw=true;
	  } else
	    sprintf(message,"U-velocity is already being displayed...");
	} else if(mousebutton==right_button) {
	  if(plottype!=u_baroclinic) {
	    plottype=u_baroclinic;
	    sprintf(message,"Baroclinic U-velocity selected...");
	    redraw=true;
	  } else
	    sprintf(message,"Baroclinic V-velocity is already being displayed...");
	}
      } else if(report.xany.window==controlButtons[vwin].butwin) {
	if(mousebutton==left_button) {
	  if(plottype!=v_velocity) {
	    plottype=v_velocity;
	    sprintf(message,"V-velocity selected...");
	    redraw=true;
	  } else
	    sprintf(message,"V-velocity is already being displayed...");
	} else if(mousebutton==right_button) {
	  if(plottype!=v_baroclinic) {
	    plottype=v_baroclinic;
	    sprintf(message,"Baroclinic V-velocity selected...");
	    redraw=true;
	  } else
	    sprintf(message,"Baroclinic V-velocity is already being displayed...");
	}
      } else if(report.xany.window==controlButtons[nutwin].butwin && mousebutton==left_button) {
	if(plottype!=nut) {
	  plottype=nut;
	  sprintf(message,"Turb. eddy-visc. selected...");
	  redraw=true;
	} else
	  sprintf(message,"Turb. eddy-visc. already being displayed...");
      } else if(report.xany.window==controlButtons[ktwin].butwin && mousebutton==left_button) {
	if(plottype!=kt) {
	  plottype=kt;
	  sprintf(message,"Turb. scalar-diff. selected...");
	  redraw=true;
	} else
	  sprintf(message,"Turb. scalar-diff. already being displayed...");
      } else if(report.xany.window==controlButtons[wwin].butwin && mousebutton==left_button) {
	if(plottype!=w_velocity) {
	  plottype=w_velocity;
	  sprintf(message,"Vertical velocity selected...");
	  redraw=true;
	} else
	  sprintf(message,"Vertical velocity is already being displayed...");
      } else if(report.xany.window==controlButtons[depthwin].butwin && mousebutton==left_button) {
	if(plottype!=depth) {
	  plottype=depth;
	  sprintf(message,"Depth selected...");
	  redraw=true;
	} else
	  sprintf(message,"Depth is already being displayed...");
      } else if(report.xany.window==controlButtons[depthwin].butwin && mousebutton==right_button) {
	if(plottype!=h_d) {
	  plottype=h_d;
	  sprintf(message,"Water height (h+d) selected...");
	  redraw=true;
	} else
	  sprintf(message,"Water height (h+d) is already being displayed...");
      } else if(report.xany.window==controlButtons[edgewin].butwin && mousebutton==left_button) {
	if(edgelines==false) {
	  sprintf(message,"Drawing edge lines");
	  edgelines=true;
	} else {
	  sprintf(message,"Removing edge lines");
	  edgelines=false;
	}
	redraw=true;
      } else if(report.xany.window==controlButtons[nextsediwin].butwin && mousebutton==left_button) {
	data->sedinum=data->sedinum+1;
	if(data->sedinum>data->Nsize)
	  data->sedinum=1;
	plottype=sedic;
	sprintf(message,"Selected sediment size class %d of %d.",data->sedinum,data->Nsize);
	redraw=true;
      } else if(report.xany.window==controlButtons[prevsediwin].butwin && mousebutton==left_button) {
	if(data->sedinum==0)
	  data->sedinum=1;
	else {
	  data->sedinum=data->sedinum-1;
	  if(data->sedinum<1)
	    data->sedinum=data->Nsize;
	}
	plottype=sedic;
	sprintf(message,"Selected sediment size class %d of %d.",data->sedinum,data->Nsize);
	redraw=true;
      } else if(report.xany.window==controlButtons[layerwin].butwin && mousebutton==left_button) {
	if(plottype!=layer) {
	  plottype=layer;
	  sprintf(message,"Total layer thickness selected...");
	  redraw=true;
	} else
	  sprintf(message,"Total layer thickness is already being displayed...");
      }else if(report.xany.window==controlButtons[taubwin].butwin && mousebutton==left_button) {
	if(plottype!=taub) {
	  plottype=taub;
	  sprintf(message,"Bottom shear stress selected...");
	  redraw=true;
	} else
	  sprintf(message,"Bottom shear stress is already being displayed...");
      } else if(report.xany.window==controlButtons[allsediwin].butwin && mousebutton==right_button) {
	if(plottype!=allsedic) {
	  plottype=allsedic;
	  sprintf(message,"Total sediment concentration selected...");
	  redraw=true;
          data->sedinum=0;
	} else
	  sprintf(message,"Total sediment concentration is already being displayed...");
      } else if(report.xany.window==controlButtons[allsediwin].butwin && mousebutton==left_button) {
	if(plottype!=sedic) {
	  plottype=sedic;
	  sprintf(message,"Selected sediment size class %d of %d.",data->sedinum,data->Nsize);
	  redraw=true;
	} else
	  sprintf(message,"Sediment size class %d of %d already being displayed.",data->sedinum,data->Nsize);
      } else if(report.xany.window==controlButtons[voronoiwin].butwin 
		&& mousebutton==left_button) {
	if(voronoipoints==false) {
	  sprintf(message,"Drawing Voronoi points");
	  voronoipoints=true;
	} else {
	  sprintf(message,"Removing Voronoi points");
	  voronoipoints=false;
	}
	redraw=true;
      } else if(report.xany.window==controlButtons[delaunaywin].butwin 
		&& mousebutton==left_button) {
	if(delaunaypoints==false) {
	  sprintf(message,"Drawing Delaunay points");
	  delaunaypoints=true;
	} else {
	  sprintf(message,"Removing Delaunay points");
	  delaunaypoints=false;
	}
	redraw=true;
      } else if(report.xany.window==controlButtons[nonewin].butwin && mousebutton==left_button) {
	if(plottype!=noplottype) {
	  plottype=noplottype;
	  sprintf(message,"Removing surface plot...");
	  redraw=true;
	}
      } else if(report.xany.window==controlButtons[zoomwin].butwin && mousebutton==left_button) {
	if(vertprofile==false) {
	  if(zooming==false) {
	    sprintf(message,"Zooming on...");
	    controlButtons[zoomwin].status=true;
	    zooming=true;
	  } else {
	    sprintf(message,"Zooming off: ready for profile points...");
	    controlButtons[zoomwin].status=false;
	    zooming=false;
	  }
	} else {
	  sprintf(message,"Can't turn zooming off during profile plot...");
	}
      } else if(report.xany.window==controlButtons[profwin].butwin) {
	if(mousebutton==left_button) {
	  if(vertprofile==true) {
	    sprintf(message,"Profile off...");
	    controlButtons[profwin].status=false;
	    vertprofile=false;
	    edgelines=false;
	  }
	  else {
	    if(data->Nslice==0) 
	      sprintf(message,"Need two points for a profile first!");
	    else {
	      sprintf(message,"Profile on...");
	      controlButtons[profwin].status=true;
	      vertprofile=true;
	      sliceType=slice;
	      edgelines=false;
	    }
	  }
	} else if(mousebutton==middle_button) {
	  sprintf(message,"Profile from left to right...");
	  xstart=0;
	  ystart=height/2*axesPosition[3];
	  xend=width*axesPosition[2];
	  yend=height/2*axesPosition[3];
	  controlButtons[profwin].status=true;
	  vertprofile=true;
	  sliceType=slice;
	  edgelines=false;
	  fromprofile=true;
	} else if(mousebutton==right_button) {
	  sprintf(message,"Profile from top to bottom...");
	  xstart=width/2*axesPosition[2];
	  ystart=height*axesPosition[3];
	  xend=width/2*axesPosition[2];
	  yend=0;
	  controlButtons[profwin].status=true;
	  vertprofile=true;
	  sliceType=slice;
	  edgelines=false;
	  fromprofile=true;
	}
	setdatalimits=false;
	setdatalimitsslice=false;
	redraw=true;
      } else if(report.xany.window==controlButtons[axisimagewin].butwin 
		&& mousebutton==left_button) {
	if(axisType=='i') {
	  axisType='n';
	  sprintf(message,"Changing axis type to normal...");
	} else {
	  axisType='i';
	  sprintf(message,"Changing axis type to image...");
	}
	redraw=true;
      } else if(report.xany.window==controlButtons[cmapholdwin].butwin) {
	if(mousebutton==left_button) {
	  if(cmaphold==false) {
	    cmaphold=true;
	    sprintf(message,"Color axes will be scaled with parameters in suntans.dat...");
	  } else {
	    cmaphold=false;
	    sprintf(message,"Scaling color axes with min/max of data...");
	  }
	  getcmap=false;
	} else if(mousebutton==right_button) {
	  GetCAxes(-EMPTY,EMPTY);
	  sprintf(message,"Color axes changed to input values...");
	  raisewindow=true;
	  getcmap=true;
	}
	redraw=true;
      } else if(report.xany.window==controlButtons[reloadwin].butwin && mousebutton==left_button) {
	ReadData(data,-1,numprocs);
	sprintf(message,"Reloading data...");
	fromprofile=true;
	redraw=true;
      } else if(report.xany.window==controlButtons[quitwin].butwin && mousebutton==left_button) {
	quit=true;
      } else if(report.xany.window==controlButtons[iskip_minus_win].butwin) {
	if(mousebutton==left_button) {
	  if(iskip>1) {
	    redraw=true;
	    iskip--;
	  } else 
	    sprintf(message,"Already at iskip=1!");
	} else if(mousebutton==right_button) {
	  if(iskip>1) {
	    redraw=true;
	    iskip=1;
	  } else
	    sprintf(message,"Already at iskip=1!");
	}
      } else if(report.xany.window==controlButtons[iskip_plus_win].butwin) {
	if(vertprofile==true) {
	  if(mousebutton==left_button) {
	    if(iskip<data->Nslice-1) {
	      redraw=true;
	      iskip++;
	    } else 
	      sprintf(message,"Already at iskip=%d!",data->Nslice-1);
	  } else if(mousebutton==right_button) {
	    if(iskip<data->Nslice-1) {
	      redraw=true;
	      iskip=data->Nslice-1;
	    } else
	      sprintf(message,"Already at iskip=%d!",data->Nslice-1);
	  }
	} else {
	  if(mousebutton==left_button) {
	    if(iskip<iskipmax) {
	      redraw=true;
	      iskip++;
	    } else 
	      sprintf(message,"Already at iskip=iskipmax (%d)!",iskipmax);
	  } else if(mousebutton==right_button) {
	    if(iskip<iskipmax) {
	      redraw=true;
	      iskip=iskipmax;
	    } else
	      sprintf(message,"Already at iskip=iskipmax!",iskipmax);
	  }
	}
      } else if(report.xany.window==controlButtons[kskip_minus_win].butwin) {
	if(vertprofile==true) {
	  if(mousebutton==left_button) {
	    if(kskip>1) {
	      redraw=true;
	      kskip--;
	    } else 
	      sprintf(message,"Already at kskip=1!");
	  } else if(mousebutton==right_button) {
	    if(kskip>1) {
	      redraw=true;
	      kskip=1;
	    } else
	      sprintf(message,"Already at kskip=1!");
	  }
	} else
	  sprintf(message,"Need to be viewing a profile to change kskip...");
      } else if(report.xany.window==controlButtons[kskip_plus_win].butwin) {
	if(vertprofile==true) {
	  if(mousebutton==left_button) {
	    if(kskip<data->Nkmax-1) {
	      redraw=true;
	      kskip++;
	    } else 
	      sprintf(message,"Already at kskip=%d!",data->Nkmax-1);
	  } else if(mousebutton==right_button) {
	    if(kskip<data->Nkmax-1) {
	      redraw=true;
	      kskip=data->Nkmax-1;
	    } else
	      sprintf(message,"Already at kskip=%d!",data->Nkmax-1);
	  }
	} else
	  sprintf(message,"Need to be viewing a profile to change kskip...");
      } else if(report.xany.window==axeswin) {
	xend=report.xbutton.x;
	yend=report.xbutton.y;
	if(zooming==true) {
	  if(report.xbutton.button==left_button) {
	    if(panning)
        zoom=pan;
      else if(xend==xstart && yend==ystart) 
	      zoom=in;
      else 
	      zoom=box;
	    redraw=true;
	  } else if(report.xbutton.button==right_button) {
	    zoom=out;
	    redraw=true;
	  } else if(report.xbutton.button==middle_button) {
      if(xend==xstart && yend==ystart){
        zoom=none;
        zoomratio=1;
        setdatalimits=false;
        setdatalimitsslice=false;
      }
      else{ // panning
        DrawArrow(xstart,ystart,(xend-xstart),-(yend-ystart),axeswin,white);
        zoom=pan;
      }
      redraw=true;
	  }
	} else {
	  if(report.xbutton.button==left_button) {
	    sliceType=slice;
	    zooming=true;
	    controlButtons[zoomwin].status=true;
	    edgelines=false;
	  } else if(report.xbutton.button==middle_button) {
	    sliceType=vertp;
	  } else if(report.xbutton.button==right_button) {
	    sliceType=value;
	  }
	  redraw=true;
	  fromprofile=true;
	}
      }
      pressed=false;
      break;
    case Expose:   
      XGetWindowAttributes(dis,win,&windowAttributes);
      newwidth=windowAttributes.width;
      newheight=windowAttributes.height;
      if(newwidth!=width || newheight!=height) {
	width=newwidth;
	height=newheight;
	XResizeWindow(dis,win,width,height);
	XFreePixmap(dis,pix);
	pix = XCreatePixmap(dis,axeswin,axesPosition[2]*width,
			    axesPosition[3]*height,DefaultDepthOfScreen(screen));
      }
      RedrawWindows();
      // Set the following line to redraw=false; if plotting huge datasets
      LoopDraw(data,plottype,procnum,numprocs);
      break;
    case KeyPress:
      switch(keysym=XLookupKeysym(&report.xkey, 0)) {
        case XK_m:
          panning=!panning;
          if(panning)
            sprintf(message,"Panning enabled, zoom to box disabled...");
          else
            sprintf(message,"Panning disabled, zoom to box enabled...");
          break;
      case XK_q:
	quit=true;
	break;
      case XK_k: case XK_Up:
	if(k<data->Nkmax-1) { redraw = true; k++; }
	  else { redraw = false; sprintf(message,"At k=Nkmax!"); }
	break;
      case XK_j: case XK_Down:
	if(k>0) { redraw = true; k--; }
	else { redraw = false ; sprintf(message,"At k=1!"); }
	break;
      case XK_p : case XK_Left:
	if(n>1) { redraw = true; n--; }
	else { redraw = false ; sprintf(message,"At n=1!"); }
	break;
      case XK_n: case XK_Right:
	if(n<data->noutput) { redraw = true; n++; }
	else { redraw = false ; sprintf(message,"At n=noutput!"); }
	break;
      case XK_s:
	if(plottype!=salinity) {
	  plottype=salinity;
	  sprintf(message,"Salinity selected...");
	  redraw=true;
	}
	break;
      case XK_h:
	if(plottype!=freesurface) {
	  plottype=freesurface;
	  sprintf(message,"Free-surface selected...");
	  redraw=true;
	}
	break;
      case XK_d:
	if(plottype!=depth) {
	  plottype=depth;
	  sprintf(message,"Depth selected...");
	  redraw=true;
	}
	break;
      case XK_a:
	if(axisType=='n') {
	  sprintf(message,"Changing plottype to image");
	  axisType='i';
	} else {
	  sprintf(message,"Changing plottype to normal");
	  axisType='n';
	}
	redraw=true;
	break;
      case XK_e:
	if(edgelines==0) {
	  sprintf(message,"Drawing edge lines");
	  edgelines=1;
	} else {
	  sprintf(message,"Removing edge lines");
	  edgelines=0;
	}
	redraw=true;
	break;
      case XK_l:
  // we want to specify a location
  sliceType=value_keyboard;
  printf("Must specify the value via the keyboard\n");
  GetLoc(data, procnum, numprocs);
  redraw=true;

  break;
//      case XK_x:
//  // print location of cursor to console
//  printf("Cursor location (%f,%f)\n",
//      dataLimits[0]+(float)xend*(dataLimits[1]-dataLimits[0])
//      /(axesPosition[2]*(float)width),
//      dataLimits[2]+(float)(height*axesPosition[3]-yend)
//      *(dataLimits[3]-dataLimits[2])/(axesPosition[3]*(float)height));
//  break;
      case XK_0: case XK_1: case XK_2: case XK_3: case XK_4:
      case XK_5: case XK_6: case XK_7: case XK_8: case XK_9:
	str[0]=keysym;
	str[1]='\0';
	if(procnum!=atoi(str)) {
	  procnum=atoi(str);
	  redraw=true;
	}
	break;
      }
    }
    }
    if(quit)
      break;
    if(redraw) {
      LoopDraw(data,plottype,procnum,numprocs);
    }
    ShowMessage();
    if(raisewindow)
      XRaiseWindow(dis,win);
  }
  FreeData(data,numprocs);
  CloseGraphics();
}

void ShowMessage(void) {
  int font_height;
  font_height=fontStruct->ascent+fontStruct->descent;

  XSetForeground(dis,gc,black);
  XFillRectangle(dis,messagewin,gc,
		 0,0,width*axesPosition[2],height*buttonAxesPosition[1]);

  XSetForeground(dis,fontgc,white);
  XDrawString(dis,messagewin,fontgc,0,font_height,
	      message,strlen(message));  
}

void LoopDraw(dataT *data, plottypeT plottype, int procnum, int numprocs) {
  float umagmax;
  int iloc, procloc, proc;
  // read all the data into *data
  ReadData(data,n,numprocs);

  if(sliceType==value_keyboard) {
    // just use the previously selected values from the keyboard
    iloc = keyboardcell;
    procloc = keyboardproc;
  }

  if(sliceType==value) 
      FindNearest(data,xend,yend,&iloc,&procloc,procnum,numprocs);
  
  if(sliceType==slice) 
    GetSlice(data,xstart,ystart,xend,yend,procnum,numprocs,plottype);

  if(plottype!=noplottype && !getcmap) {
    if(cmaphold==false)
      CAxis(data,plottype,k,procnum,numprocs);
    else
      if(!LoadCAxis())
	CAxis(data,plottype,k,procnum,numprocs);
  }

  if(vectorplot) 
    GetUMagMax(data,k,numprocs);

  if(procplottype==allprocs && !vertprofile)
    for(proc=0;proc<numprocs;proc++) {
      if(proc==0) {
	SetDataLimits(data);
	SetAxesPosition();
	Cla();
      }
      printf("Plotting proc %d of %d\n",proc+1,numprocs);
      MyDraw(data,plottype,proc,numprocs,iloc,procloc);
    }
  else {
    SetDataLimits(data);
    SetAxesPosition();
    Cla();
    MyDraw(data,plottype,procnum,numprocs,iloc,procloc);
  }

  if(vectorplot) {
    if(vertprofile && sliceType==slice) 
      Quiver(data->sliceX,data->z,data->sliceD,data->sliceH,data->sliceU,data->sliceW,
	     data->umagmax,data->Nkmax,data->Nslice);
    else {
      if(procplottype==allprocs) 
	for(proc=0;proc<numprocs;proc++) 
	  UnQuiver(data->edges[proc],data->xc,data->yc,data->xv[proc],data->yv[proc],
		   data->u[proc][k],data->v[proc][k],
		   data->umagmax,data->Ne[proc],data->Nc[proc]);
      else 
	UnQuiver(data->edges[procnum],data->xc,data->yc,data->xv[procnum],data->yv[procnum],
		 data->u[procnum][k],data->v[procnum][k],
		 data->umagmax,data->Ne[procnum],data->Nc[procnum]);
    }
  }

  XFlush(dis);
  XCopyArea(dis,pix,axeswin,gc,0,0,
	    axesPosition[2]*width,
	    axesPosition[3]*height,0,0);
  XCopyArea(dis,controlspix,controlswin,gc,0,0,
	    buttonAxesPosition[2]*width,
	    buttonAxesPosition[3]*height,0,0);
  XCopyArea(dis,cmappix,cmapwin,gc,0,0,
	    cmapAxesPosition[2]*width,
	    cmapAxesPosition[3]*height,0,0);
}

void MyDraw(dataT *data, plottypeT plottype, int procnum, int numprocs, int iloc, int procloc)
{
  int i;
  char tmpstr[BUFFERLENGTH];
  float *scal = GetScalarPointer(data,plottype,k,procnum);

  if(zooming==false && (sliceType==value || sliceType==value_keyboard) && procnum==procloc)
    if(plottype==freesurface || 
       plottype==depth || 
       plottype==h_d) {
      if(scal[iloc]==EMPTY)
	sprintf(message,"Proc: %d, value(i=%d)=EMPTY (x,y)=(%.2f,%.2f)",
		procnum,iloc,data->xv[procnum][iloc],data->yv[procnum][iloc]);
      else
	sprintf(message,"Proc: %d, value(i=%d)=%.2e (x,y)=(%.2f,%.2f)",
		procnum,iloc,scal[iloc],data->xv[procnum][iloc],data->yv[procnum][iloc]);
    } else {
      if(scal[iloc]==EMPTY)
	sprintf(message,"Proc: %d, value(i=%d,k=%d)=EMPTY (x,y)=(%.2f,%.2f)",
		procnum,iloc,k,data->xv[procnum][iloc],
		data->yv[procnum][iloc]);
      else
	sprintf(message,"Proc: %d, value(i=%d,k=%d)=%.2e (x,y)=(%.2f,%.2f)",
		procnum,iloc,k,scal[iloc],data->xv[procnum][iloc],
		data->yv[procnum][iloc]);
    }

  if(vertprofile==true && sliceType==slice) 
    QuadContour(data->sliceH,data->sliceD,
	     data->sliceData,data->sliceX,data->z,data->Nslice,data->Nkmax,plottype);
    //    QuadSurf(data->sliceH,data->sliceD,
    //	     data->sliceData,data->sliceX,data->z,data->Nslice,data->Nkmax,plottype);
  else {
    if(plottype!=noplottype)
      UnSurf(data->xc,data->yc,data->cells[procnum],scal,data->Nc[procnum],data->maxfaces,data->nfaces[procnum]);
    
    if(delaunaypoints)
      DrawDelaunayPoints(data->xc,data->yc,plottype,data->Np);
    
    if(voronoipoints) 
      DrawVoronoiPoints(data->xv[procnum],data->yv[procnum],data->Nc[procnum]);
    
    if(edgelines)
      DrawEdgeLines(data->xc,data->yc,data->cells[procnum],plottype,data->Nc[procnum],data->maxfaces,data->nfaces[procnum]);

    if(boundaries)
      DrawBoundaries(data->xc,data->yc,data->edges[procnum],plottype,data->mark[procnum],data->Ne[procnum]);
  }
  DrawControls(data,procnum,numprocs);
  DrawColorBar(data,procnum,numprocs,plottype);
}

void QuadSurf(float *h, float *D, 
	      float **data, float *x, float *z, int N, int Nk,plottypeT plottype) {
  int i, j, xp1, xp2, yp1, yp2, w, ind;
  float xs, xe, zs, dataval;
  XPoint points[5];
    
  w = 2*axesPosition[2]*width*(x[1]-x[0])/
    (dataLimits[1]-dataLimits[0]);
  xp1 = axesPosition[2]*width*(x[0]-dataLimits[0])/
    (dataLimits[1]-dataLimits[0])-w/2;

  for(i=0;i<N;i++) {
    if(i==N-1)
      xp2 = axesPosition[2]*width*(x[i]+0.5*(x[i]-x[i-1])-dataLimits[0])/
	(dataLimits[1]-dataLimits[0]);
    else
      xp2 = axesPosition[2]*width*(x[i]+0.5*(x[i+1]-x[i])-dataLimits[0])/
	(dataLimits[1]-dataLimits[0]);

    points[0].x = xp1;
    points[1].x = xp2;
    points[2].x = xp2;
    points[3].x = xp1;
    points[4].x = xp1;

    xp1 = xp2;

    if(plottype!=freesurface) {
      for(j=0;j<Nk;j++) {
	yp1 = axesPosition[3]*height*(1-(z[j+1]-dataLimits[2])/
				      (dataLimits[3]-dataLimits[2]));
	yp2 = axesPosition[3]*height*(1-(z[j]-dataLimits[2])/
				      (dataLimits[3]-dataLimits[2]));

	if(((z[j]<=h[i] && z[j+1]<=h[i]) || (z[j]>=h[i] && z[j+1]<=h[i])) &&
	   ((z[j]>=-D[i] && z[j+1]>=-D[i]) || (z[j]>=-D[i] && z[j+1]<=-D[i]))) {
	  if(z[j]<h[i] && j==0) {
	    yp2 = axesPosition[3]*height*(1-(h[i]-dataLimits[2])/
					  (dataLimits[3]-dataLimits[2]));
	    yp1 = axesPosition[3]*height*(1-(z[j+1]-dataLimits[2])/
					  (dataLimits[3]-dataLimits[2]));
	  }
	  if(z[j]>=h[i] && z[j+1]<=h[i])
	    yp2 = axesPosition[3]*height*(1-(h[i]-dataLimits[2])/
					  (dataLimits[3]-dataLimits[2]));
	  if(z[j]>=-D[i] && z[j+1]<=-D[i])
	    yp1 = axesPosition[3]*height*(1-(-D[i]-dataLimits[2])/
					  (dataLimits[3]-dataLimits[2]));
	  
	  points[0].y = yp1;
	  points[1].y = yp1;
	  points[2].y = yp2;
	  points[3].y = yp2;
	  points[4].y = yp1;
	  
	  dataval = data[i][j];
	  ind = (dataval-caxis[0])/(caxis[1]-caxis[0])*(NUMCOLORS-3);
	  
	  if(dataval>caxis[1])
	    ind = NUMCOLORS-3;
	  if(dataval<caxis[0])
	    ind = 0;
	  
	  if(dataval==EMPTY || (plottype=='D' && dataval==0))
	    ind = NUMCOLORS-1;
	  
	  if(plottype!=noplottype) {
	    XSetForeground(dis,gc,colors[ind]);
	    XFillPolygon(dis,pix,gc,points,5,Convex,CoordModeOrigin);
	  }
	  
	  if(edgelines && dataval!=EMPTY) {
	    if(plottype==noplottype)
	      XSetForeground(dis,gc,white);
	    else
	      XSetForeground(dis,gc,black);
	    XDrawLines(dis,pix,gc,points,5,0);
	  }
	}
      }
      // Draw the bottom
      yp2 = axesPosition[3]*height*(1-(-D[i]-dataLimits[2])/
				    (dataLimits[3]-dataLimits[2]));    
      if(i>0)
	yp1 = axesPosition[3]*height*(1-(-D[i-1]-dataLimits[2])/
				      (dataLimits[3]-dataLimits[2]));    
      else
	yp1 = yp2;

      XSetForeground(dis,gc,white);
      XDrawLine(dis,pix,gc,points[0].x,yp1,points[0].x,yp2);
      XDrawLine(dis,pix,gc,points[0].x,yp2,points[1].x,yp2);
    }

    // Draw the free-surface
    yp2 = axesPosition[3]*height*(1-(h[i]-dataLimits[2])/
				  (dataLimits[3]-dataLimits[2]));    
    if(i>0)
      yp1 = axesPosition[3]*height*(1-(h[i-1]-dataLimits[2])/
				    (dataLimits[3]-dataLimits[2]));    
    else 
      yp1 = yp2;

    XSetForeground(dis,gc,white);
    XDrawLine(dis,pix,gc,points[0].x,yp1,points[0].x,yp2);
    XDrawLine(dis,pix,gc,points[0].x,yp2,points[1].x,yp2);
  }
  /*
    for(i=0;i<Nc;i++) {
      yp2 = axesPosition[3]*height*(1-(0-dataLimits[2])/
				    (dataLimits[3]-dataLimits[2]));

      if(yp1!=yp2) {
	points[0].y = yp1;
	points[1].y = yp1;
	points[2].y = yp2;
	points[3].y = yp2;
	points[4].y = yp1;
	
	XSetForeground(dis,gc,black);
	XFillPolygon(dis,pix,gc,points,5,Convex,CoordModeOrigin);
      }
      if(edgelines && plottype==noplottype) {
	XSetForeground(dis,gc,white);
	XDrawLine(dis,pix,gc,points[0].x,yp1,points[1].x,yp1);
      }
    }
    */
}

void QuadContour(float *h, float *D, 
	      float **data, float *x, float *z, int N, int Nk,plottypeT plottype) {
  int i, j, xp1, xp2, yp1, yp2, w, ind;
  float xs, xe, zs, dataval;
  XPoint points[5];
    
  w = 2*axesPosition[2]*width*(x[1]-x[0])/
    (dataLimits[1]-dataLimits[0]);
  xp1 = axesPosition[2]*width*(x[0]-dataLimits[0])/
    (dataLimits[1]-dataLimits[0])-w/2;

  for(i=0;i<N;i++) {
    if(i==N-1)
      xp2 = axesPosition[2]*width*(x[i]+0.5*(x[i]-x[i-1])-dataLimits[0])/
	(dataLimits[1]-dataLimits[0]);
    else
      xp2 = axesPosition[2]*width*(x[i]+0.5*(x[i+1]-x[i])-dataLimits[0])/
	(dataLimits[1]-dataLimits[0]);

    points[0].x = xp1;
    points[1].x = xp2;
    points[2].x = xp2;
    points[3].x = xp1;
    points[4].x = xp1;

    xp1 = xp2;

    if(plottype!=freesurface) {
      for(j=0;j<Nk;j++) {
	yp1 = axesPosition[3]*height*(1-(z[j+1]-dataLimits[2])/
				      (dataLimits[3]-dataLimits[2]));
	yp2 = axesPosition[3]*height*(1-(z[j]-dataLimits[2])/
				      (dataLimits[3]-dataLimits[2]));

	if(((z[j]<=h[i] && z[j+1]<=h[i]) || (z[j]>=h[i] && z[j+1]<=h[i])) &&
	   ((z[j]>=-D[i] && z[j+1]>=-D[i]) || (z[j]>=-D[i] && z[j+1]<=-D[i]))) {
	  if(z[j]<h[i] && j==0) {
	    yp2 = axesPosition[3]*height*(1-(h[i]-dataLimits[2])/
					  (dataLimits[3]-dataLimits[2]));
	    yp1 = axesPosition[3]*height*(1-(z[j+1]-dataLimits[2])/
					  (dataLimits[3]-dataLimits[2]));
	  }
	  if(z[j]>=h[i] && z[j+1]<=h[i])
	    yp2 = axesPosition[3]*height*(1-(h[i]-dataLimits[2])/
					  (dataLimits[3]-dataLimits[2]));
	  if(z[j]>=-D[i] && z[j+1]<=-D[i])
	    yp1 = axesPosition[3]*height*(1-(-D[i]-dataLimits[2])/
					  (dataLimits[3]-dataLimits[2]));
	  
	  points[0].y = yp1;
	  points[1].y = yp1;
	  points[2].y = yp2;
	  points[3].y = yp2;
	  points[4].y = yp1;
	  
	  dataval = data[i][j];
	  ind = (dataval-caxis[0])/(caxis[1]-caxis[0])*(NUMCOLORS-3);
	  
	  if(dataval>caxis[1])
	    ind = NUMCOLORS-3;
	  if(dataval<caxis[0])
	    ind = 0;
	  
	  if(dataval==EMPTY || (plottype=='D' && dataval==0))
	    ind = NUMCOLORS-1;
	  
	  if(plottype!=noplottype) {
	    XSetForeground(dis,gc,colors[ind]);
	    XFillPolygon(dis,pix,gc,points,5,Convex,CoordModeOrigin);
	  }
	  
	  if(edgelines && dataval!=EMPTY) {
	    if(plottype==noplottype)
	      XSetForeground(dis,gc,white);
	    else
	      XSetForeground(dis,gc,black);
	    XDrawLines(dis,pix,gc,points,5,0);
	  }
	}
      }
      // Draw the bottom
      yp2 = axesPosition[3]*height*(1-(-D[i]-dataLimits[2])/
				    (dataLimits[3]-dataLimits[2]));    
      if(i>0)
	yp1 = axesPosition[3]*height*(1-(-D[i-1]-dataLimits[2])/
				      (dataLimits[3]-dataLimits[2]));    
      else
	yp1 = yp2;

      XSetForeground(dis,gc,white);
      XDrawLine(dis,pix,gc,points[0].x,yp1,points[0].x,yp2);
      XDrawLine(dis,pix,gc,points[0].x,yp2,points[1].x,yp2);
    }

    // Draw the free-surface
    yp2 = axesPosition[3]*height*(1-(h[i]-dataLimits[2])/
				  (dataLimits[3]-dataLimits[2]));    
    if(i>0)
      yp1 = axesPosition[3]*height*(1-(h[i-1]-dataLimits[2])/
				    (dataLimits[3]-dataLimits[2]));    
    else 
      yp1 = yp2;

    XSetForeground(dis,gc,white);
    XDrawLine(dis,pix,gc,points[0].x,yp1,points[0].x,yp2);
    XDrawLine(dis,pix,gc,points[0].x,yp2,points[1].x,yp2);
  }
  /*
    for(i=0;i<Nc;i++) {
      yp2 = axesPosition[3]*height*(1-(0-dataLimits[2])/
				    (dataLimits[3]-dataLimits[2]));

      if(yp1!=yp2) {
	points[0].y = yp1;
	points[1].y = yp1;
	points[2].y = yp2;
	points[3].y = yp2;
	points[4].y = yp1;
	
	XSetForeground(dis,gc,black);
	XFillPolygon(dis,pix,gc,points,5,Convex,CoordModeOrigin);
      }
      if(edgelines && plottype==noplottype) {
	XSetForeground(dis,gc,white);
	XDrawLine(dis,pix,gc,points[0].x,yp1,points[1].x,yp1);
      }
    }
    */
}

void UnQuiver(int *edges, float *xc, float *yc, float *xv, float *yv, 
	      float *u, float *v, float umagmax, int Ne, int Nc) {
  int j, ic;
  float xe, ye, umag, l, lmax, n1, n2;
  int xp, yp, ue, ve, vlength;

  if(plottype==noplottype)
    ic = white;
  else
    ic = black;

  lmax = 0;
  for(j=0;j<Ne;j++) {
    l=sqrt(pow(xc[edges[2*j]]-xc[edges[2*j+1]],2)+
	   pow(yc[edges[2*j]]-yc[edges[2*j+1]],2));
    if(l>lmax) lmax=l;
  }

  vlength = vlengthfactor*(int)(lmax/(dataLimits[1]-dataLimits[0])*
				axesPosition[2]*width);

  for(j=0;j<Nc;j+=iskip) {
    if(u[j]!=EMPTY) {
      xp = axesPosition[2]*width*(xv[j]-dataLimits[0])/
	(dataLimits[1]-dataLimits[0]);
      yp = 	axesPosition[3]*height*(1-(yv[j]-dataLimits[2])/
					(dataLimits[3]-dataLimits[2]));
      
      umag = sqrt(u[j]*u[j]+v[j]*v[j]);
      n1 = 0;
      if(u[j]!=0)
	n1 = umag/u[j];
      n2 = 0;
      if(v[j]!=0)
	n2 = umag/v[j];
      
      ue = vlength*u[j]/umagmax;
      ve = vlength*v[j]/umagmax;
      
      DrawArrow(xp,yp,ue,ve,pix,ic);
    }
  }
}

void Quiver(float *x, float *z, float *D, float *H, 
	    float **u, float **v, float umagmax, int Nk, int Nc) { 
  int i, j, ic;
  float xe, ye, umag, len, len_max=0, n1, n2;
  int xp, yp, ue, ve, vlength;

  if(plottype==noplottype)
    ic = white;
  else
    ic = black;

  for(i=0;i<Nc-1;i++) {
    len=fabs(x[i+1]-x[i]);
    if(len>=len_max) len_max=len;
  }
  vlength = vlengthfactor*(int)(len_max/(dataLimits[1]-dataLimits[0])*
		  axesPosition[2]*width);

  for(i=0;i<Nc;i+=iskip) 
     for(j=0;j<Nk;j+=kskip) {
       if(u[i][j]!=EMPTY) {
	 xp = axesPosition[2]*width*(x[i]-dataLimits[0])/
	   (dataLimits[1]-dataLimits[0]);
	 yp = 	axesPosition[3]*height*(1-(0.5*(z[j]+z[j+1])-dataLimits[2])/
					(dataLimits[3]-dataLimits[2]));

	 umag = sqrt(u[i][j]*u[i][j]+v[i][j]*v[i][j]);
	 n1 = 0;
	 if(u[i][j]!=0)
	   n1 = umag/u[i][j];
	 n2 = 0;
	 if(v[i][j]!=0)
	   n2 = umag/v[i][j];
	 
	 ue = vlength*u[i][j]/umagmax;
	 ve = vlength*v[i][j]/umagmax;
	 
	 if(0.5*(z[j]+z[j+1])>=-D[i] && 0.5*(z[j]+z[j+1])<=H[i])
	   DrawArrow(xp,yp,ue,ve,pix,ic);
       }
     }
}


void DrawArrow(int xp, int yp, int ue, int ve, Window mywin, int ic) {
  int i, mag;
  float r=0.25, angle=20, arrowlength=10;
  XPoint points[4];

  mag = (int)sqrt(ue*ue+ve*ve);


  points[0].x = r*mag;
  points[1].x = 0;
  points[2].x = 0;
  points[3].x = r*mag;
  points[0].y = 0;
  points[1].y = -r*mag*sin(angle*M_PI/180);
  points[2].y = r*mag*sin(angle*M_PI/180);
  points[3].y = 0;

  /*
  points[0].x = arrowlength;
  points[1].x = 0;
  points[2].x = 0;
  points[3].x = arrowlength;
  points[0].y = 0;
  points[1].y = -arrowlength*sin(angle*M_PI/180);
  points[2].y = arrowlength*sin(angle*M_PI/180);
  points[3].y = 0;
  */

  Rotate(points,4,ue,-ve,mag);
  for(i=0;i<4;i++) {
    points[i].x+=(xp+(1-r)*ue);
    points[i].y+=(yp-(1-r)*ve);
  }
  
  XSetForeground(dis,gc,ic);
  XDrawLine(dis,mywin,gc,xp,yp,xp+ue,yp-ve);
  XDrawLine(dis,mywin,gc,points[0].x,points[0].y,points[1].x,points[1].y);
  XDrawLine(dis,mywin,gc,points[0].x,points[0].y,points[2].x,points[2].y);
  //XFillPolygon(dis,mywin,gc,points,4,Convex,CoordModeOrigin);
}

void Rotate(XPoint *points, int N, int ue, int ve, int mag) {
  int i, xr, yr;
  float s = (float)ve/(float)mag, c = (float)ue/(float)mag;

  for(i=0;i<N;i++) {
    xr = points[i].x*c + points[i].y*s;
    yr = points[i].x*s - points[i].y*c;
    points[i].x = xr;
    points[i].y = yr;
  }
}    
  
void DrawDelaunayPoints(float *xc, float *yc, plottypeT plottype, int Np) {
  int i, ci, xp, yp;

  if(plottype != noplottype) 
    ci = black;
  else
    ci = white;

  for(i=0;i<Np;i++) {
    xp = axesPosition[2]*width*(xc[i]-dataLimits[0])/
      (dataLimits[1]-dataLimits[0]);
    yp = 	axesPosition[3]*height*(1-(yc[i]-dataLimits[2])/
					(dataLimits[3]-dataLimits[2]));
    FillCircle(xp,yp,POINTSIZE,ci,pix);
  }
}
  
void DrawVoronoiPoints(float *xv, float *yv, int Nc) {
  int xp, yp, i, ic;

  for(i=0;i<Nc;i++) {
    xp = axesPosition[2]*width*(xv[i]-dataLimits[0])/
      (dataLimits[1]-dataLimits[0]);
    yp = 	axesPosition[3]*height*(1-(yv[i]-dataLimits[2])/
					(dataLimits[3]-dataLimits[2]));
    FillCircle(xp,yp,POINTSIZE,red,pix);
    //printf("cell=%d xv=%f yv=%f\n", xv[i],yv[i]);
  }
}

void FillCircle(int xp, int yp, int r, int ic, Window mywin) {
  XSetForeground(dis,gc,ic);
  XFillArc(dis,mywin,gc,xp-r/2,yp-r/2,r,r,0,360*64);
}
  
float Min(float *x, int N) {
  int i;
  float xmin = INFTY;
  for(i=0;i<N;i++)
    if(x[i]<xmin) xmin=x[i];

  return xmin;
}

float Max(float *x, int N) {
  int i;
  float xmax = -INFTY;
  for(i=0;i<N;i++)
    if(x[i]>xmax) xmax=x[i];

  return xmax;
}

void AxisImage(float *axes, float *data) {
  float h, w, dx, dy, densx, densy;

  dx = data[1]-data[0];
  dy = data[3]-data[2];

  if(dy/dx<MINAXESASPECT)
    dy=MINAXESASPECT*dx;

  densx = width*axes[2]/dx;
  densy = height*axes[3]/dy;

  if(densx>densy) {
    w = axes[2]*densy/densx;
    axes[0] = axes[0]+0.5*(axes[2]-w);
    axes[2] = w;
  } else {
    h = axes[3]*densx/densy;
    axes[1] = axes[1]+0.5*(axes[3]-h);
    axes[3] = h;
  }
}
////
void Fill(XPoint *vertices, int N, int cindex, int edges) {
  int i, ic;
  if(plottype!=noplottype) {
    XSetForeground(dis,gc,colors[cindex]);
    XFillPolygon(dis,pix,gc,vertices,(N+1),Convex,CoordModeOrigin);
  }
}

float *GetScalarPointer(dataT *data, plottypeT plottype, int klevel, int proc) {
  float *scal;

  switch(plottype) {
  case freesurface:
    return data->h[proc];
    break;
  case depth:
    return data->depth[proc];
    break;
  case salinity:
    return data->s[proc][klevel];
    break;
  case saldiff:
    return data->sd[proc][klevel];
    break;
  case salinity0:
    return data->s0[proc][klevel];
    break;
  case temperature:
    return data->T[proc][klevel];
    break;
  case pressure:
    return data->q[proc][klevel];
    break;
  case u_velocity: case u_baroclinic:
    return data->u[proc][klevel];
    break;
  case v_velocity: case v_baroclinic:
    return data->v[proc][klevel];
    break;
  case sedic:
    return data->sediC[proc][data->sedinum-1][klevel];
    break;
  case allsedic: 
    return data->allsediC[proc][klevel];
    break;
  case layer:
    return data->layerthickness[proc];
    break;
  case taub:
    return data->taub[proc];
    break;
  case nut:
    return data->nut[proc][klevel];
    break;
  case kt:
    return data->kt[proc][klevel];
    break;
  case w_velocity:
    return data->w[proc][klevel];
    break;
  case h_d:
    return data->h_d[proc];
    break;
  }
  return NULL;
}
  
void CAxis(dataT *data, plottypeT plottype, int klevel, int procnum, int numprocs) {
  int i, ik, is, ie, ni;
  float *scal, val;

  caxis[0] = EMPTY;
  caxis[1] = -EMPTY;

  if(procplottype==allprocs) {
    is=0;
    ie=numprocs;
  } else {
    is=procnum;
    ie=procnum+1;
  }

  if(sliceType==slice && vertprofile) {
    for(i=0;i<data->Nslice;i++) 
      for(ik=0;ik<data->Nkmax;ik++) {
	val=data->sliceData[i][ik]; 
	if(val<=caxis[0] && val!=EMPTY) caxis[0]=val;
	if(val>=caxis[1] && val!=EMPTY) caxis[1]=val;
      }
  } else {
    for(i=is;i<ie;i++) {
      scal = GetScalarPointer(data,plottype,klevel,i);
      for(ni=0;ni<data->Nc[i];ni++) {
	if(scal[ni]<=caxis[0] && scal[ni]!=EMPTY) caxis[0]=scal[ni];
	if(scal[ni]>=caxis[1] && scal[ni]!=EMPTY) caxis[1]=scal[ni];
      }
    }
  }

  if(caxis[0]==caxis[1]) {
    caxis[0]=0;
    caxis[1]=1;
  }
}

void ReadColorMap(char *str) {
  int i;
  float r, g, b, cmap[64][3];

  LOADCMAPARRAY;

  for(i=0;i<NUMCOLORS-2;i++) {
    r = cmap[i][0];
    g = cmap[i][1];
    b = cmap[i][2];

    color.red = r * 0xffff;
    color.green = g * 0xffff;
    color.blue = b * 0xffff;
    XAllocColor(dis,colormap,&color);  
    colors[i] = color.pixel;
  }
  colors[NUMCOLORS-2]=white;
  colors[NUMCOLORS-1]=black;
}

void DrawEdgeLines(float *xc, float *yc, int *cells, plottypeT plottype, int N, int maxfaces, int *nfaces) {
  int i, j, ind;
  XPoint *vertices = (XPoint *)malloc((maxfaces+1)*sizeof(XPoint));    ////////????????????

  for(i=0;i<N;i++) {
    for(j=0;j<nfaces[i];j++) {
      vertices[j].x = 
	axesPosition[2]*width*(xc[cells[maxfaces*i+j]]-dataLimits[0])/
	(dataLimits[1]-dataLimits[0]);
      vertices[j].y = 
	axesPosition[3]*height*(1-(yc[cells[maxfaces*i+j]]-dataLimits[2])/
	(dataLimits[3]-dataLimits[2]));
    }
    
    vertices[nfaces[i]].x = vertices[0].x;
    vertices[nfaces[i]].y = vertices[0].y;
    
 
    if(plottype!=noplottype)
      XSetForeground(dis,gc,black);
    else
      XSetForeground(dis,gc,white);
    XDrawLines(dis,pix,gc,vertices,(nfaces[i]+1),0);
  }
  free(vertices);
}

void UnSurf(float *xc, float *yc, int *cells, float *data, int N, int maxfaces, int *nfaces) {
  int i, j, ind;
  XPoint *vertices = (XPoint *)malloc((maxfaces+1)*sizeof(XPoint));

  for(i=0;i<N;i++) {
    for(j=0;j<nfaces[i];j++) {
      vertices[j].x = 
	axesPosition[2]*width*(xc[cells[maxfaces*i+j]]-dataLimits[0])/
	(dataLimits[1]-dataLimits[0]);
      vertices[j].y = 
	axesPosition[3]*height*(1-(yc[cells[maxfaces*i+j]]-dataLimits[2])/
	(dataLimits[3]-dataLimits[2]));
    }

    vertices[nfaces[i]].x = vertices[0].x;
    vertices[nfaces[i]].y = vertices[0].y;

    ind = (data[i]-caxis[0])/(caxis[1]-caxis[0])*(NUMCOLORS-3);
    if(data[i]==EMPTY || (plottype=='D' && data[i]==0) || ind>=NUMCOLORS)
      ind = NUMCOLORS-1;

    Fill(vertices,nfaces[i],ind,edgelines);
  }
  free(vertices);
}

void InitializeGraphics(void) {
  int screen_number;
  XGCValues fontvalues;
  XColor temp1, temp2;

  dis = XOpenDisplay(NULL);
  screen = ScreenOfDisplay(dis,0);
  screen_number = XScreenNumberOfScreen(screen);

  black = BlackPixel(dis,screen_number);
  white = WhitePixel(dis,screen_number);

  width=XDisplayWidth(dis,screen_number);
  height=XDisplayHeight(dis,screen_number);

  if(WINSCALE) 
    win = XCreateSimpleWindow(dis, RootWindow(dis, 0), 
			      WINLEFT*width,WINTOP*height,
			      WINWIDTH*width,WINHEIGHT*height, 
			      0,black,black);
  else
    win = XCreateSimpleWindow(dis, RootWindow(dis, 0), 
			      0,0,
			      WINWIDTHPIXELS,WINHEIGHTPIXELS,
			      0,black,black);
  width = WINWIDTH*width;
  height = WINHEIGHT*height;

  //  pix = XCreatePixmap(dis,win,width,height,DefaultDepthOfScreen(screen));

  XMapWindow(dis, win);

  colormap = DefaultColormap(dis,screen_number);
  gc = XCreateGC(dis, win,0,0);

  XAllocNamedColor(dis,colormap,"red",&temp1,&temp2);
  red = temp2.pixel;
  XAllocNamedColor(dis,colormap,"blue",&temp1,&temp2);
  blue = temp2.pixel;
  XAllocNamedColor(dis,colormap,"green",&temp1,&temp2);
  green = temp2.pixel;
  XAllocNamedColor(dis,colormap,"yellow",&temp1,&temp2);
  yellow = temp2.pixel;
  
  //  fontStruct = XLoadQueryFont(dis,DEFAULT_FONT);
  int mm, actual;
  char **fontslist = XListFonts(dis,"*",10,&actual);
  fontStruct = XLoadQueryFont(dis,fontslist[4]);
  
  if(!fontStruct) {
    printf("Font \"%s\" does not exist!\n",DEFAULT_FONT);
    exit(0);
  } else {
    fontvalues.background = black;
    fontvalues.font = fontStruct->fid;
    fontvalues.fill_style = FillSolid;
    fontvalues.line_width = 1;
    fontgc = XCreateGC(dis,win, GCForeground | GCBackground |
		       GCFont | GCLineWidth | GCFillStyle, &fontvalues);
  }

  axesPosition[0] = AXESLEFT;
  axesPosition[1] = AXESBOTTOM;
  axesPosition[2] = AXESWIDTH;
  axesPosition[3] = AXESHEIGHT;

  buttonAxesPosition[0]=.1*axesPosition[0];
  buttonAxesPosition[1]=axesPosition[1];
  buttonAxesPosition[2]=.8*axesPosition[0];
  buttonAxesPosition[3]=axesPosition[3];

  cmapAxesPosition[0]=1.1*axesPosition[0]+axesPosition[2];
  cmapAxesPosition[1]=axesPosition[1]+.2*axesPosition[3];
  cmapAxesPosition[2]=.2*axesPosition[2];
  cmapAxesPosition[3]=.6*axesPosition[3];
}

void SetDataLimits(dataT *data) {
  float dx, dy, Xc, Yc, x1, y1, x2, y2, a1, a2, xmin, xmax, ymin, ymax;
  dx = dataLimits[1]-dataLimits[0];
  dy = dataLimits[3]-dataLimits[2];

  if(!setdatalimits && !vertprofile) {
    dataLimits[0] = Min(data->xc,data->Np);
    dataLimits[1] = Max(data->xc,data->Np);
    dataLimits[2] = Min(data->yc,data->Np);
    dataLimits[3] = Max(data->yc,data->Np);
    setdatalimits=true;
  } else if(!setdatalimitsslice && vertprofile && sliceType==slice) {
    if(plottype!=freesurface) {
      dataLimits[0] = 0;
      dataLimits[1] = Max(data->sliceX,data->Nslice);
      dataLimits[2] = -1*GetDmaxSlice(data->sliceD,data->Nslice);//-data->dmax;
      dataLimits[3] = AXESSLICETOP*data->dmax;
      setdatalimitsslice=true;
    } else {
      GetHMax(data,data->numprocs);
      dataLimits[0] = 0;
      dataLimits[1] = Max(data->sliceX,data->Nslice);
      dataLimits[2] = data->hmin;
      dataLimits[3] = data->hmax;
      setdatalimitsslice=true;
    }
    if((dataLimits[1]-dataLimits[0])>AXESBIGRATIO*(dataLimits[3]-dataLimits[2])) {
      Yc=0.5*(dataLimits[3]-dataLimits[2]);
      dataLimits[2]=Yc-0.5*(dataLimits[1]-dataLimits[0])/AXESBIGRATIO;
      dataLimits[3]=Yc+0.5*(dataLimits[1]-dataLimits[0])/AXESBIGRATIO;
    }
  } else {
    switch(zoom) {
    case in:
      if(zoomratio>=MINZOOMRATIO) {
	Xc = dataLimits[0]+(float)xstart/((float)axesPosition[2]*width)*dx;
	Yc = dataLimits[3]-(float)ystart/((float)axesPosition[3]*height)*dy;
	dataLimits[0]=Xc-dx/ZOOMFACTOR/2;
	dataLimits[1]=Xc+dx/ZOOMFACTOR/2;
	dataLimits[2]=Yc-dy/ZOOMFACTOR/2;
	dataLimits[3]=Yc+dy/ZOOMFACTOR/2;
	zoomratio/=2;
      } else 
	printf("Cannot zoom further.  Beyond zoom limits!\n");
      break;
    case out:
      if(zoomratio<=MAXZOOMRATIO) {
	Xc = dataLimits[0]+(float)xstart/((float)axesPosition[2]*width)*dx;
	Yc = dataLimits[3]-(float)ystart/((float)axesPosition[3]*height)*dy;
	dataLimits[0]=Xc-dx*ZOOMFACTOR/2;
	dataLimits[1]=Xc+dx*ZOOMFACTOR/2;
	dataLimits[2]=Yc-dy*ZOOMFACTOR/2;
	dataLimits[3]=Yc+dy*ZOOMFACTOR/2;
	zoomratio*=2;
      } else 
	printf("Cannot zoom further.  Beyond zoom limits!\n");
      break;
    case pan:
      //      if(zoomratio>=MINZOOMRATIO) {
      if(true) {
        dataLimits[0]=dataLimits[0] - (float)(xend-xstart)/((float)axesPosition[2]*width)*dx;
        dataLimits[1]=dataLimits[1] - (float)(xend-xstart)/((float)axesPosition[2]*width)*dx; 
        dataLimits[2]=dataLimits[2] + (float)(yend-ystart)/((float)axesPosition[3]*height)*dy;
        dataLimits[3]=dataLimits[3] + (float)(yend-ystart)/((float)axesPosition[3]*height)*dy;
      } else 
        printf("Too fine a zooming area.  Please try again!\n");
      break;
    case box:
      a1 = dx*dy;
      x1 = dataLimits[0]+(float)xstart/((float)axesPosition[2]*width)*dx;
      x2 = dataLimits[0]+(float)xend/((float)axesPosition[2]*width)*dx;
      y1 = dataLimits[2]+(1-(float)ystart/((float)axesPosition[3]*height))*dy;
      y2 = dataLimits[2]+(1-(float)yend/((float)axesPosition[3]*height))*dy;
      a2 = abs(x2-x1)*abs(y2-y1);
      if(x2>x1) {
	xmin = x1;
	xmax = x2;
      }	else {
	xmin = x2;
	xmax = x1;
      }

      if(y2>y1) {
	ymin = y1;
	ymax = y2;
      }	else {
	ymin = y2;
	ymax = y1;
      }
      zoomratio*=sqrt(a2/a1);

      //      if(zoomratio>=MINZOOMRATIO) {
      if(true) {
	dataLimits[0]=xmin;
	dataLimits[1]=xmax;
	dataLimits[2]=ymin;
	dataLimits[3]=ymax;
      } else 
	printf("Too fine a zooming area.  Please try again!\n");
      break;
    }
  }
}

void SetAxesPosition(void) {
  axesPosition[0] = AXESLEFT;
  axesPosition[1] = AXESBOTTOM;
  axesPosition[2] = AXESWIDTH;
  axesPosition[3] = AXESHEIGHT;

  if(axisType=='i') 
    AxisImage(axesPosition,dataLimits);

  XMoveResizeWindow(dis,axeswin,
		    axesPosition[0]*width,
		    axesPosition[1]*height,
		    axesPosition[2]*width,
		    axesPosition[3]*height);
  XFreePixmap(dis,pix);
  pix = XCreatePixmap(dis,axeswin,axesPosition[2]*width,
		      axesPosition[3]*height,DefaultDepthOfScreen(screen));
}

void Cla(void) {
  XSetForeground(dis,gc,black);
  XFillRectangle(dis,pix,gc,0,0,width*axesPosition[2],height*axesPosition[3]);
}

void Clf(void) {
  XSetForeground(dis,gc,black);
  XFillRectangle(dis,win,gc,0,0,width,height);
}

void Text(Window window, float x, float y, int boxwidth, int boxheight,
	  char *str, int fontsize, int color, 
	  hjustifyT hjustify, vjustifyT vjustify) {

  char **fonts;
  XFontStruct **fontsReturn;
  int i, xf, yf, font_width, font_height, numfonts;

  font_width=XTextWidth(fontStruct,str,strlen(str));
  font_height=fontStruct->ascent+fontStruct->descent;

  switch(hjustify) {
  case left:
    xf=x*boxwidth;
    break;
  case center:
    xf=x*boxwidth-font_width/2;
    break;
  case right:
    xf=x*boxwidth-font_width;
    break;
  default:
    printf("Error!  Unknown horizontal justification type!\n");
    break;
  }

  switch(vjustify) {
  case bottom:
    yf=(1-y)*boxheight;
    break;
  case middle:
    yf=y*boxheight+font_height/2;
    break;
  case top:
    yf=y*boxheight-font_height;
    break;
  default:
    printf("Error!  Unknown vertical justification type!\n");
    break;
  }

  XSetForeground(dis,fontgc,color);
  XDrawString(dis,window,fontgc,xf,yf,str,strlen(str));  
  /*
  fonts = XListFontsWithInfo(dis,"*cour*",200,&numfonts,fontsReturn);
  for(i=0;i<numfonts;i++)
    printf("Found %s\n",fonts[i]);
  exit(0);
  */
}

void RedrawWindows(void) {
  int buttonnum;

  XMoveResizeWindow(dis,controlswin,
		    buttonAxesPosition[0]*width,
		    buttonAxesPosition[1]*height,
		    buttonAxesPosition[2]*width,
		    buttonAxesPosition[3]*height);
  XFreePixmap(dis,controlspix);
  controlspix = XCreatePixmap(dis,controlswin,buttonAxesPosition[2]*width,
			      buttonAxesPosition[3]*height,DefaultDepthOfScreen(screen));

  XMoveResizeWindow(dis,cmapwin,
		    cmapAxesPosition[0]*width,
		    cmapAxesPosition[1]*height,
		    cmapAxesPosition[2]*width,
		    cmapAxesPosition[3]*height);
  XFreePixmap(dis,cmappix);
  cmappix = XCreatePixmap(dis,cmapwin,cmapAxesPosition[2]*width,
			  cmapAxesPosition[3]*height,DefaultDepthOfScreen(screen));

  XMoveResizeWindow(dis,axeswin,
		      axesPosition[0]*width,
		      axesPosition[1]*height,
		      axesPosition[2]*width,
		      axesPosition[3]*height);

  XMoveResizeWindow(dis,messagewin,
		    axesPosition[0]*width,
		    (1.2*buttonAxesPosition[1]+buttonAxesPosition[3])*height,
		    axesPosition[2]*width,
		    buttonAxesPosition[1]*height);

  for(buttonnum=0;buttonnum<NUMBUTTONS;buttonnum++)
    XMoveResizeWindow(dis,controlButtons[buttonnum].butwin,
		      controlButtons[buttonnum].l*buttonAxesPosition[2]*width,
		      controlButtons[buttonnum].b*buttonAxesPosition[2]*height,
		      controlButtons[buttonnum].w*buttonAxesPosition[2]*width,
		      controlButtons[buttonnum].h);
}
  
void MapWindows(void) {
  int buttonnum; 

  controlswin = NewButton(win,"controls",
			  buttonAxesPosition[0]*width,
			  buttonAxesPosition[1]*height,
			  buttonAxesPosition[2]*width,
			  buttonAxesPosition[3]*height,false,white);
  controlspix = XCreatePixmap(dis,controlswin,
			      buttonAxesPosition[2]*width,
			      buttonAxesPosition[3]*height,
			      DefaultDepthOfScreen(screen));
  XMapWindow(dis,controlswin);

  cmapwin = NewButton(win,"cmap",
		      cmapAxesPosition[0]*width,
		      cmapAxesPosition[1]*height,
		      cmapAxesPosition[2]*width,
		      cmapAxesPosition[3]*height,false,black);
  cmappix = XCreatePixmap(dis,cmapwin,
			  cmapAxesPosition[2]*width,
			  cmapAxesPosition[3]*height,
			  DefaultDepthOfScreen(screen));
  XMapWindow(dis,cmapwin);

  axeswin = NewButton(win,"axes",
		      axesPosition[0]*width,
		      axesPosition[1]*height,
		      axesPosition[2]*width,
		      axesPosition[3]*height,true,black);
  pix = XCreatePixmap(dis,axeswin,
		      axesPosition[2]*width,
		      axesPosition[3]*height,DefaultDepthOfScreen(screen));
  XMapWindow(dis,axeswin);

  messagewin = NewButton(win,"message",
			 axesPosition[0]*width,
			 (1.2*buttonAxesPosition[1]+buttonAxesPosition[3])*height,
			 axesPosition[2]*width,
			 buttonAxesPosition[1]*height,false,black);
  XMapWindow(dis,messagewin);

  for(buttonnum=0;buttonnum<NUMBUTTONS;buttonnum++) {
    controlButtons[buttonnum].butwin=NewButton(controlswin,
			      controlButtons[buttonnum].mapstring,
			      controlButtons[buttonnum].l*buttonAxesPosition[2]*width,
			      controlButtons[buttonnum].b*buttonAxesPosition[2]*height,
			      controlButtons[buttonnum].w*buttonAxesPosition[2]*width,
			      controlButtons[buttonnum].h,false,black);
    XMapWindow(dis,controlButtons[buttonnum].butwin);
  }
}

void DrawControls(dataT *data, int procnum, int numprocs) {
  int buttonnum, bcolor;
  XPoint *vertices = (XPoint *)malloc(5*sizeof(XPoint));
  
  XSetForeground(dis,gc,black);
  XFillRectangle(dis,controlspix,gc,
		 0,0,
		 buttonAxesPosition[2]*width,
		 buttonAxesPosition[3]*height);

  XSetForeground(dis,gc,white);
  XDrawRectangle(dis,controlspix,gc,0,0,
		 buttonAxesPosition[2]*width,
		 buttonAxesPosition[3]*height);

  for(buttonnum=0;buttonnum<NUMBUTTONS;buttonnum++) {
    if(controlButtons[buttonnum].status)
      bcolor=red;
    else
      bcolor=white;
    bcolor=white;
    DrawButton(controlButtons[buttonnum].butwin,controlButtons[buttonnum].string,bcolor);
  }

  sprintf(str,"Step: %d of %d (max %d)",n,data->noutput,data->nsteps);
  DrawHeader(controlButtons[prevwin].butwin,controlButtons[nextwin].butwin,str);

  sprintf(str,"Level: %d of %d",k+1,data->Nkmax);
  DrawHeader(controlButtons[kdownwin].butwin,controlButtons[kupwin].butwin,str);

  if(procplottype==allprocs || numprocs==1)
    sprintf(str,"Showing All %d Procs", numprocs);
  else
    sprintf(str,"Processor: %d of %d",procnum+1,numprocs);

  DrawHeader(controlButtons[prevprocwin].butwin,controlButtons[nextprocwin].butwin,str);

  sprintf(str,"iskip: %d",iskip);
  DrawHeader(controlButtons[iskip_minus_win].butwin,controlButtons[iskip_plus_win].butwin,str);

  sprintf(str,"kskip: %d",kskip);
  DrawHeader(controlButtons[kskip_minus_win].butwin,controlButtons[kskip_plus_win].butwin,str);

  if(axisType=='n')
    sprintf(str,"Normal");
  else
    sprintf(str,"Image");
  DrawHeader(controlButtons[axisimagewin].butwin,controlButtons[axisimagewin].butwin,str);

  if(cmaphold)
    sprintf(str,"Fixed");
  else
    sprintf(str,"Unfixed");
  DrawHeader(controlButtons[cmapholdwin].butwin,controlButtons[cmapholdwin].butwin,str);
}

void DrawColorBar(dataT *data, int procnum, int numprocs, plottypeT plottype) {
  int n, h, font_width, font_height, headsep=5, offset=0, spc=5;
  char str[BUFFERLENGTH];
  XPoint *vertices = (XPoint *)malloc(5*sizeof(XPoint));

  font_height=fontStruct->ascent+fontStruct->descent;

  XSetForeground(dis,gc,black);
  XFillRectangle(dis,cmappix,gc,
		 0,0,
		 cmapAxesPosition[2]*width,
		 cmapAxesPosition[3]*height);

  if(plottype!=noplottype) {
    h = 1+(int)(cmapAxesPosition[3]*(float)height/(float)(NUMCOLORS-2-2*offset));
    for(n=0;n<NUMCOLORS-2;n++) {
      XSetForeground(dis,gc,colors[n]);
      XFillRectangle(dis,cmappix,gc,0,cmapAxesPosition[3]*height-n*h,
		     cmapAxesPosition[2]*width/8,h);
    }
    XSetForeground(dis,fontgc,white);  
    sprintf(str,"%.2e",caxis[0]);
    font_width=XTextWidth(fontStruct,str,strlen(str));
    XDrawString(dis,cmappix,
		fontgc,cmapAxesPosition[2]*width/8,cmapAxesPosition[3]*height,str,strlen(str));  
    sprintf(str,"%.2e",caxis[1]);
    font_width=XTextWidth(fontStruct,str,strlen(str));
    XDrawString(dis,cmappix,
		fontgc,cmapAxesPosition[2]*width/8,font_height,str,strlen(str));  
    
    switch(plottype) {
    case u_velocity: 
      sprintf(str,"U");
      break;
    case u_baroclinic: 
      sprintf(str,"U baro");
      break;
    case v_velocity: 
      sprintf(str,"V");
      break;
    case nut:
      sprintf(str,"log10(nu)");
      break;
    case kt:
      sprintf(str,"log10(k)");
      break;
    case v_baroclinic: 
      sprintf(str,"V baro");
      break;
    case w_velocity: 
      sprintf(str,"W");
      break;
    case freesurface: 
      sprintf(str,"h");
      break;
    case depth:
      sprintf(str,"d");
      break;
    case sedic:
      sprintf(str,"SediC%d",data->sedinum);
      break;
    case allsedic:
      sprintf(str,"all sediC");
      break;   
    case layer:
      sprintf(str,"Layerthick");
      break; 
    case taub:
      sprintf(str,"taub");
      break;
    case h_d: 
      sprintf(str,"h+d");
      break;
    case salinity: 
      sprintf(str,"s");
      break;
    case saldiff: 
      sprintf(str,"s-s0");
      break;
    case salinity0: 
      sprintf(str,"s0");
      break;
    case temperature: 
      sprintf(str,"T");
      break;
    case pressure:
      sprintf(str,"q");
      break;
    default:
      sprintf(str,"");
      break;
    }
    font_width=XTextWidth(fontStruct,str,strlen(str));
    XDrawString(dis,cmappix,
		fontgc,cmapAxesPosition[2]*width/8+spc,
		cmapAxesPosition[3]*height/2,str,strlen(str));  
    
  }
  free(vertices);
}


Window NewButton(Window parent, char *name, int x, int y, 
		 int buttonwidth, int buttonheight, bool motion, int bordercolor) {
  char c;

  XSetWindowAttributes attr;
  XSizeHints hints;
  Window button;

  attr.background_pixel = black;
  attr.border_pixel = bordercolor;
  attr.backing_store = NotUseful;
  if(motion) 
    attr.event_mask = ButtonReleaseMask | ButtonPressMask | Button1MotionMask ;
  else
    attr.event_mask = ButtonReleaseMask | ButtonPressMask;
  attr.bit_gravity = SouthWestGravity;
  attr.win_gravity = SouthWestGravity;
  attr.save_under = False;
  button = XCreateWindow(dis,parent, x, y, buttonwidth, buttonheight,
                         2, 0, InputOutput, CopyFromParent,
                         CWBackPixel | CWBorderPixel | CWEventMask |
                         CWBitGravity | CWWinGravity | CWBackingStore |
                         CWSaveUnder, &attr);
  hints.width = buttonwidth;
  hints.height = buttonheight - 4;
  hints.min_width = 0;
  hints.min_height = buttonheight - 4;
  hints.max_width = buttonwidth;
  hints.max_height = buttonheight - 4;
  hints.width_inc = 1;
  hints.height_inc = 1;
  hints.flags = PMinSize | PMaxSize | PSize | PResizeInc;
  XSetStandardProperties(dis,button,name,"SUNTANS",None,(char **)NULL,0,&hints);

  return button;
}

void DrawButton(Window button, char *str, int bcolor) {
  int x, y, w, h, d, b, font_width, font_height, border=3;
  Window root;

  font_width=XTextWidth(fontStruct,str,strlen(str));
  font_height=fontStruct->ascent+fontStruct->descent;

  XGetGeometry(dis,button,&root, &x, &y, &w, &h, &d, &b);

  XSetForeground(dis,gc,bcolor);  
  XFillRectangle(dis,button, gc, 0, 0,w,h);

  XSetForeground(dis,gc,black);  
  XDrawRectangle(dis,button, gc,border,border,w-2*border,h-2*border);
  XDrawLine(dis,button,gc,0,0,border,border);
  XDrawLine(dis,button,gc,w,0,w-border,border);
  XDrawLine(dis,button,gc,0,h,border,h-border);
  XDrawLine(dis,button,gc,w,h,w-border,h-border);

  XSetForeground(dis,fontgc,black);  
  XDrawString(dis,button,fontgc,(int)(w/2-font_width/2),(int)font_height,str,strlen(str));  
}

void DrawHeader(Window leftwin,Window rightwin,char *str) {
  int x1, y1, w1, h1, d1, b1, font_width, font_height,
    x2, y2, w2, h2, d2, b2, headsep=5;

  font_width=XTextWidth(fontStruct,str,strlen(str));
  font_height=fontStruct->ascent+fontStruct->descent;

  XGetGeometry(dis,leftwin,&RootWindow(dis,0), &x1, &y1, &w1, &h1, &d1, &b1);
  XGetGeometry(dis,rightwin,&RootWindow(dis,0), &x2, &y2, &w2, &h2, &d2, &b2);

  XSetForeground(dis,fontgc,white);  
  XDrawString(dis,controlspix,
	      fontgc,(int)((x1+x2+w2)/2)-font_width/2,y1-headsep,str,strlen(str));  
}

void DrawZoomBox(void) {
  int x1, y1, boxwidth, boxheight;
  boxwidth=abs(xend-xstart);
  boxheight=abs(yend-ystart);
  if(xend>xstart)
    x1 = xstart;
  else
    x1 = xend;
  if(yend>ystart)
    y1 = ystart;
  else
    y1 = yend;
  XSetForeground(dis,gc,white);
  XDrawRectangle(dis,axeswin,gc,
		 x1,y1,boxwidth,boxheight);
}

void DrawSliceLine(int where) {
  if(where==0) {
    XSetForeground(dis,gc,white);
    XDrawLine(dis,axeswin,gc,xstart,ystart,xend,yend);
  } else {
    XSetForeground(dis,gc,white);
    XDrawLine(dis,pix,gc,xstart,ystart,xend,yend);
  }
}

void SetUpButtons(void) {
  int buttonnum;
  float dist=0.25;

  AllButtonsFalse();

  controlButtons[prevwin].string="<--";
  controlButtons[prevwin].mapstring="prev";
  controlButtons[prevwin].l=0.05;
  controlButtons[prevwin].b=0.12;
  controlButtons[prevwin].w=0.35;
  controlButtons[prevwin].h=(float)BUTTONHEIGHT;

  controlButtons[gowin].string="M";
  controlButtons[gowin].mapstring="go";
  controlButtons[gowin].l=0.45;
  controlButtons[gowin].b=0.12;
  controlButtons[gowin].w=0.1;
  controlButtons[gowin].h=(float)BUTTONHEIGHT;

  controlButtons[nextwin].string="-->";
  controlButtons[nextwin].mapstring="next";
  controlButtons[nextwin].l=0.6;
  controlButtons[nextwin].b=0.12;
  controlButtons[nextwin].w=0.35;
  controlButtons[nextwin].h=(float)BUTTONHEIGHT;

  controlButtons[kdownwin].string="<--";
  controlButtons[kdownwin].mapstring="kdownwin";
  controlButtons[kdownwin].l=0.05;
  controlButtons[kdownwin].b=controlButtons[nextwin].b+dist;
  controlButtons[kdownwin].w=0.4;
  controlButtons[kdownwin].h=(float)BUTTONHEIGHT;

  controlButtons[kupwin].string="-->";
  controlButtons[kupwin].mapstring="kupwin";
  controlButtons[kupwin].l=0.55;
  controlButtons[kupwin].b=controlButtons[nextwin].b+dist;
  controlButtons[kupwin].w=0.4;
  controlButtons[kupwin].h=(float)BUTTONHEIGHT;

  controlButtons[prevprocwin].string="<--";
  controlButtons[prevprocwin].mapstring="prevprocwin";
  controlButtons[prevprocwin].l=0.05;
  controlButtons[prevprocwin].b=controlButtons[nextwin].b+2*dist;
  controlButtons[prevprocwin].w=0.25;
  controlButtons[prevprocwin].h=(float)BUTTONHEIGHT;

  controlButtons[allprocswin].string="ALL";
  controlButtons[allprocswin].mapstring="allprocswin";
  controlButtons[allprocswin].l=0.375;
  controlButtons[allprocswin].b=controlButtons[nextwin].b+2*dist;
  controlButtons[allprocswin].w=0.25;
  controlButtons[allprocswin].h=(float)BUTTONHEIGHT;

  controlButtons[nextprocwin].string="-->";
  controlButtons[nextprocwin].mapstring="nextprocwin";
  controlButtons[nextprocwin].l=0.7;
  controlButtons[nextprocwin].b=controlButtons[nextwin].b+2*dist;
  controlButtons[nextprocwin].w=0.25;
  controlButtons[nextprocwin].h=(float)BUTTONHEIGHT;

  controlButtons[saltwin].string="S";
  controlButtons[saltwin].mapstring="saltwin";
  controlButtons[saltwin].l=0.05;
  controlButtons[saltwin].b=controlButtons[nextwin].b+3*dist;
  controlButtons[saltwin].w=0.1;
  controlButtons[saltwin].h=(float)BUTTONHEIGHT;

  controlButtons[tempwin].string="T";
  controlButtons[tempwin].mapstring="tempwin";
  controlButtons[tempwin].l=0.2;
  controlButtons[tempwin].b=controlButtons[nextwin].b+3*dist;
  controlButtons[tempwin].w=0.1;
  controlButtons[tempwin].h=(float)BUTTONHEIGHT;

  controlButtons[preswin].string="q";
  controlButtons[preswin].mapstring="preswin";
  controlButtons[preswin].l=0.35;
  controlButtons[preswin].b=controlButtons[nextwin].b+3*dist;
  controlButtons[preswin].w=0.1;
  controlButtons[preswin].h=(float)BUTTONHEIGHT;

  controlButtons[fswin].string="h";
  controlButtons[fswin].mapstring="saltwin";
  controlButtons[fswin].l=0.55;
  controlButtons[fswin].b=controlButtons[nextwin].b+3*dist;
  controlButtons[fswin].w=0.1;
  controlButtons[fswin].h=(float)BUTTONHEIGHT;

  controlButtons[nutwin].string="n";
  controlButtons[nutwin].mapstring="nutwin";
  controlButtons[nutwin].l=0.7;
  controlButtons[nutwin].b=controlButtons[nextwin].b+3*dist;
  controlButtons[nutwin].w=0.1;
  controlButtons[nutwin].h=(float)BUTTONHEIGHT;

  controlButtons[ktwin].string="k";
  controlButtons[ktwin].mapstring="ktwin";
  controlButtons[ktwin].l=0.85;
  controlButtons[ktwin].b=controlButtons[nextwin].b+3*dist;
  controlButtons[ktwin].w=0.1;
  controlButtons[ktwin].h=(float)BUTTONHEIGHT;

  controlButtons[uwin].string="U";
  controlButtons[uwin].mapstring="uwin";
  controlButtons[uwin].l=0.05;
  controlButtons[uwin].b=controlButtons[nextwin].b+4*dist;
  controlButtons[uwin].w=0.1;
  controlButtons[uwin].h=(float)BUTTONHEIGHT;

  controlButtons[vwin].string="V";
  controlButtons[vwin].mapstring="vwin";
  controlButtons[vwin].l=0.2;
  controlButtons[vwin].b=controlButtons[nextwin].b+4*dist;
  controlButtons[vwin].w=0.1;
  controlButtons[vwin].h=(float)BUTTONHEIGHT;

  controlButtons[wwin].string="W";
  controlButtons[wwin].mapstring="wwin";
  controlButtons[wwin].l=0.35;
  controlButtons[wwin].b=controlButtons[nextwin].b+4*dist;
  controlButtons[wwin].w=0.1;
  controlButtons[wwin].h=(float)BUTTONHEIGHT;

  controlButtons[vecwin].string="Vectors";
  controlButtons[vecwin].mapstring="vecwin";
  controlButtons[vecwin].l=0.55;
  controlButtons[vecwin].b=controlButtons[nextwin].b+4*dist;
  controlButtons[vecwin].w=0.4;
  controlButtons[vecwin].h=(float)BUTTONHEIGHT;

  controlButtons[depthwin].string="Depth";
  controlButtons[depthwin].mapstring="depthwin";
  controlButtons[depthwin].l=0.05;
  controlButtons[depthwin].b=controlButtons[nextwin].b+5*dist;
  controlButtons[depthwin].w=0.4;
  controlButtons[depthwin].h=(float)BUTTONHEIGHT;

  controlButtons[nonewin].string="None";
  controlButtons[nonewin].mapstring="nonewin";
  controlButtons[nonewin].l=0.55;
  controlButtons[nonewin].b=controlButtons[nextwin].b+5*dist;
  controlButtons[nonewin].w=0.4;
  controlButtons[nonewin].h=(float)BUTTONHEIGHT;

  controlButtons[prevsediwin].string="<-";
  controlButtons[prevsediwin].mapstring="prevsediwin";
  controlButtons[prevsediwin].l=0.05;
  controlButtons[prevsediwin].b=controlButtons[nextwin].b+6*dist;
  controlButtons[prevsediwin].w=0.2;
  controlButtons[prevsediwin].h=(float)BUTTONHEIGHT;

  controlButtons[allsediwin].string="sediment";
  controlButtons[allsediwin].mapstring="allsediwin";
  controlButtons[allsediwin].l=0.3;
  controlButtons[allsediwin].b=controlButtons[nextwin].b+6*dist;
  controlButtons[allsediwin].w=0.4;
  controlButtons[allsediwin].h=(float)BUTTONHEIGHT;

  controlButtons[nextsediwin].string="->";
  controlButtons[nextsediwin].mapstring="nextsediwin";
  controlButtons[nextsediwin].l=0.75;
  controlButtons[nextsediwin].b=controlButtons[nextwin].b+6*dist;
  controlButtons[nextsediwin].w=0.2;
  controlButtons[nextsediwin].h=(float)BUTTONHEIGHT;

  controlButtons[layerwin].string="Layer";
  controlButtons[layerwin].mapstring="layerwin";
  controlButtons[layerwin].l=0.04;
  controlButtons[layerwin].b=controlButtons[nextwin].b+7*dist;
  controlButtons[layerwin].w=0.44;
  controlButtons[layerwin].h=(float)BUTTONHEIGHT;

  controlButtons[taubwin].string="Taub";
  controlButtons[taubwin].mapstring="taubwin";
  controlButtons[taubwin].l=0.52;
  controlButtons[taubwin].b=controlButtons[nextwin].b+7*dist;
  controlButtons[taubwin].w=0.44;
  controlButtons[taubwin].h=(float)BUTTONHEIGHT;

  controlButtons[edgewin].string="Edges";
  controlButtons[edgewin].mapstring="edgewin";
  controlButtons[edgewin].l=0.05;
  controlButtons[edgewin].b=controlButtons[nextwin].b+8*dist;
  controlButtons[edgewin].w=0.25;
  controlButtons[edgewin].h=(float)BUTTONHEIGHT;

  controlButtons[voronoiwin].string="Voro";
  controlButtons[voronoiwin].mapstring="voronoiwin";
  controlButtons[voronoiwin].l=0.375;
  controlButtons[voronoiwin].b=controlButtons[nextwin].b+8*dist;
  controlButtons[voronoiwin].w=0.25;
  controlButtons[voronoiwin].h=(float)BUTTONHEIGHT;

  controlButtons[delaunaywin].string="Dela";
  controlButtons[delaunaywin].mapstring="delaunaywin";
  controlButtons[delaunaywin].l=0.7;
  controlButtons[delaunaywin].b=controlButtons[nextwin].b+8*dist;
  controlButtons[delaunaywin].w=0.25;
  controlButtons[delaunaywin].h=(float)BUTTONHEIGHT;

  controlButtons[zoomwin].string="ZOOM";
  controlButtons[zoomwin].mapstring="zoomwin";
  controlButtons[zoomwin].l=0.05;
  controlButtons[zoomwin].b=controlButtons[nextwin].b+9*dist;
  controlButtons[zoomwin].w=0.9;
  controlButtons[zoomwin].h=(float)BUTTONHEIGHT;
  controlButtons[zoomwin].status=true;

  controlButtons[profwin].string="Profile";
  controlButtons[profwin].mapstring="profwin";
  controlButtons[profwin].l=0.05;
  controlButtons[profwin].b=controlButtons[nextwin].b+10*dist;
  controlButtons[profwin].w=0.9;
  controlButtons[profwin].h=(float)BUTTONHEIGHT;

  controlButtons[iskip_minus_win].string="<";
  controlButtons[iskip_minus_win].mapstring="iskip_minus_win";
  controlButtons[iskip_minus_win].l=0.05;
  controlButtons[iskip_minus_win].b=controlButtons[nextwin].b+11*dist;
  controlButtons[iskip_minus_win].w=0.2;
  controlButtons[iskip_minus_win].h=(float)BUTTONHEIGHT;

  controlButtons[iskip_plus_win].string=">";
  controlButtons[iskip_plus_win].mapstring="iskip_plus_win";
  controlButtons[iskip_plus_win].l=0.27;
  controlButtons[iskip_plus_win].b=controlButtons[nextwin].b+11*dist;
  controlButtons[iskip_plus_win].w=0.2;
  controlButtons[iskip_plus_win].h=(float)BUTTONHEIGHT;

  controlButtons[kskip_minus_win].string="<";
  controlButtons[kskip_minus_win].mapstring="kskip_minus_win";
  controlButtons[kskip_minus_win].l=0.53;
  controlButtons[kskip_minus_win].b=controlButtons[nextwin].b+11*dist;
  controlButtons[kskip_minus_win].w=0.2;
  controlButtons[kskip_minus_win].h=(float)BUTTONHEIGHT;

  controlButtons[kskip_plus_win].string=">";
  controlButtons[kskip_plus_win].mapstring="kskip_plus_win";
  controlButtons[kskip_plus_win].l=0.75;
  controlButtons[kskip_plus_win].b=controlButtons[nextwin].b+11*dist;
  controlButtons[kskip_plus_win].w=0.2;
  controlButtons[kskip_plus_win].h=(float)BUTTONHEIGHT;

  controlButtons[axisimagewin].string="Aspect";
  controlButtons[axisimagewin].mapstring="axisimagewin";
  controlButtons[axisimagewin].l=0.05;
  controlButtons[axisimagewin].b=controlButtons[nextwin].b+12*dist;
  controlButtons[axisimagewin].w=0.4;
  controlButtons[axisimagewin].h=(float)BUTTONHEIGHT;

  controlButtons[cmapholdwin].string="Caxis";
  controlButtons[cmapholdwin].mapstring="axisimagewin";
  controlButtons[cmapholdwin].l=0.55;
  controlButtons[cmapholdwin].b=controlButtons[nextwin].b+12*dist;
  controlButtons[cmapholdwin].w=0.4;
  controlButtons[cmapholdwin].h=(float)BUTTONHEIGHT;

  controlButtons[reloadwin].string="Reload";
  controlButtons[reloadwin].mapstring="reloadwin";
  controlButtons[reloadwin].l=0.05;
  controlButtons[reloadwin].b=controlButtons[nextwin].b+13*dist;
  controlButtons[reloadwin].w=0.9;
  controlButtons[reloadwin].h=(float)BUTTONHEIGHT;

  controlButtons[quitwin].string="QUIT";
  controlButtons[quitwin].mapstring="quitwin";
  controlButtons[quitwin].l=0.05;
  controlButtons[quitwin].b=controlButtons[nextwin].b+14*dist;
  controlButtons[quitwin].w=0.9;
  controlButtons[quitwin].h=(float)BUTTONHEIGHT;
}

void ParseCommandLine(int N, char *argv[], int *numprocs, int *n, int *k, dimT *dimensions, goT *go)
{
  int i, j, js, done=0, status;
  struct stat filestat;
  char str[BUFFERLENGTH], val[BUFFERLENGTH];
  *numprocs=1;
  *dimensions=three_d;
  *go=nomovie;

  sprintf(DATADIR,".");
  sprintf(DATAFILE,"%s/%s",DATADIR,DEFAULTDATAFILE);

  if(N>1) 
    for(i=1;i<N;i++) {
      if(argv[i][0]=='-') {
	if(!strcmp(argv[i],"-np"))
	  *numprocs = (int)atof(argv[++i]);
	else if(!strcmp(argv[i],"-n"))
	  *n = (int)atof(argv[++i]);
	else if(!strcmp(argv[i],"-k"))
	  *k = (int)atof(argv[++i]);
	else if(!strcmp(argv[i],"-2d"))
	  *dimensions=two_d;
	else if(!strcmp(argv[i],"-d"))
	  DEBUG=true;
	else if(!strcmp(argv[i],"-g"))
	  plotGrid=true;
	else if(!strcmp(argv[i],"-m"))
	  *go=forward;
	else if(argv[i][1]=='-') {
	  if(!strncmp(argv[i],"--datadir=",strlen("--datadir="))) {
	    js=strlen("--datadir=");
	    for(j=js;j<strlen(argv[i]);j++)
	      DATADIR[j-js]=argv[i][j];
	    sprintf(DATAFILE,"%s/%s",DATADIR,DEFAULTDATAFILE);
	  }
	} else
	  done=1;
      } else
	done=1;
    }

  if(stat(DATAFILE,&filestat)==-1) {
    sprintf(str,"Error opening data file %s",DATAFILE);
    perror(str);
    exit(EXIT_FAILURE);
  }

  if(done) {
    Usage(argv[0]);
    exit(1);
  }
}    

void Usage(char *str) {
  printf("Usage: %s [-np (numprocs), -2d, -n (time step), -k (vert level), -g, -m]\n",str);
}

void FindNearest(dataT *data, int x, int y, int *iloc, int *procloc, int procnum, int numprocs) {
  int proc, i, procstart, procend;
  float mindist, dist, xval, yval;
  //datalimits is xmin xmax ymin ymax
  xval = dataLimits[0]+
    (float)x*(dataLimits[1]-dataLimits[0])/(axesPosition[2]*(float)width);
  yval = dataLimits[2]+
    (float)(height*axesPosition[3]-y)*
    (dataLimits[3]-dataLimits[2])/(axesPosition[3]*(float)height);

  if(procplottype==allprocs) {
    procstart=0;
    procend=numprocs;
  } else {
    procstart=procnum;
    procend=procnum+1;
  }

  for(proc=procstart;proc<procend;proc++) {
    for(i=0;i<data->Nc[proc];i++) {
      dist = sqrt(pow(xval-data->xv[proc][i],2)+pow(yval-data->yv[proc][i],2));
      if(i==0 && proc==procstart) mindist=dist;
      if(dist<=mindist) {
	mindist=dist;
	*iloc=i;
	*procloc=proc;
      }
    }
  }
}

dataT *NewData(int numprocs) {
  int proc, nosize;
  dataT *data = (dataT *)malloc(sizeof(dataT));
  data->timestep=-1;
  data->klevel=-1;
  data->sedinum=1;
  data->bg_salinity_loaded=0;
  data->initial_layer_loaded=0;
  data->freesurface_step_loaded=(int *)malloc(numprocs*sizeof(int));
  data->salinity_step_loaded=(int *)malloc(numprocs*sizeof(int));
  data->temperature_step_loaded=(int *)malloc(numprocs*sizeof(int));
  data->pressure_step_loaded=(int *)malloc(numprocs*sizeof(int));
  data->velocity_step_loaded=(int *)malloc(numprocs*sizeof(int));
  data->nut_step_loaded=(int *)malloc(numprocs*sizeof(int));
  data->kt_step_loaded=(int *)malloc(numprocs*sizeof(int));
  data->layer_step_loaded=(int *)malloc(numprocs*sizeof(int));
  data->sedi_step_loaded=(int **)malloc(MAX_SED_SIZES*sizeof(int *));
  for(nosize=0;nosize<MAX_SED_SIZES;nosize++)
    data->sedi_step_loaded[nosize]=(int *)malloc(numprocs*sizeof(int));
  data->taub_step_loaded=(int *)malloc(numprocs*sizeof(int));

  for(proc=0;proc<numprocs;proc++) {
    data->freesurface_step_loaded[proc]=0;
    data->salinity_step_loaded[proc]=0;
    data->temperature_step_loaded[proc]=0;
    data->pressure_step_loaded[proc]=0;
    data->velocity_step_loaded[proc]=0;
    data->nut_step_loaded[proc]=0;
    data->kt_step_loaded[proc]=0;
    data->layer_step_loaded[proc]=0;
    data->taub_step_loaded[proc]=0;
    for(nosize=0;nosize<MAX_SED_SIZES;nosize++)
      data->sedi_step_loaded[nosize][proc]=0;
  }
  return data;
}

void CloseGraphics(void) {
  XFreePixmap(dis,pix);  
  XFreePixmap(dis,controlspix);  
  XDestroyWindow(dis,win);
  XCloseDisplay(dis);
  XFlush(dis);
}

void FreeData(dataT *data, int numprocs) {
  int proc, j,nosize;
    for(proc=0;proc<numprocs;proc++) {
      free(data->cells[proc]);
      free(data->edges[proc]);
      free(data->face[proc]);
      free(data->xv[proc]);
      free(data->yv[proc]);
      free(data->layerthickness[proc]);
      free(data->initial_layerthickness[proc]);
      free(data->taub[proc]);
      free(data->depth[proc]);
      free(data->h[proc]);
      free(data->h_d[proc]);
      free(data->nfaces[proc]);
      for(j=0;j<data->Nkmax;j++) {
        for(nosize=0;nosize<data->Nsize;nosize++)
          free(data->sediC[proc][nosize][j]);
        free(data->allsediC[proc][j]);
	free(data->s[proc][j]);
	free(data->sd[proc][j]);
	free(data->s0[proc][j]);
	free(data->T[proc][j]);
	free(data->q[proc][j]);
	free(data->u[proc][j]);
	free(data->v[proc][j]);
	free(data->nut[proc][j]);
	free(data->kt[proc][j]);
	free(data->wf[proc][j]);
	free(data->w[proc][j]);
      }
      free(data->w[proc][data->Nkmax]);
      free(data->allsediC[proc]);
      for(nosize=0;nosize<data->Nsize;nosize++)
        free(data->sediC[proc][nosize]);
      free(data->sediC[proc]);
      free(data->s[proc]);
      free(data->sd[proc]);
      free(data->s0[proc]);
      free(data->T[proc]);
      free(data->q[proc]);
      free(data->u[proc]);
      free(data->v[proc]);
      free(data->nut[proc]);
      free(data->kt[proc]);
      free(data->wf[proc]);
      free(data->w[proc]);
    }
    free(data->allsediC);
    free(data->sediC);
    free(data->layerthickness);
    free(data->initial_layerthickness);
    free(data->taub);
    free(data->cells);
    free(data->nfaces);
    free(data->edges);
    free(data->face);
    free(data->xv);
    free(data->yv);
    free(data->depth);
    free(data->h);
    free(data->h_d);
    free(data->s);
    free(data->sd);
    free(data->s0);
    free(data->T);
    free(data->u);
    free(data->v);
    free(data->nut);
    free(data->kt);
    free(data->wf);
    free(data->w);
    free(data->Ne);
    free(data->Nc);
    free(data->xc);
    free(data->yc);
}

int GetFile(char *string, char *datadir, char *datafile, char *name, int proc) {
  int status;
  char str[BUFFERLENGTH];
  
  GetString(str,datafile,name,&status);
  if(proc==-1) {
    if(str[0]=='/')
      strcpy(string,str);
    else
      sprintf(string,"%s/%s",datadir,str);
  } else {
    if(str[0]=='/')
      sprintf(string,"%s.%d",str,proc);
    else
      sprintf(string,"%s/%s.%d",datadir,str,proc);
  }

  return status;
}

void ReadData(dataT *data, int nstep, int numprocs) {
  int i, j, ik, ik0, proc, status, ind, ind0, nf, count, nsteps, ntout, nk, turbmodel, kk, numcolumns, nosize, nosize_start, nosize_end;
  float xind, vel, ubar, vbar, dz, beta, dmax, fdepth;
  double *dummy;
  char string[BUFFERLENGTH];
  FILE *fid;

  GetFile(POINTSFILE,DATADIR,DATAFILE,"points",-1);
  GetFile(EDGEFILE,DATADIR,DATAFILE,"edges",-1);
  GetFile(CELLSFILE,DATADIR,DATAFILE,"cells",-1);
  GetFile(INPUTDEPTHFILE,DATADIR,DATAFILE,"depth",-1); 
  GetFile(CELLCENTEREDFILE,DATADIR,DATAFILE,"celldata",-1);
  GetFile(VERTSPACEFILE,DATADIR,DATAFILE,"vertspace",-1);

  if(nstep==-1) {
    data->Np=getsize(POINTSFILE);

    data->cells = (int **)malloc(numprocs*sizeof(int *));
    data->edges = (int **)malloc(numprocs*sizeof(int *));
    data->mark = (int **)malloc(numprocs*sizeof(int *));
    data->face = (int **)malloc(numprocs*sizeof(int *));
    //added part
    data->nfaces = (int **)malloc(numprocs*sizeof(int *));

    data->xv = (float **)malloc(numprocs*sizeof(float *));
    data->yv = (float **)malloc(numprocs*sizeof(float *));

    data->depth = (float **)malloc(numprocs*sizeof(float *));
    data->h = (float **)malloc(numprocs*sizeof(float *));
    data->h_d = (float **)malloc(numprocs*sizeof(float *));
    data->s = (float ***)malloc(numprocs*sizeof(float **));
    data->sd = (float ***)malloc(numprocs*sizeof(float **));
    data->s0 = (float ***)malloc(numprocs*sizeof(float **));
    data->T = (float ***)malloc(numprocs*sizeof(float **));
    data->q = (float ***)malloc(numprocs*sizeof(float **));
    data->u = (float ***)malloc(numprocs*sizeof(float **));
    data->v = (float ***)malloc(numprocs*sizeof(float **));
    data->nut = (float ***)malloc(numprocs*sizeof(float **));
    data->kt = (float ***)malloc(numprocs*sizeof(float **));
    data->ubar = (float **)malloc(numprocs*sizeof(float *));
    data->vbar = (float **)malloc(numprocs*sizeof(float *));
    data->wf = (float ***)malloc(numprocs*sizeof(float **));
    data->w = (float ***)malloc(numprocs*sizeof(float **));

    data->Ne = (int *)malloc(numprocs*sizeof(int));
    data->Nc = (int *)malloc(numprocs*sizeof(int));
    
    data->timestep=-1;
    data->maxfaces=(int)GetValue(DATAFILE,"maxFaces",&status);
    if(data->maxfaces==0)
      data->maxfaces=3;
    data->Nkmax=(int)GetValue(DATAFILE,"Nkmax",&status);
    nsteps=(int)GetValue(DATAFILE,"nsteps",&status);
    data->ntout = (int)GetValue(DATAFILE,"ntout",&status);
    if(data->ntout==1 || nsteps==1)
      data->nsteps=nsteps;
    else
      data->nsteps=1+(int)GetValue(DATAFILE,"nsteps",&status)/(data->ntout);

    data->sedi=(int)GetValue(DATAFILE,"computeSediments",&status);
    if(data->sedi==1){
      data->Nsize=(int)GetValue(DATAFILE,"Nsize",&status);
      data->tbmax=(int)GetValue(DATAFILE,"TBMAX",&status);      
    } else {
      data->Nsize=1;
    }
    data->taub =  (float **)malloc(numprocs*sizeof(float *));
    data->sediC = (float ****)malloc(numprocs*sizeof(float ***));
    data->allsediC = (float ***)malloc(numprocs*sizeof(float **));
    data->layerthickness =  (float **)malloc(numprocs*sizeof(float *));
    data->initial_layerthickness =  (float **)malloc(numprocs*sizeof(float *));

    data->numprocs=numprocs;

    if(mergeArrays) {
      for(proc=0;proc<numprocs;proc++) {
	data->Ne[proc]=getsize(EDGEFILE);
	data->Nc[proc]=getsize(CELLSFILE);
      }
    } else {
      for(proc=0;proc<numprocs;proc++) {
	sprintf(string,"%s.%d",EDGEFILE,proc);
	data->Ne[proc]=getsize(string);
	sprintf(string,"%s.%d",CELLSFILE,proc);
	data->Nc[proc]=getsize(string);
      }
    }
    
    data->xc = (float *)malloc(data->Np*sizeof(float *));
    data->yc = (float *)malloc(data->Np*sizeof(float *));

    data->sliceData = (float **)malloc(NSLICEMAX*sizeof(float *));
    data->sliceU = (float **)malloc(NSLICEMAX*sizeof(float *));
    data->sliceW = (float **)malloc(NSLICEMAX*sizeof(float *));
    data->sliceH = (float *)malloc(NSLICEMAX*sizeof(float));
    data->sliceD = (float *)malloc(NSLICEMAX*sizeof(float));
    for(i=0;i<NSLICEMAX;i++) {
      data->sliceData[i] = (float *)malloc(data->Nkmax*sizeof(float));
      data->sliceU[i] = (float *)malloc(data->Nkmax*sizeof(float));
      data->sliceW[i] = (float *)malloc(data->Nkmax*sizeof(float));
    }
    data->sliceX = (float *)malloc(NSLICEMAX*sizeof(float));
    data->z = (float *)malloc((data->Nkmax+1)*sizeof(float));

    data->sliceInd = (int *)malloc(NSLICEMAX*sizeof(int));
    data->sliceProc = (int *)malloc(NSLICEMAX*sizeof(int));
    data->Nslice=0;

    for(proc=0;proc<numprocs;proc++) {

      data->cells[proc]=(int *)malloc(data->maxfaces*data->Nc[proc]*sizeof(int));
      data->edges[proc]=(int *)malloc(2*data->Ne[proc]*sizeof(int));
      data->mark[proc]=(int *)malloc(data->Ne[proc]*sizeof(int));
      data->face[proc]=(int *)malloc(data->maxfaces*data->Nc[proc]*sizeof(int));

      data->xv[proc]=(float *)malloc(data->Nc[proc]*sizeof(float));
      data->yv[proc]=(float *)malloc(data->Nc[proc]*sizeof(float));
      data->nfaces[proc]=(int *)malloc(data->Nc[proc]*sizeof(float));

      // added sediment part 
      data->sediC[proc]=(float ***)malloc(data->Nsize*sizeof(float **));
      for(nosize=0;nosize<data->Nsize;nosize++)
        data->sediC[proc][nosize]=(float **)malloc(data->Nkmax*sizeof(float *));        
      data->allsediC[proc]=(float **)malloc(data->Nkmax*sizeof(float *));
      data->layerthickness[proc]=(float *)malloc(data->Nc[proc]*sizeof(float));
      data->initial_layerthickness[proc]=(float *)malloc(data->Nc[proc]*sizeof(float));
      data->taub[proc]=(float *)malloc(data->Nc[proc]*sizeof(float));

      data->depth[proc]=(float *)malloc(data->Nc[proc]*sizeof(float));
      data->h[proc]=(float *)malloc(data->Nc[proc]*sizeof(float));
      data->h_d[proc]=(float *)malloc(data->Nc[proc]*sizeof(float));
      data->s[proc]=(float **)malloc(data->Nkmax*sizeof(float *));
      data->sd[proc]=(float **)malloc(data->Nkmax*sizeof(float *));
      data->s0[proc]=(float **)malloc(data->Nkmax*sizeof(float *));
      data->T[proc]=(float **)malloc(data->Nkmax*sizeof(float *));
      data->q[proc]=(float **)malloc(data->Nkmax*sizeof(float *));
      data->u[proc]=(float **)malloc(data->Nkmax*sizeof(float *));
      data->v[proc]=(float **)malloc(data->Nkmax*sizeof(float *));
      data->ubar[proc]=(float *)malloc(data->Nc[proc]*sizeof(float));
      data->vbar[proc]=(float *)malloc(data->Nc[proc]*sizeof(float));
      data->wf[proc]=(float **)malloc(data->Nkmax*sizeof(float *));
      data->w[proc]=(float **)malloc(data->Nkmax*sizeof(float *));
      data->nut[proc]=(float **)malloc(data->Nkmax*sizeof(float *));
      data->kt[proc]=(float **)malloc(data->Nkmax*sizeof(float *));

      for(i=0;i<data->Nkmax;i++) {
	data->s[proc][i]=(float *)malloc(data->Nc[proc]*sizeof(float));
	data->sd[proc][i]=(float *)malloc(data->Nc[proc]*sizeof(float));
	data->s0[proc][i]=(float *)malloc(data->Nc[proc]*sizeof(float));

        //added sediment part
        for(nosize=0;nosize<data->Nsize;nosize++)
	  data->sediC[proc][nosize][i]=(float *)malloc(data->Nc[proc]*sizeof(float));
	data->allsediC[proc][i]=(float *)malloc(data->Nc[proc]*sizeof(float));

	data->T[proc][i]=(float *)malloc(data->Nc[proc]*sizeof(float));
	data->q[proc][i]=(float *)malloc(data->Nc[proc]*sizeof(float));
	data->u[proc][i]=(float *)malloc(data->Nc[proc]*sizeof(float));
	data->v[proc][i]=(float *)malloc(data->Nc[proc]*sizeof(float));
	data->nut[proc][i]=(float *)malloc(data->Nc[proc]*sizeof(float));
	data->kt[proc][i]=(float *)malloc(data->Nc[proc]*sizeof(float));
	data->wf[proc][i]=(float *)malloc(data->Ne[proc]*sizeof(float));
	data->w[proc][i]=(float *)malloc(data->Nc[proc]*sizeof(float));
	}
      data->w[proc][data->Nkmax]=(float *)malloc(data->Nc[proc]*sizeof(float));
    }

    fid = MyFOpen(POINTSFILE,"r","ReadData");
    for(i=0;i<data->Np;i++) 
      fscanf(fid,"%f %f %d",&(data->xc[i]),&(data->yc[i]),&ind);
    fclose(fid);  

    for(proc=0;proc<numprocs;proc++) {
      if(mergeArrays) {
	fid = MyFOpen(CELLSFILE,"r","ReadData");
	numcolumns=getNumColumns(CELLSFILE);
      } else {
	sprintf(string,"%s.%d",CELLSFILE,proc);
	fid = MyFOpen(string,"r","ReadData");
	numcolumns=getNumColumns(string);
      }

      //corrected part (only for 3 or 4)
      for(i=0;i<data->Nc[proc];i++) {
        if(numcolumns<=8)
	  data->nfaces[proc][i]=3;
	else
	  fscanf(fid,"%d ",&(data->nfaces[proc][i]));
	// printf("%d\n   ",data->nfaces[proc][i]);
        fscanf(fid,"%f %f ",&(data->xv[proc][i]),&(data->yv[proc][i]));
        for(kk=0;kk<data->nfaces[proc][i];kk++)
          fscanf(fid,"%d ",&(data->cells[proc][data->maxfaces*i+kk]));
        for(kk=0;kk<data->nfaces[proc][i];kk++){
          if(kk==(data->nfaces[proc][i]-1))
            fscanf(fid,"%d",&ind);
          else
            fscanf(fid,"%d ",&ind);
        }  
      }
      fclose(fid);

      if(mergeArrays) {
	fid = MyFOpen(EDGEFILE,"r","ReadData");
	numcolumns=getNumColumns(EDGEFILE);
      } else {
	sprintf(string,"%s.%d",EDGEFILE,proc);
	numcolumns=getNumColumns(string);
	fid = MyFOpen(string,"r","ReadData");
      }
      if(numcolumns==5) {
	for(i=0;i<data->Ne[proc];i++) 
	  fscanf(fid,"%d %d %d %d %d",
		 &(data->edges[proc][2*i]),
		 &(data->edges[proc][2*i+1]),
		 &(data->mark[proc][i]),&ind,&ind);
      } else {
	for(i=0;i<data->Ne[proc];i++) 
	  fscanf(fid,"%d %d %d %d %d %d",
		 &(data->edges[proc][2*i]),
		 &(data->edges[proc][2*i+1]),
		 &(data->mark[proc][i]),&ind,&ind,&ind);
      }
      fclose(fid);

      if(mergeArrays) {
	sprintf(string,"%s-voro",INPUTDEPTHFILE);
	fid = MyFOpen(string,"r","ReadData");

	for(i=0;i<data->Nc[proc];i++) {
	  fscanf(fid,"%f %f %f",&fdepth,&fdepth,&fdepth);
	  data->depth[proc][i]=fdepth;
	}
      } else {
	sprintf(string,"%s.%d",CELLCENTEREDFILE,proc);
	fid = MyFOpen(string,"r","ReadData");
	//printf("Nc[proc=%d]=%d\n",proc,data->Nc[proc]);

	for(i=0;i<data->Nc[proc];i++) {
	  // This line was removed since 1 less int is printed to this file
	  // in grid.c (mnptr is no longer printed).
	  //	fscanf(fid,"%f %f %f %f %d %d %d %d %d %d %d %d %d %d %d",
	  fscanf(fid,"%d %f %f %f %f %d",&ind,&xind,&xind,&xind,&(data->depth[proc][i]),&ind);
	  //printf("proc:%d cell:%d xv=%f yv=%f\n",proc,i,data->xv[proc][i],data->yv[proc][i]);
	  for(kk=0;kk<data->nfaces[proc][i];kk++)
	    fscanf(fid,"%d ",&(data->face[proc][data->maxfaces*i+kk]));
	  for(kk=0;kk<data->nfaces[proc][i];kk++)
	    fscanf(fid,"%d ",&ind);
	  for(kk=0;kk<data->nfaces[proc][i];kk++)
	    fscanf(fid,"%d ",&ind);
	  for(kk=0;kk<data->nfaces[proc][i];kk++)
	    fscanf(fid,"%f ",&ind);
	  for(kk=0;kk<data->nfaces[proc][i];kk++)
	    fscanf(fid,"%d ",&ind);
	  fscanf(fid,"%d",&ind); // mnptr added during quad-netcdf merge
	}
	fclose(fid);
      }
    }
    GetDMax(data,numprocs);
    GetDMin(data,numprocs);
    // printf("%f \n",data->dmax);
    for(i=0;i<data->Nkmax+1;i++) 
      data->z[i]=-data->dmax*(float)i/(float)(data->Nkmax);
    sprintf(string,"%s",VERTSPACEFILE);
    fid = MyFOpen(string,"r","ReadData");
    data->z[0]=0;
    for(i=1;i<data->Nkmax+1;i++) {
      fscanf(fid,"%f",&dz);
      data->z[i]=data->z[i-1]-dz;
    }
  } else if(plotGrid != true) {

    data->timestep=nstep;

    for(proc=0;proc<numprocs;proc++) {

      dummy=(double *)malloc(data->Nc[proc]*sizeof(double));

      if(!(data->bg_salinity_loaded)) {
	data->bg_salinity_loaded=1;

	if(mergeArrays)
	  GetFile(string,DATADIR,DATAFILE,"BGSalinityFile",-1);
	else
	  GetFile(string,DATADIR,DATAFILE,"BGSalinityFile",proc);	  
	fid = fopen(string,"r");
	if(fid) {
	  for(i=0;i<data->Nkmax;i++) {
	    fread(dummy,sizeof(double),data->Nc[proc],fid);      
	    for(j=0;j<data->Nc[proc];j++) 
	      if(dummy[j]!=EMPTY)
		data->s0[proc][i][j]=dummy[j];
	      else
		data->s0[proc][i][j]=EMPTY;
	  }
	  fclose(fid);
	}
      }

      if(!(data->initial_layer_loaded)) {
	data->initial_layer_loaded=1;

	if(mergeArrays)
	  GetFile(string,DATADIR,DATAFILE,"LayerFile",-1);
	else
	  GetFile(string,DATADIR,DATAFILE,"LayerFile",proc);
        fid = fopen(string,"r");
	if(fid) {
	  fread(dummy,sizeof(double),data->Nc[proc],fid);      
	  for(i=0;i<data->Nc[proc];i++)
	    data->initial_layerthickness[proc][i]=dummy[i];
	  fclose(fid);
	}
      }
      
      if(mergeArrays)
	GetFile(string,DATADIR,DATAFILE,"SalinityFile",-1);
      else
	GetFile(string,DATADIR,DATAFILE,"SalinityFile",proc);
      fid = fopen(string,"r");
      if(fid && data->salinity_step_loaded[proc]!=data->timestep && (plottype==salinity || plottype==saldiff)) {
	if(DEBUG) showloadstats(data->timestep,plottype);

	data->salinity_step_loaded[proc]=data->timestep;
	fseek(fid,(nstep-1)*data->Nc[proc]*data->Nkmax*sizeof(double),0);
	for(i=0;i<data->Nkmax;i++) {
	  fread(dummy,sizeof(double),data->Nc[proc],fid);      
	  for(j=0;j<data->Nc[proc];j++) {
	    data->s[proc][i][j]=dummy[j];
	    if(dummy[j]!=EMPTY)
	      data->sd[proc][i][j]=dummy[j]-data->s0[proc][i][j];
	    else
	      data->sd[proc][i][j]=EMPTY;
	  }
	}
      }
      if(fid) fclose(fid);

      if(mergeArrays)
	GetFile(string,DATADIR,DATAFILE,"TemperatureFile",-1);
      else
	GetFile(string,DATADIR,DATAFILE,"TemperatureFile",proc);	
      fid = fopen(string,"r");
      if(fid && data->temperature_step_loaded[proc]!=data->timestep && plottype==temperature) {
	if(DEBUG) showloadstats(data->timestep,plottype); 

	data->temperature_step_loaded[proc]=data->timestep;
	fseek(fid,(nstep-1)*data->Nc[proc]*data->Nkmax*sizeof(double),0);
	for(i=0;i<data->Nkmax;i++) {
	  fread(dummy,sizeof(double),data->Nc[proc],fid);      
	  for(j=0;j<data->Nc[proc];j++) 
	    data->T[proc][i][j]=dummy[j];
	}
      }
      if(fid) fclose(fid);

      if(data->sedi==1){
	if(plottype==allsedic) {
	  nosize_start=1;
	  nosize_end=data->Nsize;
	} else {
	  nosize_start=data->sedinum;
	  nosize_end=data->sedinum;
	}
        for(nosize=nosize_start;nosize<=nosize_end;nosize++){
          sprintf(str,"Sediment%dFile",nosize);
	  if(mergeArrays)
	    GetFile(string,DATADIR,DATAFILE,str,-1);
	  else
	    GetFile(string,DATADIR,DATAFILE,str,proc);
          fid = fopen(string,"r");
          if(fid && data->sedi_step_loaded[nosize-1][proc]!=data->timestep && (plottype==sedic || plottype==allsedic)) {
	    if(DEBUG) printf("Loading sediment class %d of %d at step %d on proc %d\n",
			     nosize,data->Nsize,data->timestep,proc);

	    data->sedi_step_loaded[nosize-1][proc]=data->timestep;
	    fseek(fid,(nstep-1)*data->Nc[proc]*data->Nkmax*sizeof(double),0);
	    for(i=0;i<data->Nkmax;i++) {            
	      fread(dummy,sizeof(double),data->Nc[proc],fid);
	      for(j=0;j<data->Nc[proc];j++) {
                if(dummy[j]!=EMPTY)
	          data->sediC[proc][nosize-1][i][j]=dummy[j];
                else
                  data->sediC[proc][nosize-1][i][j]=EMPTY;
              }
            }
	  }
	  if(fid) fclose(fid);	  
	}

        // all the sediment concentration
        for(i=0;i<data->Nkmax;i++) {
          for(j=0;j<data->Nc[proc];j++){
            data->allsediC[proc][i][j]=0;
            if(data->sediC[proc][0][i][j]!=EMPTY)
              for(nosize=0;nosize<data->Nsize;nosize++)
                data->allsediC[proc][i][j]+=data->sediC[proc][nosize][i][j];
	    else
	      data->allsediC[proc][i][j]=EMPTY;
          }         
	}

        // layer file
	if(mergeArrays)
	  GetFile(string,DATADIR,DATAFILE,"LayerFile",-1);
	else
	  GetFile(string,DATADIR,DATAFILE,"LayerFile",proc);
        fid = fopen(string,"r");
	if(fid && data->layer_step_loaded[proc]!=data->timestep && plottype==layer) {
	  if(DEBUG) showloadstats(data->timestep,plottype);

	  data->layer_step_loaded[proc]=data->timestep;
          fseek(fid,(nstep-1)*data->Nc[proc]*sizeof(double),0);
	  fread(dummy,sizeof(double),data->Nc[proc],fid);      
	  for(i=0;i<data->Nc[proc];i++)
	    data->layerthickness[proc][i]=dummy[i]-data->initial_layerthickness[proc][i];
        }
	if(fid) fclose(fid);	  

        // bottom shear stress file
        if(data->tbmax==1){
	  if(mergeArrays)
	    GetFile(string,DATADIR,DATAFILE,"tbFile",-1);
	  else
	    GetFile(string,DATADIR,DATAFILE,"tbFile",proc);	    
	  fid = fopen(string,"r");
	  if(fid && data->taub_step_loaded[proc]!=data->timestep && plottype==taub) {
	    if(DEBUG) showloadstats(data->timestep,plottype);

	    data->taub_step_loaded[proc]=data->timestep;
	    fseek(fid,(nstep-1)*data->Nc[proc]*sizeof(double),0);
	    fread(dummy,sizeof(double),data->Nc[proc],fid);      
	    for(i=0;i<data->Nc[proc];i++){
	      data->taub[proc][i]=dummy[i];
	    }
	  }
	  if(fid) fclose(fid);	  
        }
      } else {
	// give initial value to make sure it can work even prop->sedi=0       
	for(j=0;j<data->Nc[proc];j++){
	  for(i=0;i<data->Nkmax;i++){
	    for(nosize=0;nosize<data->Nsize;nosize++)
	      data->sediC[proc][nosize][i][j]=0;         
	    data->allsediC[proc][i][j]=0;
	  }
	  data->layerthickness[proc][j]=0;
	  data->taub[proc][j]=0;
	}  
      }

      if(mergeArrays)
	GetFile(string,DATADIR,DATAFILE,"PressureFile",-1);
      else
	GetFile(string,DATADIR,DATAFILE,"PressureFile",proc);	
      fid = fopen(string,"r");
      if(fid && data->pressure_step_loaded[proc]!=data->timestep && plottype==pressure) {
	if(DEBUG) showloadstats(data->timestep,plottype);

	data->pressure_step_loaded[proc]=data->timestep;
	fseek(fid,(nstep-1)*data->Nc[proc]*data->Nkmax*sizeof(double),0);
	for(i=0;i<data->Nkmax;i++) {
	  fread(dummy,sizeof(double),data->Nc[proc],fid);      
	  for(j=0;j<data->Nc[proc];j++) 
	    data->q[proc][i][j]=dummy[j];
	}
      }
      if(fid) fclose(fid);

      if(mergeArrays)
	GetFile(string,DATADIR,DATAFILE,"HorizontalVelocityFile",-1);
      else
	GetFile(string,DATADIR,DATAFILE,"HorizontalVelocityFile",proc);	
      fid = fopen(string,"r");
      if(fid && data->velocity_step_loaded[proc]!=data->timestep && 
	 (plottype==u_velocity || plottype==v_velocity || plottype==w_velocity || vectorplot || 
	  plottype==u_baroclinic || plottype==v_baroclinic)) {

	if(DEBUG) showloadstats(data->timestep,plottype);

	data->velocity_step_loaded[proc]=data->timestep;
	fseek(fid,3*(nstep-1)*data->Nc[proc]*data->Nkmax*sizeof(double),0);
	for(i=0;i<data->Nkmax;i++) {
	  
	  //voronoi point u 
	  fread(dummy,sizeof(double),data->Nc[proc],fid);     
	  for(j=0;j<data->Nc[proc];j++) 
	    data->u[proc][i][j]=dummy[j];
	}
	for(i=0;i<data->Nkmax;i++) {	  
	  //voronoi point v 
	  fread(dummy,sizeof(double),data->Nc[proc],fid);     
	  for(j=0;j<data->Nc[proc];j++) 
	    data->v[proc][i][j]=dummy[j];
	}
	for(i=0;i<data->Nkmax;i++) {
	  //voronoi point w 
	  fread(dummy,sizeof(double),data->Nc[proc],fid);      
	  for(j=0;j<data->Nc[proc];j++) 
	    data->w[proc][i][j]=dummy[j];
	}

	for(i=0;i<data->Nkmax;i++) {
	  for(j=0;j<data->Nc[proc];j++) 
	    if(data->s0[proc][i][j]==EMPTY) {
	      data->u[proc][i][j]=EMPTY;
	      data->v[proc][i][j]=EMPTY;
	      data->w[proc][i][j]=EMPTY;
	    }
	}

	//      printf("Computing ubar and vbar at step %d, proc %d\n",nstep,proc);
	
	dz = data->dmax/data->Nkmax;
	for(j=0;j<data->Nc[proc];j++) {
	  data->ubar[proc][j]=0;
	  data->vbar[proc][j]=0;
	  for(i=0;i<data->Nkmax;i++) {
	    dz = data->z[i-1]-data->z[i];
	    if(data->s0[proc][i][j]!=EMPTY) {
	      data->ubar[proc][j]+=data->u[proc][i][j]*dz/data->depth[proc][j];
	      data->vbar[proc][j]+=data->v[proc][i][j]*dz/data->depth[proc][j];
	    }
	  }
	}
      }
      if(fid) fclose(fid);

      if(mergeArrays)
	GetFile(string,DATADIR,DATAFILE,"FreeSurfaceFile",-1);
      else
	GetFile(string,DATADIR,DATAFILE,"FreeSurfaceFile",proc);
      fid = fopen(string,"r");
      // Always load free-surface
      if(fid && data->freesurface_step_loaded[proc]!=data->timestep) {
	if(DEBUG) showloadstats(data->timestep,plottype);

	data->freesurface_step_loaded[proc]=data->timestep;
	fseek(fid,(nstep-1)*data->Nc[proc]*sizeof(double),0);
	fread(dummy,sizeof(double),data->Nc[proc],fid);      
	for(i=0;i<data->Nc[proc];i++)
	  data->h[proc][i]=dummy[i];
      }
      if(fid) fclose(fid);

      for(i=0;i<data->Nc[proc];i++) {
	data->h_d[proc][i]=data->h[proc][i]+data->depth[proc][i];
	if(data->h_d[proc][i]/data->depth[proc][i]<SMALLHEIGHT)
	  data->h_d[proc][i]=EMPTY;
      }

      turbmodel=(int)GetValue(DATAFILE,"turbmodel",&status);
      if(turbmodel) {
	if(mergeArrays)
	  GetFile(string,DATADIR,DATAFILE,"EddyViscosityFile",-1);
	else
	  GetFile(string,DATADIR,DATAFILE,"EddyViscosityFile",proc);	  
	fid = fopen(string,"r");
	if(fid && data->nut_step_loaded[proc]!=data->timestep && plottype==nut) {
	  if(DEBUG) showloadstats(data->timestep,plottype);

	  data->nut_step_loaded[proc]=data->timestep;
	  fseek(fid,(nstep-1)*data->Nc[proc]*data->Nkmax*sizeof(double),0);
	  for(i=0;i<data->Nkmax;i++) {
	    fread(dummy,sizeof(double),data->Nc[proc],fid);      
	    for(j=0;j<data->Nc[proc];j++) {
	      if(dummy[j]!=0)
		data->nut[proc][i][j]=log10(fabs(dummy[j]));
	      if(data->s[proc][i][j]==EMPTY)
		data->nut[proc][i][j]=EMPTY;
	    }
	  }
	}
	if(fid) fclose(fid);

	if(mergeArrays)
	  GetFile(string,DATADIR,DATAFILE,"ScalarDiffusivityFile",-1);
	else
	  GetFile(string,DATADIR,DATAFILE,"ScalarDiffusivityFile",proc);	  
	fid = fopen(string,"r");
	if(fid && data->kt_step_loaded[proc]!=data->timestep && plottype==kt) {
	  if(DEBUG) showloadstats(data->timestep,plottype);

	  data->kt_step_loaded[proc]=data->timestep;
	  fseek(fid,(nstep-1)*data->Nc[proc]*data->Nkmax*sizeof(double),0);
	  for(i=0;i<data->Nkmax;i++) {
	    fread(dummy,sizeof(double),data->Nc[proc],fid);      
	    for(j=0;j<data->Nc[proc];j++) {
	      if(dummy[j]!=0)
		data->kt[proc][i][j]=log10(fabs(dummy[j]));
	      if(data->s[proc][i][j]==EMPTY)
		data->kt[proc][i][j]=EMPTY;
	    }
	  }
	}
	if(fid) fclose(fid);
      }
      free(dummy);
    }
  }
}
    
void GetUMagMax(dataT *data, int klevel, int numprocs) {
  int j, i, proc;
  float umag, umagmax=0, ud, vd, hd;
  
  if(sliceType==slice && vertprofile) {
    for(i=0;i<data->Nslice;i++) 
      for(j=0;j<data->Nkmax;j++) {
	ud = data->sliceU[i][j];
	vd = data->sliceW[i][j];
	umag=sqrt(pow(ud,2)+pow(vd,2));
	if(ud != EMPTY && vd != EMPTY && umag>umagmax) umagmax=umag;
      }
    data->umagmax=umagmax;
  } else {//if(data->klevel!=klevel) {
    data->klevel=klevel;
    for(proc=0;proc<numprocs;proc++)  
      for(j=0;j<data->Nc[proc];j++) {
	ud = data->u[proc][klevel][j];
	vd = data->v[proc][klevel][j];
	hd = data->h[proc][j]+data->depth[proc][j];
	umag=sqrt(pow(ud,2)+pow(vd,2));
	if(ud != EMPTY && vd != EMPTY && hd>0.01*data->dmin && umag>umagmax) umagmax=umag;
      }
    data->umagmax=umagmax;
  }
}

void GetDMax(dataT *data, int numprocs) {
  int j, proc;
  float dmax=0;
  
  for(proc=0;proc<numprocs;proc++)  
    for(j=0;j<data->Nc[proc];j++) 
      if(data->depth[proc][j]>dmax) dmax=data->depth[proc][j];
  data->dmax=dmax;
}

void GetDMin(dataT *data, int numprocs) {
  int j, proc;
  float dmin=1e20;
  
  for(proc=0;proc<numprocs;proc++)  
    for(j=0;j<data->Nc[proc];j++) 
      if(data->depth[proc][j]<dmin) dmin=data->depth[proc][j];
  data->dmin=dmin;
}

void GetHMax(dataT *data, int numprocs) {
  int j, proc;
  float hmax=-INFTY, hmin=INFTY;
  
  for(proc=0;proc<numprocs;proc++)  
    for(j=0;j<data->Nc[proc];j++) {
      if(data->h[proc][j]>hmax) hmax=data->h[proc][j];
      if(data->h[proc][j]<hmin) hmin=data->h[proc][j];
    }
  data->hmax=hmax;
  data->hmin=hmin;
}

void GetSlice(dataT *data, int xs, int ys, int xe, int ye, 
	      int procnum, int numprocs, plottypeT plottype) {
  int i, nf, proc, ik, istart, pstart, iend, pend, numpoints, nk;
  float dz, dmax, xstart, ystart, xend, yend, rx0, ry0, 
    rx, ry, dist, xcent, ycent, rad, mag, mag0, ubar;

  if(!(xs==xe && ys==ye) && fromprofile && sliceType==slice) {

    FindNearest(data,xs,ys,&istart,&pstart,procnum,numprocs);
    FindNearest(data,xe,ye,&iend,&pend,procnum,numprocs);

    xstart = data->xv[pstart][istart];
    ystart = data->yv[pstart][istart];
    xend = data->xv[pend][iend];
    yend = data->yv[pend][iend];

    rx0 = xend-xstart;
    ry0 = yend-ystart;
    mag0 = sqrt(rx0*rx0+ry0*ry0);
    rx0 = rx0/mag0;
    ry0 = ry0/mag0;

    data->rx = rx0;
    data->ry = ry0;

    numpoints=0;
    for(proc=0;proc<numprocs;proc++) {
      for(i=0;i<data->Nc[proc];i++) {
	xcent=0;
	ycent=0;
	for(nf=0;nf<data->nfaces[proc][i];nf++) {
	  xcent+=data->xc[data->cells[proc][data->maxfaces*i+nf]]/data->nfaces[proc][i];
	  ycent+=data->yc[data->cells[proc][data->maxfaces*i+nf]]/data->nfaces[proc][i];
	}
	rad = sqrt(pow(data->xc[data->cells[proc][data->maxfaces*i]]-xcent,2)+
		   pow(data->yc[data->cells[proc][data->maxfaces*i]]-ycent,2));
	rx = (xcent-xstart);
	ry = (ycent-ystart);
	mag = rx*rx0+ry*ry0;
	dist = sqrt(rx*rx+ry*ry-mag*mag);
	if(dist<=rad && mag>=0 && mag<=mag0) {
	  data->sliceInd[numpoints]=i;
	  data->sliceProc[numpoints]=proc;
	  data->sliceX[numpoints++]=mag;
	}
      }
    }
    data->Nslice = numpoints;
    Sort(data->sliceInd,data->sliceProc,data->sliceX,data->Nslice);
    if(numpoints>NSLICEMAX) {
      printf("Error!  Too many points in slice.  Increase NSLICEMAX\n");
      exit(0);
    }

  } 

  if(data->Nslice>=NSLICEMIN) {
    for(i=0;i<data->Nslice;i++) {
      data->sliceH[i]=data->h[data->sliceProc[i]][data->sliceInd[i]];
      data->sliceD[i]=data->depth[data->sliceProc[i]][data->sliceInd[i]];
    }

    dz = data->z[0]-data->z[1];
    for(i=0;i<data->Nslice;i++) {
      for(ik=0;ik<data->Nkmax;ik++) {
	data->sliceU[i][ik]=data->rx*data->u[data->sliceProc[i]][ik][data->sliceInd[i]]+
	  data->ry*data->v[data->sliceProc[i]][ik][data->sliceInd[i]];
	data->sliceW[i][ik]=data->w[data->sliceProc[i]][ik][data->sliceInd[i]];
      }
      if(plottype==u_baroclinic || plottype==v_baroclinic) {
	ubar=0;
	nk=0;
	for(ik=0;ik<data->Nkmax;ik++) 
	  if(data->u[data->sliceProc[i]][ik][data->sliceInd[i]]!=EMPTY) {
	    nk++;
	    ubar+=data->sliceU[i][ik]*(data->z[i-1]-data->z[i]);
	  }
	for(ik=0;ik<data->Nkmax;ik++) 
	  data->sliceU[i][ik]-=ubar/data->sliceD[i];
      }
    }
    
    switch(plottype) {
    case salinity:
      for(i=0;i<data->Nslice;i++) 
	for(ik=0;ik<data->Nkmax;ik++) 
	  data->sliceData[i][ik]=data->s[data->sliceProc[i]][ik][data->sliceInd[i]];
      break;
    case nut:
      for(i=0;i<data->Nslice;i++) 
	for(ik=0;ik<data->Nkmax;ik++) 
	  data->sliceData[i][ik]=data->nut[data->sliceProc[i]][ik][data->sliceInd[i]];
      break;
    case kt:
      for(i=0;i<data->Nslice;i++) 
	for(ik=0;ik<data->Nkmax;ik++) 
	  data->sliceData[i][ik]=data->kt[data->sliceProc[i]][ik][data->sliceInd[i]];
      break;
    case saldiff:
      for(i=0;i<data->Nslice;i++) 
	for(ik=0;ik<data->Nkmax;ik++) 
	  data->sliceData[i][ik]=data->sd[data->sliceProc[i]][ik][data->sliceInd[i]];
      break;
    case salinity0:
      for(i=0;i<data->Nslice;i++) 
	for(ik=0;ik<data->Nkmax;ik++) 
	  data->sliceData[i][ik]=data->s0[data->sliceProc[i]][ik][data->sliceInd[i]];
      break;
    case temperature:
      for(i=0;i<data->Nslice;i++) 
	for(ik=0;ik<data->Nkmax;ik++) 
	  data->sliceData[i][ik]=data->T[data->sliceProc[i]][ik][data->sliceInd[i]];
      break;
    case allsedic:
      for(i=0;i<data->Nslice;i++) 
	for(ik=0;ik<data->Nkmax;ik++) 
	  data->sliceData[i][ik]=data->allsediC[data->sliceProc[i]][ik][data->sliceInd[i]];
      break;
    case sedic:
      for(i=0;i<data->Nslice;i++) 
	for(ik=0;ik<data->Nkmax;ik++) 
	  data->sliceData[i][ik]=data->sediC[data->sliceProc[i]][data->sedinum-1][ik][data->sliceInd[i]];
      break;    
    case pressure:
      for(i=0;i<data->Nslice;i++) 
	for(ik=0;ik<data->Nkmax;ik++) 
	  data->sliceData[i][ik]=data->q[data->sliceProc[i]][ik][data->sliceInd[i]];
      break;
    case u_velocity: 
      for(i=0;i<data->Nslice;i++) 
	for(ik=0;ik<data->Nkmax;ik++) 
	  data->sliceData[i][ik]=data->u[data->sliceProc[i]][ik][data->sliceInd[i]];
      break;
    case u_baroclinic:
      for(i=0;i<data->Nslice;i++) {
	nk=0;
	ubar=0;
	for(ik=0;ik<data->Nkmax;ik++) {
	  data->sliceData[i][ik]=data->u[data->sliceProc[i]][ik][data->sliceInd[i]];
	  if(data->sliceData[i][ik]!=EMPTY) {
	    nk++;
	    ubar+=data->sliceData[i][ik];
	  }
	}
	for(ik=0;ik<data->Nkmax;ik++) 
	  data->sliceData[i][ik]-=ubar/(float)nk;
      }
      break;
    case v_velocity:
      for(i=0;i<data->Nslice;i++) 
	for(ik=0;ik<data->Nkmax;ik++) 
	  data->sliceData[i][ik]=data->v[data->sliceProc[i]][ik][data->sliceInd[i]];
      break;
    case v_baroclinic:
      for(i=0;i<data->Nslice;i++) {
	nk=0;
	ubar=0;
	for(ik=0;ik<data->Nkmax;ik++) {
	  data->sliceData[i][ik]=data->v[data->sliceProc[i]][ik][data->sliceInd[i]];
	  if(data->sliceData[i][ik]!=EMPTY) {
	    nk++;
	    ubar+=data->sliceData[i][ik];
	  }
	}
	for(ik=0;ik<data->Nkmax;ik++) 
	  data->sliceData[i][ik]-=ubar/(float)nk;
      }
      break;
    case w_velocity:
      for(i=0;i<data->Nslice;i++) 
	for(ik=0;ik<data->Nkmax;ik++) 
	  data->sliceData[i][ik]=data->w[data->sliceProc[i]][ik][data->sliceInd[i]];
      break;
    default:
      for(i=0;i<data->Nslice;i++) 
	for(ik=0;ik<data->Nkmax;ik++) {
	  if(data->s[data->sliceProc[i]][ik][data->sliceInd[i]]!=EMPTY)
	    data->sliceData[i][ik]=1.0;
	  else
	    data->sliceData[i][ik]=EMPTY;
	}
      break;
    }
  } else {
    sliceType=none;
    vertprofile=false;
    edgelines=true;
    sprintf(message,"Cannot plot this slice!");
  }
}
    
void Sort(int *ind1, int *ind2, float *data, int N) {
  int i, j, ind, itmp;
  float ftmp;
  
  for(i=0;i<N;i++) 
    for(j=i;j<N;j++) 
      if(data[j]<data[i]) {
	ftmp = data[j];
	data[j] = data[i];
	data[i] = ftmp;

	itmp = ind1[j];
	ind1[j] = ind1[i];
	ind1[i] = itmp;
    
	itmp = ind2[j];
	ind2[j] = ind2[i];
	ind2[i] = itmp;
      }
}

void AllButtonsFalse(void) {
  int buttonnum;
  for(buttonnum=0;buttonnum<NUMBUTTONS;buttonnum++)
    controlButtons[buttonnum].status=false;
}    

int LoadCAxis(void) {
  int status;
  REAL caxis0,caxis1;

  caxis0=GetValue(DATAFILE,"caxismin",&status);  
  caxis1=GetValue(DATAFILE,"caxismax",&status);  

  if(!status) 
    printf("Error! Could not find values for caxismin or caxismax in %s (using min and max from data).\n",DATAFILE);
  else 
    caxis[0]=caxis0;
    caxis[1]=caxis1;
  
  return status;
}

int GetStep(dataT *data) {
  printf("Enter time step to plot: ");
  return GetNumber(1,data->noutput);
}

int GetLoc(dataT *data, int procnum, int numprocs) {
  int procstart, procend; 
  // get the processor info
  if(procplottype==allprocs) {
    procstart=0;
    procend=numprocs;
  } else {
    procstart=procnum;
    procend=procnum+1;
  }
  // get the particular processor from user
  printf("Get processor number = ");
  keyboardproc=GetNumber(procstart,procend);
  // get the particular cell from user
  printf("Get cell number = ");
  keyboardcell=GetNumber(0,data->Nc[keyboardproc]);

  printf("Selected value on proc %d and cell %d\n",
      keyboardproc, keyboardcell);
}

int GetNumber(int min, int max) {
  int num;

  fscanf(stdin,"%d",&num);
  while(num<min || num>max) {
    printf("Must be > %d and < %d\n",min-1,max+1);
    printf("Please try again: ");
    fscanf(stdin,"%d",&num);
  }

  return num;
}
    
void GetCAxes(REAL datamin, REAL datamax) {
  printf("Enter Color Axes: ");
  fscanf(stdin,"%f %f",&caxis[0],&caxis[1]);

  while((caxis[0]<datamin && caxis[1]>datamax) || caxis[0]==caxis[1]) {
    if(caxis[0]==caxis[1])
      printf("Color axes limits may not be equal.  Try again: ");
    else if(caxis[0]<datamin && caxis[1]>datamax)
      printf("Axes must be in the range [%.2e, %.2e].  Try again: ",datamin,datamax);

    fscanf(stdin,"%f %f",&caxis[0],&caxis[1]);
  }
}

int GetNumOutput(int ntout) {
  int numoutput=0;
  char str[BUFFERLENGTH], str2[BUFFERLENGTH];
  FILE *fid;

  sprintf(str2,"%s/step.dat",DATADIR);
  fid=fopen(str2,"r");
  if(fid) {
    fscanf(fid,"%s %d",str,&numoutput);
    fclose(fid);
  }
  
  if(numoutput) {
    numoutput=1+(int)((float)numoutput/(float)ntout);
    if(numoutput==0)
      numoutput=1;
  } else 
    numoutput=-1;

  return numoutput;
}

void DrawBoundaries(float *xc, float *yc, int *edges, plottypeT plottype, int *mark, int Ne) {
  int j, m;
  XPoint *vertices = (XPoint *)malloc(2*sizeof(XPoint)); 

  for(j=0;j<Ne;j++) {
    if(mark[j]==1 || mark[j]==2 || mark[j]==3) {
      for(m=0;m<2;m++) {
	vertices[m].x = axesPosition[2]*width*(xc[edges[2*j+m]]-dataLimits[0])/
	  (dataLimits[1]-dataLimits[0]);
	vertices[m].y = axesPosition[3]*height*(1-(yc[edges[2*j+m]]-dataLimits[2])/
						(dataLimits[3]-dataLimits[2]));
      }
      
      if(mark[j]==1)
	XSetForeground(dis,gc,white);
      else if(mark[j]==2)
	XSetForeground(dis,gc,red);	
      else if(mark[j]==3)
	XSetForeground(dis,gc,blue);		
      XDrawLines(dis,pix,gc,vertices,2,0);
    }
  }
  free(vertices);
}

void showloadstats(int step, plottypeT plottype) {

  switch(plottype) {
  case noplottype:
    printf("Loading step %d for type: noplottype\n",step);
    break;
  case freesurface:
    printf("Loading step %d for type: freesurface\n",step);
    break;
  case depth:
    printf("Loading step %d for type: depth\n",step);
    break;
  case h_d:
    printf("Loading step %d for type: h_d\n",step);
    break;
  case salinity:
    printf("Loading step %d for type: salinity\n",step);
    break;
  case temperature:
    printf("Loading step %d for type: temperature\n",step);
    break;
  case pressure:
    printf("Loading step %d for type: pressure\n",step);
    break;
  case saldiff:
    printf("Loading step %d for type: saldiff\n",step);
    break;
  case sedic:
    printf("Loading step %d for type: sedic\n",step);
    break;
  case allsedic:
    printf("Loading step %d for type: allsedic\n",step);
    break;
  case layer:
    printf("Loading step %d for type: layer\n",step);
    break;
  case taub:
    printf("Loading step %d for type: taub\n",step);
    break;
  case salinity0:
    printf("Loading step %d for type: salinity0\n",step);
    break;
  case u_velocity:
    printf("Loading step %d for type: u_velocity\n",step);
    break;
  case v_velocity:
    printf("Loading step %d for type: v_velocity\n",step);
    break;
  case w_velocity:
    printf("Loading step %d for type: w_velocity\n",step);
    break;
  case u_baroclinic:
    printf("Loading step %d for type: u_baroclinic\n",step);
    break;
  case v_baroclinic:
    printf("Loading step %d for type: v_baroclinic\n",step);
    break;
  case  nut:
    printf("Loading step %d for type:  nut\n",step);
    break;
  case  kt:
    printf("Loading step %d for type:  kt\n",step);
    break;
  default:
    break;
  }
}

float GetDmaxSlice(float *depth, int N) {
  int i;
  float dmax=0;
  for(i=0;i<N;i++)
    if(depth[i]>dmax)
      dmax=depth[i];

  return dmax;
}
