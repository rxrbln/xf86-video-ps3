/*
 * Authors:  Alan Hourihane, <alanh@fairlite.demon.co.uk>
 *	     Michel Dänzer, <michel@tungstengraphics.com>
 *
 * PS3 Modifications (c) Vivien Chappelier (vivien.chappelier@free.fr)
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

/* all driver need this */
#include "xf86.h"
#include "xf86_OSproc.h"

#include "mipointer.h"
#include "micmap.h"
#include "colormapst.h"
#include "xf86cmap.h"
#include "shadow.h"
#include "dgaproc.h"

/* for visuals */
#include "fb.h"

#if GET_ABI_MAJOR(ABI_VIDEODRV_VERSION) < 6
#include "xf86Resources.h"
#include "xf86RAC.h"
#endif

#include "fbdevhw.h"

#include "xf86xv.h"
#include "exa.h"

#include "ps3.h"
#include "ps3_gpu.h"

static Bool debug = 0;

#define TRACE_ENTER(str) \
    do { if (debug) ErrorF("ps3: " str " %d\n",pScrn->scrnIndex); } while (0)
#define TRACE_EXIT(str) \
    do { if (debug) ErrorF("ps3: " str " done\n"); } while (0)
#define TRACE(str) \
    do { if (debug) ErrorF("ps3 trace: " str "\n"); } while (0)

/* -------------------------------------------------------------------- */
/* prototypes                                                           */

static const OptionInfoRec * PS3AvailableOptions(int chipid, int busid);
static void	PS3Identify(int flags);
static Bool	PS3Probe(DriverPtr drv, int flags);
static Bool	PS3PreInit(ScrnInfoPtr pScrn, int flags);
static Bool	PS3ScreenInit(int Index, ScreenPtr pScreen, int argc,
				char **argv);
static Bool	PS3CloseScreen(int scrnIndex, ScreenPtr pScreen);
static Bool	PS3DriverFunc(ScrnInfoPtr pScrn, xorgDriverFuncOp op,
				pointer ptr);

/* -------------------------------------------------------------------- */

/*
 * This is intentionally screen-independent.  It indicates the binding
 * choice made in the first PreInit.
 */
static int pix24bpp = 0;

#define PS3_VERSION		4000
#define PS3_NAME		"PS3"
#define PS3_DRIVER_NAME	"ps3"

_X_EXPORT DriverRec PS3 = {
	PS3_VERSION,
	PS3_DRIVER_NAME,
	PS3Identify,
	PS3Probe,
	PS3AvailableOptions,
	NULL,
	0,
	PS3DriverFunc,
};

/* Supported "chipsets" */
static SymTabRec PS3Chipsets[] = {
    { 0, "ps3" },
    {-1, NULL }
};

/* Supported options */
typedef enum {
	OPTION_NOACCEL,
	OPTION_FBDEV,
	OPTION_DEBUG
} PS3Opts;

static const OptionInfoRec PS3Options[] = {
	{ OPTION_NOACCEL,       "NoAccel",      OPTV_BOOLEAN,   {0},    FALSE },
	{ OPTION_FBDEV,		"fbdev",	OPTV_STRING,	{0},	FALSE },
	{ OPTION_DEBUG,		"debug",	OPTV_BOOLEAN,	{0},	FALSE },
	{ -1,			NULL,		OPTV_NONE,	{0},	FALSE }
};

/* -------------------------------------------------------------------- */

#ifdef XFree86LOADER

MODULESETUPPROTO(ps3Setup);

static XF86ModuleVersionInfo ps3VersRec =
{
	"ps3",
	MODULEVENDORSTRING,
	MODINFOSTRING1,
	MODINFOSTRING2,
	XORG_VERSION_CURRENT,
	PACKAGE_VERSION_MAJOR, PACKAGE_VERSION_MINOR, PACKAGE_VERSION_PATCHLEVEL,
	ABI_CLASS_VIDEODRV,
	ABI_VIDEODRV_VERSION,
	NULL,
	{0,0,0,0}
};

