/*
 * Authors:  Alan Hourihane, <alanh@fairlite.demon.co.uk>
 *	     Michel Dänzer, <michel@tungstengraphics.com>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

/* all driver need this */
#include "xf86.h"
#include "xf86_OSproc.h"

#include "mipointer.h"
#include "mibstore.h"
#include "micmap.h"
#include "colormapst.h"
#include "xf86cmap.h"
#include "shadow.h"
#include "dgaproc.h"

/* for visuals */
#include "fb.h"
#ifdef USE_AFB
#include "afb.h"
#endif

#include "xf86Resources.h"
#include "xf86RAC.h"

#include "fbdevhw.h"

#include "xf86xv.h"

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

static const OptionInfoRec * Ps3AvailableOptions(int chipid, int busid);
static void	Ps3Identify(int flags);
static Bool	Ps3Probe(DriverPtr drv, int flags);
static Bool	Ps3PreInit(ScrnInfoPtr pScrn, int flags);
static Bool	Ps3ScreenInit(int Index, ScreenPtr pScreen, int argc,
				char **argv);
static Bool	Ps3CloseScreen(int scrnIndex, ScreenPtr pScreen);
static Bool	Ps3DriverFunc(ScrnInfoPtr pScrn, xorgDriverFuncOp op,
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
	Ps3Identify,
	Ps3Probe,
	Ps3AvailableOptions,
	NULL,
	0,
	Ps3DriverFunc,
};

/* Supported "chipsets" */
static SymTabRec Ps3Chipsets[] = {
    { 0, "ps3" },
    {-1, NULL }
};

/* Supported options */
typedef enum {
	OPTION_FBDEV,
	OPTION_DEBUG
} Ps3Opts;

static const OptionInfoRec Ps3Options[] = {
	{ OPTION_FBDEV,		"fbdev",	OPTV_STRING,	{0},	FALSE },
	{ OPTION_DEBUG,		"debug",	OPTV_BOOLEAN,	{0},	FALSE },
	{ -1,			NULL,		OPTV_NONE,	{0},	FALSE }
};

/* -------------------------------------------------------------------- */

static const char *fbSymbols[] = {
	"fbScreenInit",
	"fbPictureInit",
	NULL
};

static const char *fbdevHWSymbols[] = {
	"fbdevHWInit",
	"fbdevHWProbe",
	"fbdevHWSetVideoModes",
	"fbdevHWUseBuildinMode",

	"fbdevHWGetDepth",
	"fbdevHWGetLineLength",
	"fbdevHWGetName",
	"fbdevHWGetType",
	"fbdevHWGetVidmem",
	"fbdevHWLinearOffset",
	"fbdevHWLoadPalette",
	"fbdevHWMapVidmem",
	"fbdevHWUnmapVidmem",

	/* colormap */
	"fbdevHWLoadPalette",
	"fbdevHWLoadPaletteWeak",

	/* ScrnInfo hooks */
	"fbdevHWAdjustFrameWeak",
	"fbdevHWEnterVTWeak",
	"fbdevHWLeaveVTWeak",
	"fbdevHWModeInit",
	"fbdevHWRestore",
	"fbdevHWSave",
	"fbdevHWSaveScreen",
	"fbdevHWSaveScreenWeak",
	"fbdevHWSwitchModeWeak",
	"fbdevHWValidModeWeak",

	"fbdevHWDPMSSet",
	"fbdevHWDPMSSetWeak",

	NULL
};

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
		LoaderRefSymLists(fbSymbols, fbdevHWSymbols, NULL);
		return (pointer)1;
	} else {
		if (errmaj) *errmaj = LDR_ONCEONLY;
		return NULL;
	}
}

#endif /* XFree86LOADER */

/* -------------------------------------------------------------------- */
/* our private data, and two functions to allocate/free this            */

typedef struct {
	Ps3GpuPtr			gpu;
	unsigned char*			fbstart;
	unsigned char*			fbmem;
	int				fboff;
	int				lineLength;
	CloseScreenProcPtr		CloseScreen;
	CreateScreenResourcesProcPtr	CreateScreenResources;
	EntityInfoPtr			pEnt;
	OptionInfoPtr			Options;
} Ps3Rec, *Ps3Ptr;