_X_EXPORT XF86ModuleData ps3ModuleData = { &ps3VersRec, ps3Setup, NULL };

pointer
ps3Setup(pointer module, pointer opts, int *errmaj, int *errmin)
{
	static Bool setupDone = FALSE;

	if (!setupDone) {
		setupDone = TRUE;
		xf86AddDriver(&PS3, module, HaveDriverFuncs);
		return (pointer)1;
	} else {
		if (errmaj) *errmaj = LDR_ONCEONLY;
		return NULL;
	}
}

#endif /* XFree86LOADER */

/* -------------------------------------------------------------------- */
/* our private data, and two functions to allocate/free this            */

static Bool
PS3GetRec(ScrnInfoPtr pScrn)
{
	if (pScrn->driverPrivate != NULL)
		return TRUE;
	
	pScrn->driverPrivate = xnfcalloc(sizeof(PS3Rec), 1);
	return TRUE;
}

static void
PS3FreeRec(ScrnInfoPtr pScrn)
{
	if (pScrn->driverPrivate == NULL)
		return;
	free(pScrn->driverPrivate);
	pScrn->driverPrivate = NULL;
}

/* -------------------------------------------------------------------- */

static const OptionInfoRec *
PS3AvailableOptions(int chipid, int busid)
{
	return PS3Options;
}

static void
PS3Identify(int flags)
{
	xf86PrintChipsets(PS3_NAME, "driver for framebuffer", PS3Chipsets);
}


static Bool
PS3Probe(DriverPtr drv, int flags)
{
	int i;
	ScrnInfoPtr pScrn;
       	GDevPtr *devSections;
	int numDevSections;
	int bus,device,func;
	char *dev;
	Bool foundScreen = FALSE;


	TRACE("probe start");

	/* For now, just bail out for PROBE_DETECT. */
	if (flags & PROBE_DETECT)
		return FALSE;

	if ((numDevSections = xf86MatchDevice(PS3_DRIVER_NAME, &devSections)) <= 0) 
	    return FALSE;

	if (!xf86LoadDrvSubModule(drv, "fbdevhw"))
	    return FALSE;

	for (i = 0; i < numDevSections; i++) {

	    dev = xf86FindOptionValue(devSections[i]->options,"fbdev");

	    if (fbdevHWProbe(NULL, dev, NULL)) {
		    int entity;

		    pScrn = NULL;

		    entity = xf86ClaimFbSlot(drv, 0,
					      devSections[i], TRUE);
		    pScrn = xf86ConfigFbEntity(pScrn,0,entity,
					       NULL,NULL,NULL,NULL);


		    if (pScrn) {
			    foundScreen = TRUE;
		    
			    pScrn->driverVersion = PS3_VERSION;
			    pScrn->driverName    = PS3_DRIVER_NAME;
			    pScrn->name          = PS3_NAME;
			    pScrn->Probe         = PS3Probe;
			    pScrn->PreInit       = PS3PreInit;
			    pScrn->ScreenInit    = PS3ScreenInit;
			    pScrn->SwitchMode    = fbdevHWSwitchModeWeak();
			    pScrn->AdjustFrame   = fbdevHWAdjustFrameWeak();
			    pScrn->EnterVT       = fbdevHWEnterVTWeak();
			    pScrn->LeaveVT       = fbdevHWLeaveVTWeak();
			    pScrn->ValidMode     = fbdevHWValidModeWeak();
		    
			    xf86DrvMsg(pScrn->scrnIndex, X_INFO,
				       "using %s\n",
				       dev ? dev : "default device");
		    }
	    }
	}
	free(devSections);
	TRACE("probe done");
	return foundScreen;
}

static Bool
PS3PreInit(ScrnInfoPtr pScrn, int flags)
{
	PS3Ptr pPS3;
	int default_depth, fbbpp;
	const char *mod = NULL, *s;
	const char **syms = NULL;
	int type;

	if (flags & PROBE_DETECT) return FALSE;

	TRACE_ENTER("PreInit");

	/* Check the number of entities, and fail if it isn't one. */
	if (pScrn->numEntities != 1)
		return FALSE;

	pScrn->monitor = pScrn->confScreen->monitor;

	PS3GetRec(pScrn);
	pPS3 = PS3PTR(pScrn);

	pPS3->pEnt = xf86GetEntityInfo(pScrn->entityList[0]);

#if GET_ABI_MAJOR(ABI_VIDEODRV_VERSION) < 6
	pScrn->racMemFlags = RAC_FB | RAC_COLORMAP | RAC_CURSOR | RAC_VIEWPORT;
	/* XXX Is this right?  Can probably remove RAC_FB */
	pScrn->racIoFlags = RAC_FB | RAC_COLORMAP | RAC_CURSOR | RAC_VIEWPORT;
#endif

	/* open device */
	if (!fbdevHWInit(pScrn,NULL,xf86FindOptionValue(pPS3->pEnt->device->options,"fbdev")))
		return FALSE;
	default_depth = fbdevHWGetDepth(pScrn,&fbbpp);
	if (!xf86SetDepthBpp(pScrn, default_depth, default_depth, fbbpp,
			     Support32bppFb))
		return FALSE;
	xf86PrintDepthBpp(pScrn);

	/* color weight */
	{
		rgb zeros = { 0, 0, 0 };
		if (!xf86SetWeight(pScrn, zeros, zeros))
			return FALSE;
	}

	/* visual init */
	if (!xf86SetDefaultVisual(pScrn, -1))
		return FALSE;

	/* We don't currently support DirectColor at > 8bpp */
	if (pScrn->defaultVisual != TrueColor) {
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "requested default visual"
			   " (%s) is not supported at depth %d\n",
			   xf86GetVisualName(pScrn->defaultVisual), pScrn->depth);
		return FALSE;
	}

	{
		Gamma zeros = {0.0, 0.0, 0.0};

		if (!xf86SetGamma(pScrn,zeros)) {
			return FALSE;
		}
	}

	pScrn->progClock = TRUE;
	pScrn->rgbBits   = 8;
	pScrn->chipset   = "ps3";
	pScrn->videoRam  = fbdevHWGetVidmem(pScrn);

	xf86DrvMsg(pScrn->scrnIndex, X_INFO, "hardware: %s (video memory:"
		   " %dkB)\n", fbdevHWGetName(pScrn), pScrn->videoRam/1024);

	/* handle options */
	xf86CollectOptions(pScrn, NULL);
	if (!(pPS3->Options = malloc(sizeof(PS3Options))))
		return FALSE;
	memcpy(pPS3->Options, PS3Options, sizeof(PS3Options));
	xf86ProcessOptions(pScrn->scrnIndex, pPS3->pEnt->device->options, pPS3->Options);

	debug = xf86ReturnOptValBool(pPS3->Options, OPTION_DEBUG, FALSE);

	if (xf86ReturnOptValBool(pPS3->Options, OPTION_NOACCEL, FALSE)) {
		pPS3->NoAccel = TRUE;
		xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "Acceleration disabled\n");
	} else
		pPS3->NoAccel = FALSE;

	/* select video modes */

	xf86DrvMsg(pScrn->scrnIndex, X_INFO, "checking modes against framebuffer device...\n");
	fbdevHWSetVideoModes(pScrn);

	xf86DrvMsg(pScrn->scrnIndex, X_INFO, "checking modes against monitor...\n");
	{
		DisplayModePtr mode, first = mode = pScrn->modes;
		
		if (mode != NULL) do {
			mode->status = xf86CheckModeForMonitor(mode, pScrn->monitor);
			mode = mode->next;
		} while (mode != NULL && mode != first);

		xf86PruneDriverModes(pScrn);
	}

	if (NULL == pScrn->modes)
		fbdevHWUseBuildinMode(pScrn);
	pScrn->currentMode = pScrn->modes;

	/* First approximation, may be refined in ScreenInit */
	pScrn->displayWidth = pScrn->virtualX;

	xf86PrintModes(pScrn);

	/* Set display resolution */
	xf86SetDpi(pScrn, 0, 0);

	/* Load FB */
	if (xf86LoadSubModule(pScrn, "fb") == NULL) {
		PS3FreeRec(pScrn);
		return FALSE;
	}

	/* Load EXA */
	if (!pPS3->NoAccel) {
		if (!xf86LoadSubModule(pScrn, "exa")) {
			xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
				   "EXA module not found\n");
			PS3FreeRec(pScrn);
			return FALSE;
		}
	}

	TRACE_EXIT("PreInit");
	return TRUE;
}