#define PS3PTR(p) ((Ps3Ptr)((p)->driverPrivate))

static Bool
Ps3GetRec(ScrnInfoPtr pScrn)
{
	if (pScrn->driverPrivate != NULL)
		return TRUE;
	
	pScrn->driverPrivate = xnfcalloc(sizeof(Ps3Rec), 1);
	return TRUE;
}

static void
Ps3FreeRec(ScrnInfoPtr pScrn)
{
	if (pScrn->driverPrivate == NULL)
		return;
	xfree(pScrn->driverPrivate);
	pScrn->driverPrivate = NULL;
}

/* -------------------------------------------------------------------- */

static const OptionInfoRec *
Ps3AvailableOptions(int chipid, int busid)
{
	return Ps3Options;
}

static void
Ps3Identify(int flags)
{
	xf86PrintChipsets(PS3_NAME, "driver for framebuffer", Ps3Chipsets);
}


static Bool
Ps3Probe(DriverPtr drv, int flags)
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
	    
	xf86LoaderReqSymLists(fbdevHWSymbols, NULL);

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
			    pScrn->Probe         = Ps3Probe;
			    pScrn->PreInit       = Ps3PreInit;
			    pScrn->ScreenInit    = Ps3ScreenInit;
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
	xfree(devSections);
	TRACE("probe done");
	return foundScreen;
}

static Bool
Ps3PreInit(ScrnInfoPtr pScrn, int flags)
{
	Ps3Ptr fPtr;
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

	Ps3GetRec(pScrn);
	fPtr = PS3PTR(pScrn);

	fPtr->pEnt = xf86GetEntityInfo(pScrn->entityList[0]);

	pScrn->racMemFlags = RAC_FB | RAC_COLORMAP | RAC_CURSOR | RAC_VIEWPORT;
	/* XXX Is this right?  Can probably remove RAC_FB */
	pScrn->racIoFlags = RAC_FB | RAC_COLORMAP | RAC_CURSOR | RAC_VIEWPORT;

	/* open device */
	if (!fbdevHWInit(pScrn,NULL,xf86FindOptionValue(fPtr->pEnt->device->options,"fbdev")))
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
	if (!(fPtr->Options = xalloc(sizeof(Ps3Options))))
		return FALSE;
	memcpy(fPtr->Options, Ps3Options, sizeof(Ps3Options));
	xf86ProcessOptions(pScrn->scrnIndex, fPtr->pEnt->device->options, fPtr->Options);

	debug = xf86ReturnOptValBool(fPtr->Options, OPTION_DEBUG, FALSE);

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

	/* Load bpp-specific modules */
	if (pScrn->bitsPerPixel == 32) {
		mod = "fb";
		syms = fbSymbols;
	} else {
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
			   "unsupported number of bits per pixel: %d",
			   pScrn->bitsPerPixel);
		return FALSE;
	}

	if (mod && xf86LoadSubModule(pScrn, mod) == NULL) {
		Ps3FreeRec(pScrn);
		return FALSE;
	}
	if (mod && syms) {
		xf86LoaderReqSymLists(syms, NULL);
	}


	TRACE_EXIT("PreInit");
	return TRUE;
}


static Bool
Ps3CreateScreenResources(ScreenPtr pScreen)
{
    PixmapPtr pPixmap;
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
    Ps3Ptr fPtr = PS3PTR(pScrn);
    Bool ret;

    pScreen->CreateScreenResources = fPtr->CreateScreenResources;
    ret = pScreen->CreateScreenResources(pScreen);
    pScreen->CreateScreenResources = Ps3CreateScreenResources;

    if (!ret)
	return FALSE;

    pPixmap = pScreen->GetScreenPixmap(pScreen);

    return TRUE;
}