static Bool
PS3CreateScreenResources(ScreenPtr pScreen)
{
    PixmapPtr pPixmap;
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
    PS3Ptr pPS3 = PS3PTR(pScrn);
    Bool ret;

    pScreen->CreateScreenResources = pPS3->CreateScreenResources;
    ret = pScreen->CreateScreenResources(pScreen);
    pScreen->CreateScreenResources = PS3CreateScreenResources;

    if (!ret)
	return FALSE;

    pPixmap = pScreen->GetScreenPixmap(pScreen);

    return TRUE;
}

static Bool
PS3ScreenInit(int scrnIndex, ScreenPtr pScreen, int argc, char **argv)
{
	ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
	PS3Ptr pPS3 = PS3PTR(pScrn);
	VisualPtr visual;
	int init_picture = 0;
	int ret, flags;
	int type;

	TRACE_ENTER("PS3ScreenInit");

#if DEBUG
	ErrorF("\tbitsPerPixel=%d, depth=%d, defaultVisual=%s\n"
	       "\tmask: %x,%x,%x, offset: %d,%d,%d\n",
	       pScrn->bitsPerPixel,
	       pScrn->depth,
	       xf86GetVisualName(pScrn->defaultVisual),
	       pScrn->mask.red,pScrn->mask.green,pScrn->mask.blue,
	       pScrn->offset.red,pScrn->offset.green,pScrn->offset.blue);
#endif

	if (NULL == (pPS3->fbmem = fbdevHWMapVidmem(pScrn))) {
	        xf86DrvMsg(scrnIndex,X_ERROR,"mapping of video memory"
			   " failed\n");
		return FALSE;
	}

	pPS3->fboff = fbdevHWLinearOffset(pScrn);

	PS3GpuInit(pPS3);

	fbdevHWSave(pScrn);

	if (!fbdevHWModeInit(pScrn, pScrn->currentMode)) {
		xf86DrvMsg(scrnIndex,X_ERROR,"mode initialization failed\n");
		return FALSE;
	}
	fbdevHWSaveScreen(pScreen, SCREEN_SAVER_ON);
	fbdevHWAdjustFrame(scrnIndex,0,0);

	/* mi layer */
	miClearVisualTypes();
	if (!miSetVisualTypes(pScrn->depth, TrueColorMask, pScrn->rgbBits, TrueColor)) {
		xf86DrvMsg(scrnIndex,X_ERROR,"visual type setup failed"
			   " for %d bits per pixel [1]\n",
			   pScrn->bitsPerPixel);
		return FALSE;
	}
	if (!miSetPixmapDepths()) {
	  xf86DrvMsg(scrnIndex,X_ERROR,"pixmap depth setup failed\n");
	  return FALSE;
	}

	/* FIXME: this doesn't work for all cases, e.g. when each scanline
	   has a padding which is independent from the depth (controlfb) */
	pScrn->displayWidth = fbdevHWGetLineLength(pScrn) /
		(pScrn->bitsPerPixel / 8);

	if (pScrn->displayWidth != pScrn->virtualX) {
		xf86DrvMsg(scrnIndex, X_INFO,
			   "Pitch updated to %d after ModeInit\n",
			   pScrn->displayWidth);
	}

	pPS3->fbstart = pPS3->fbmem + pPS3->fboff;

	if (pScrn->bitsPerPixel == 32) {
		ret = fbScreenInit(pScreen, pPS3->fbstart, pScrn->virtualX,
				   pScrn->virtualY, pScrn->xDpi,
				   pScrn->yDpi, pScrn->displayWidth,
				   pScrn->bitsPerPixel);
		init_picture = 1;
	} else {
		xf86DrvMsg(scrnIndex, X_ERROR,
			   "internal error: invalid number of bits per"
			   " pixel (%d) encountered in"
			   " PS3ScreenInit()\n", pScrn->bitsPerPixel);
		ret = FALSE;
	}

	if (!ret)
		return FALSE;

	/* Fixup RGB ordering */
	visual = pScreen->visuals + pScreen->numVisuals;
	while (--visual >= pScreen->visuals) {
		if ((visual->class | DynamicClass) == DirectColor) {
			visual->offsetRed   = pScrn->offset.red;
			visual->offsetGreen = pScrn->offset.green;
			visual->offsetBlue  = pScrn->offset.blue;
			visual->redMask     = pScrn->mask.red;
			visual->greenMask   = pScrn->mask.green;
			visual->blueMask    = pScrn->mask.blue;
		}
	}

	/* must be after RGB ordering fixed */
	if (init_picture && !fbPictureInit(pScreen, NULL, 0))
		xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
			   "Render extension initialisation failed\n");

	xf86SetBlackWhitePixels(pScreen);

	if (!pPS3->NoAccel) {
		if (!PS3InitDma(pScrn)) {
			xf86DrvMsg(scrnIndex,X_ERROR, "DMA init failed\n");
			return FALSE;
		}

		PS3ExaInit(pScreen);
	}

	xf86SetBackingStore(pScreen);

	/* software cursor */
	miDCInitialize(pScreen, xf86GetPointerScreenFuncs());

	if (!miCreateDefColormap(pScreen)) {
		xf86DrvMsg(scrnIndex, X_ERROR,
			   "internal error: miCreateDefColormap failed "
			   "in PS3ScreenInit()\n");
		return FALSE;
	}

	flags = CMAP_PALETTED_TRUECOLOR;
	if(!xf86HandleColormaps(pScreen, 256, 8, fbdevHWLoadPaletteWeak(), 
				NULL, flags))
		return FALSE;

	xf86DPMSInit(pScreen, fbdevHWDPMSSetWeak(), 0);

	PS3InitVideo(pScreen);
	
	pScreen->SaveScreen = fbdevHWSaveScreenWeak();

	/* Wrap the current CloseScreen function */
	pPS3->CloseScreen = pScreen->CloseScreen;
	pScreen->CloseScreen = PS3CloseScreen;

	{
	    XF86VideoAdaptorPtr *ptr;

	    int n = xf86XVListGenericAdaptors(pScrn,&ptr);
	    if (n) {
		xf86XVScreenInit(pScreen,ptr,n);
	    }
	}

	TRACE_EXIT("PS3ScreenInit");

	return TRUE;
}

static Bool
PS3CloseScreen(int scrnIndex, ScreenPtr pScreen)
{
	ScrnInfoPtr pScrn = xf86Screens[scrnIndex];
	PS3Ptr pPS3 = PS3PTR(pScrn);
	
	fbdevHWRestore(pScrn);
	fbdevHWUnmapVidmem(pScrn);

	PS3GpuCleanup(pPS3);

	pScrn->vtSema = FALSE;

	pScreen->CreateScreenResources = pPS3->CreateScreenResources;
	pScreen->CloseScreen = pPS3->CloseScreen;
	return (*pScreen->CloseScreen)(scrnIndex, pScreen);
}

static Bool
PS3DriverFunc(ScrnInfoPtr pScrn, xorgDriverFuncOp op, pointer ptr)
{
    xorgHWFlags *flag;
    
    switch (op) {
	case GET_REQUIRED_HW_INTERFACES:
	    flag = (CARD32*)ptr;
	    (*flag) = 0;
	    return TRUE;
	default:
	    return FALSE;
    }
}