static Bool
Ps3ScreenInit(int scrnIndex, ScreenPtr pScreen, int argc, char **argv)
{
	ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
	Ps3Ptr fPtr = PS3PTR(pScrn);
	VisualPtr visual;
	int init_picture = 0;
	int ret, flags;
	int type;

	TRACE_ENTER("Ps3ScreenInit");

#if DEBUG
	ErrorF("\tbitsPerPixel=%d, depth=%d, defaultVisual=%s\n"
	       "\tmask: %x,%x,%x, offset: %d,%d,%d\n",
	       pScrn->bitsPerPixel,
	       pScrn->depth,
	       xf86GetVisualName(pScrn->defaultVisual),
	       pScrn->mask.red,pScrn->mask.green,pScrn->mask.blue,
	       pScrn->offset.red,pScrn->offset.green,pScrn->offset.blue);
#endif

// TEMP
	ErrorF("Ps3GpuInit\n");
	fPtr->gpu = Ps3GpuInit();

	if (NULL == (fPtr->fbmem = fbdevHWMapVidmem(pScrn))) {
	        xf86DrvMsg(scrnIndex,X_ERROR,"mapping of video memory"
			   " failed\n");
		return FALSE;
	}
	fPtr->fboff = fbdevHWLinearOffset(pScrn);

// TEMP
	fPtr->fbmem = fPtr->gpu->vram_base;

	fbdevHWSave(pScrn);

	if (!fbdevHWModeInit(pScrn, pScrn->currentMode)) {
		xf86DrvMsg(scrnIndex,X_ERROR,"mode initialization failed\n");
		return FALSE;
	}
	fbdevHWSaveScreen(pScreen, SCREEN_SAVER_ON);
	fbdevHWAdjustFrame(scrnIndex,0,0,0);

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

	fPtr->fbstart = fPtr->fbmem + fPtr->fboff;

	if (pScrn->bitsPerPixel == 32) {
		ret = fbScreenInit(pScreen, fPtr->fbstart, pScrn->virtualX,
				   pScrn->virtualY, pScrn->xDpi,
				   pScrn->yDpi, pScrn->displayWidth,
				   pScrn->bitsPerPixel);
		init_picture = 1;
	} else {
		xf86DrvMsg(scrnIndex, X_ERROR,
			   "internal error: invalid number of bits per"
			   " pixel (%d) encountered in"
			   " Ps3ScreenInit()\n", pScrn->bitsPerPixel);
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
	miInitializeBackingStore(pScreen);
	xf86SetBackingStore(pScreen);

	/* software cursor */
	miDCInitialize(pScreen, xf86GetPointerScreenFuncs());

	if (!miCreateDefColormap(pScreen)) {
		xf86DrvMsg(scrnIndex, X_ERROR,
			   "internal error: miCreateDefColormap failed "
			   "in Ps3ScreenInit()\n");
		return FALSE;
	}

	flags = CMAP_PALETTED_TRUECOLOR;
	if(!xf86HandleColormaps(pScreen, 256, 8, fbdevHWLoadPaletteWeak(), 
				NULL, flags))
		return FALSE;

	xf86DPMSInit(pScreen, fbdevHWDPMSSetWeak(), 0);

	pScreen->SaveScreen = fbdevHWSaveScreenWeak();

	/* Wrap the current CloseScreen function */
	fPtr->CloseScreen = pScreen->CloseScreen;
	pScreen->CloseScreen = Ps3CloseScreen;

	{
	    XF86VideoAdaptorPtr *ptr;

	    int n = xf86XVListGenericAdaptors(pScrn,&ptr);
	    if (n) {
		xf86XVScreenInit(pScreen,ptr,n);
	    }
	}

	TRACE_EXIT("Ps3ScreenInit");

	return TRUE;
}

static Bool
Ps3CloseScreen(int scrnIndex, ScreenPtr pScreen)
{
	ScrnInfoPtr pScrn = xf86Screens[scrnIndex];
	Ps3Ptr fPtr = PS3PTR(pScrn);
	
	fbdevHWRestore(pScrn);
	fbdevHWUnmapVidmem(pScrn);
// TEMP
	ErrorF("Ps3GpuCleanup\n");
	Ps3GpuCleanup(fPtr->gpu);

	pScrn->vtSema = FALSE;

	pScreen->CreateScreenResources = fPtr->CreateScreenResources;
	pScreen->CloseScreen = fPtr->CloseScreen;
	return (*pScreen->CloseScreen)(scrnIndex, pScreen);
}

static Bool
Ps3DriverFunc(ScrnInfoPtr pScrn, xorgDriverFuncOp op, pointer ptr)
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
