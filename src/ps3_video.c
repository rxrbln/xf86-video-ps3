
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "xf86.h"
#include "xf86_OSproc.h"
#if GET_ABI_MAJOR(ABI_VIDEODRV_VERSION) < 6
#include "xf86Resources.h"
#endif
#include "xf86PciInfo.h"
#include "xf86Pci.h"
#include "xf86fbman.h"
#include "regionstr.h"

#include "xf86xv.h"
#include <X11/extensions/Xv.h>
#include "exa.h"
#include "damage.h"
#include "dixstruct.h"
#include "fourcc.h"
#include "nouveau_reg.h"
#include "ps3.h"
#include "ps3_dma.h"

#define GPU_FB_START (64 * 1024)

#define IMAGE_MAX_W 1920
#define IMAGE_MAX_H 1080

#define OFF_DELAY 	500  /* milliseconds */
#define FREE_DELAY 	5000

#define OFF_TIMER 	0x01
#define FREE_TIMER	0x02
#define CLIENT_VIDEO_ON	0x04

#define TIMER_MASK      (OFF_TIMER | FREE_TIMER)

#define NUM_BLIT_PORTS 32

/* Value taken by pPriv -> currentHostBuffer when we failed to allocate the two private buffers in TT memory, so that we can catch this case
   and attempt no other allocation afterwards (performance reasons) */
#define NO_PRIV_HOST_BUFFER_AVAILABLE 9999 
typedef struct _PS3PortPrivRec {
	short		brightness;
	short		contrast;
	short		saturation;
	short		hue;
	RegionRec	clip;
	Bool		doubleBuffer;
	CARD32		videoStatus;
	int		currentBuffer;
	Time		videoTime;
	Bool		blitter;
	Bool		SyncToVBlank;
	void 	       *video_mem;
	int		pitch;
	int		offset;
	//	PS3AllocRec * 	TT_mem_chunk[2];
	int		currentHostBuffer;
} PS3PortPrivRec, *PS3PortPrivPtr;


/* Xv DMA notifiers status tracing */

enum {
	XV_DMA_NOTIFIER_NOALLOC=0, //notifier not allocated 
	XV_DMA_NOTIFIER_INUSE=1,
	XV_DMA_NOTIFIER_FREE=2, //notifier allocated, ready for use
};

/* PS3PutImage action flags */
enum {
	IS_YV12 = 1,
	IS_YUY2 = 2,
	COPS3ERT_TO_YUY2=4,
	USE_OVERLAY=8,
	SWAP_UV=16,
};
	
#define GET_OVERLAY_PRIVATE(pPS3)					\
	(PS3PortPrivPtr)((pPS3)->overlayAdaptor->pPortPrivates[0].ptr)

#define GET_BLIT_PRIVATE(pPS3)						\
	(PS3PortPrivPtr)((pPS3)->blitAdaptor->pPortPrivates[0].ptr)

#define MAKE_ATOM(a) MakeAtom(a, sizeof(a) - 1, TRUE)

static Atom xvBrightness, xvContrast, xvColorKey, xvSaturation, 
	xvHue, xvAutopaintColorKey, xvSetDefaults, xvDoubleBuffer,
	xvITURBT709, xvSyncToVBlank;

/* client libraries expect an encoding */
static XF86VideoEncodingRec DummyEncoding =
{ 
	0,
	"XV_IMAGE",
	IMAGE_MAX_W, IMAGE_MAX_H,
	{1, 1}
};

#define NUM_FORMATS_ALL 6

XF86VideoFormatRec PS3Formats[NUM_FORMATS_ALL] = 
{
	{15, TrueColor}, {16, TrueColor}, {24, TrueColor},
	{15, DirectColor}, {16, DirectColor}, {24, DirectColor}
};


#define NUM_OVERLAY_ATTRIBUTES 9
XF86AttributeRec PS3OverlayAttributes[NUM_OVERLAY_ATTRIBUTES] =
{
	{XvSettable | XvGettable, 0, 1, "XV_DOUBLE_BUFFER"},
	{XvSettable             , 0, 0, "XV_SET_DEFAULTS"},
	{XvSettable | XvGettable, -512, 511, "XV_BRIGHTNESS"},
	{XvSettable | XvGettable, 0, 8191, "XV_CONTRAST"},
	{XvSettable | XvGettable, 0, 8191, "XV_SATURATION"},
	{XvSettable | XvGettable, 0, 360, "XV_HUE"},
};

#define NUM_BLIT_ATTRIBUTES 2
XF86AttributeRec PS3BlitAttributes[NUM_BLIT_ATTRIBUTES] =
{
	{XvSettable             , 0, 0, "XV_SET_DEFAULTS"},
	{XvSettable | XvGettable, 0, 1, "XV_SYNC_TO_VBLANK"}
};


#define NUM_IMAGES_YUV 1
#define NUM_IMAGES_ALL 1


static XF86ImageRec PS3Images[NUM_IMAGES_ALL] =
{
//	XVIMAGE_YUY2,
//	XVIMAGE_YV12,
	XVIMAGE_UYVY,
};

static void
PS3WaitVSync(ScrnInfoPtr pScrn)
{
	PS3Ptr pPS3 = PS3PTR(pScrn);
#if 0 //TODO
	BEGIN_RING(PS3ImageBlit, 0x0000012C, 1);
	OUT_RING  (0);
	BEGIN_RING(PS3ImageBlit, 0x00000134, 1);
	/* If crtc1 is active, this will produce one, otherwise zero */
	/* The assumption is that at least one is active */
	OUT_RING  (pPS3->crtc_active[1]);
	BEGIN_RING(PS3ImageBlit, 0x00000100, 1);
	OUT_RING  (0);
	BEGIN_RING(PS3ImageBlit, 0x00000130, 1);
	OUT_RING  (0);
#endif
}

/**
 * PS3SetPortDefaults
 * set attributes of port "pPriv" to compiled-in (except for colorKey) defaults
 * 
 * @param pScrn screen to get the default colorKey from
 * @param pPriv port to reset to defaults
 */
static void 
PS3SetPortDefaults (ScrnInfoPtr pScrn, PS3PortPrivPtr pPriv)
{
	PS3Ptr pPS3 = PS3PTR(pScrn);
	pPriv->brightness		= 0;
	pPriv->contrast			= 4096;
	pPriv->saturation		= 4096;
	pPriv->hue			= 0;
	pPriv->doubleBuffer		= TRUE;
}

/**
 * PS3ResetVideo
 * writes the current attributes from the overlay port to the hardware
 */
void 
PS3ResetVideo (ScrnInfoPtr pScrn)
{
}

/**
 * PS3StopOverlay
 * Tell the hardware to stop the overlay
 */
static void 
PS3StopOverlay (ScrnInfoPtr pScrn)
{
	PS3Ptr pPS3 = PS3PTR(pScrn);
	//TODO
}

/**
 * PS3AllocateVideoMemory
 * allocates video memory for a given port
 * 
 * @param pScrn screen which requests the memory
 * @param mem pointer to previously allocated memory for reallocation
 * @param size size of requested memory segment
 * @return pointer to the allocated memory
 */
static void *
PS3AllocateVideoMemory(ScrnInfoPtr pScrn, void *mem, int size)
{
	PS3Ptr pPS3 = PS3PTR(pScrn);

	if (size > pPS3->xv_size) {
		ErrorF("XV request too large\n");
		return NULL;
	}
	return (void *)pPS3->xv_base;
}
#ifndef ExaOffscreenMarkUsed
extern void ExaOffscreenMarkUsed(PixmapPtr);
#endif
#ifndef exaGetDrawablePixmap
extern PixmapPtr exaGetDrawablePixmap(DrawablePtr);
#endif
#ifndef exaPixmapIsOffscreen
extern Bool exaPixmapIsOffscreen(PixmapPtr p);
#endif
/* To support EXA 2.0, 2.1 has this in the header */
#ifndef exaMoveInPixmap
extern void exaMoveInPixmap(PixmapPtr pPixmap);
#endif

/**
 * PS3PutBlitImage
 * 
 * @param pScrn screen
 * @param src_offset
 * @param id colorspace of image
 * @param src_pitch
 * @param dstBox
 * @param x1
 * @param y1
 * @param x2
 * @param y2
 * @param width
 * @param height
 * @param src_w
 * @param src_h
 * @param drw_w
 * @param drw_h
 * @param clipBoxes
 * @param pDraw
 */
static void
PS3PutBlitImage(ScrnInfoPtr pScrn, int src_offset, int id,
		int src_pitch, BoxPtr dstBox,
		int x1, int y1, int x2, int y2,
		short width, short height,
		short src_w, short src_h,
		short drw_w, short drw_h,
		RegionPtr clipBoxes,
		DrawablePtr pDraw)
{
	PS3Ptr          pPS3   = PS3PTR(pScrn);
	PS3PortPrivPtr  pPriv = GET_BLIT_PRIVATE(pPS3);
	BoxPtr         pbox;
	int            nbox;
	CARD32         dsdx, dtdy;
	CARD32         dst_size, dst_point;
	CARD32         src_point, src_format;

	ScreenPtr pScreen = pScrn->pScreen;
	PixmapPtr pPix    = exaGetDrawablePixmap(pDraw);
	int dst_format;

	/* If we failed, draw directly onto the screen pixmap.
	 * Not sure if this is the best approach, maybe failing
	 * with BadAlloc would be better?
	 */
	if (!exaPixmapIsOffscreen(pPix)) {
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
			   "XV: couldn't move dst surface into vram,"
			   " that's expected\n");
		pPix = pScreen->GetScreenPixmap(pScreen);
	}

	PS3AccelGetCtxSurf2DFormatFromPixmap(pPix, &dst_format);
	PS3AccelSetCtxSurf2D(pPix, pPix, dst_format);

#ifdef COMPOSITE
	/* Adjust coordinates if drawing to an offscreen pixmap */
	if (pPix->screen_x || pPix->screen_y) {
		REGION_TRANSLATE(pScrn->pScreen, clipBoxes,
				 -pPix->screen_x,
				 -pPix->screen_y);
		dstBox->x1 -= pPix->screen_x;
		dstBox->x2 -= pPix->screen_x;
		dstBox->y1 -= pPix->screen_y;
		dstBox->y2 -= pPix->screen_y;
	}

	DamageDamageRegion((DrawablePtr)pPix, clipBoxes);
#endif

	pbox = REGION_RECTS(clipBoxes);
	nbox = REGION_NUM_RECTS(clipBoxes);

	dsdx = (src_w << 20) / drw_w;
	dtdy = (src_h << 20) / drw_h;

	dst_size  = ((dstBox->y2 - dstBox->y1) << 16) |
		(dstBox->x2 - dstBox->x1);
	dst_point = (dstBox->y1 << 16) | dstBox->x1;
//	src_pitch = 0x5A0;//TMP
//	src_pitch = 0x4BC;
	src_pitch |= (NV04_SCALED_IMAGE_FROM_MEMORY_FORMAT_ORIGIN_CENTER |
		      NV04_SCALED_IMAGE_FROM_MEMORY_FORMAT_FILTER_BILINEAR);
	src_point = ((y1 << 4) & 0xffff0000) | (x1 >> 12);

	switch(id) {
	case FOURCC_UYVY:
		src_format =
			NV04_SCALED_IMAGE_FROM_MEMORY_COLOR_FORMAT_YB8V8YA8U8;
		break;
	default:
		src_format =
			NV04_SCALED_IMAGE_FROM_MEMORY_COLOR_FORMAT_V8YB8U8YA8;
		break;
	}

/* TODO Not yet
   if(pPriv->SyncToVBlank) {
   FIRE_RING();
   PS3WaitVSync(pScrn);
   }
*/
	//src_format = NV04_SCALED_IMAGE_FROM_MEMORY_COLOR_FORMAT_A8R8G8B8;
	//memset(pPS3->xv_base, 0xFF, pPS3->xv_size);
	PS3DmaStart(pPS3, PS3ScaledImageChannel,
		    NV04_SCALED_IMAGE_FROM_MEMORY_DMA_IMAGE, 1);
	PS3DmaNext (pPS3, PS3DmaXDR);
	PS3DmaStart(pPS3, PS3ScaledImageChannel,
		    NV04_SCALED_IMAGE_FROM_MEMORY_COLOR_FORMAT, 2);
	PS3DmaNext(pPS3, src_format);
	PS3DmaNext(pPS3, NV04_SCALED_IMAGE_FROM_MEMORY_OPERATION_SRCCOPY);
	while(nbox--) {
		PS3DmaStart(pPS3, PS3ScaledImageChannel,
			    NV04_SCALED_IMAGE_FROM_MEMORY_CLIP_POINT, 6);
		PS3DmaNext(pPS3, (pbox->y1 << 16) | pbox->x1); 
		PS3DmaNext(pPS3, ((pbox->y2 - pbox->y1) << 16) |
			   (pbox->x2 - pbox->x1));
		PS3DmaNext(pPS3, dst_point);
		PS3DmaNext(pPS3, dst_size);
		PS3DmaNext(pPS3, dsdx);
		PS3DmaNext(pPS3, dtdy);
		
		PS3DmaStart(pPS3, PS3ScaledImageChannel,
			    NV04_SCALED_IMAGE_FROM_MEMORY_SIZE, 4);
		PS3DmaNext(pPS3, (height << 16) | width);
		PS3DmaNext(pPS3, src_pitch);
		PS3DmaNext(pPS3, pPS3->iof_offset + GPU_FB_START +
			   (pPS3->xv_base - pPS3->iof_base) + src_offset);
		PS3DmaNext(pPS3, src_point);
		pbox++;
	}

	PS3DmaKickoff(pPS3);

	exaMarkSync(pScrn->pScreen);

	pPriv->videoStatus = FREE_TIMER;
	pPriv->videoTime = currentTime.milliseconds + FREE_DELAY;
}

/**
 * PS3StopBlitVideo
 */
static void
PS3StopBlitVideo(ScrnInfoPtr pScrn, pointer data, Bool Exit)
{
}

/**
 * PS3SetOverlayPortAttribute
 * sets the attribute "attribute" of port "data" to value "value"
 * calls PS3ResetVideo(pScrn) to apply changes to hardware
 * 
 * @param pScrenInfo
 * @param attribute attribute to set
 * @param value value to which attribute is to be set
 * @param data port from which the attribute is to be set
 * 
 * @return Success, if setting is successful
 * BadValue/BadMatch, if value/attribute are ips3alid
 * @see PS3ResetVideo(ScrnInfoPtr pScrn)
 */
static int
PS3SetOverlayPortAttribute(ScrnInfoPtr pScrn, Atom attribute,
			   INT32 value, pointer data)
{
	PS3PortPrivPtr pPriv = (PS3PortPrivPtr)data;

	if (attribute == xvBrightness) {
		if ((value < -512) || (value > 512))
			return BadValue;
		pPriv->brightness = value;
	} else
		if (attribute == xvDoubleBuffer) {
			if ((value < 0) || (value > 1))
				return BadValue;
			pPriv->doubleBuffer = value;
		} else
			if (attribute == xvContrast) {
				if ((value < 0) || (value > 8191))
					return BadValue;
				pPriv->contrast = value;
			} else
				if (attribute == xvHue) {
					value %= 360;
					if (value < 0)
						value += 360;
					pPriv->hue = value;
				} else
					if (attribute == xvSaturation) {
						if ((value < 0) || (value > 8191))
							return BadValue;
						pPriv->saturation = value;
					} else
						if (attribute == xvSetDefaults) {
							PS3SetPortDefaults(pScrn, pPriv);
						} else
							return BadMatch;

	PS3ResetVideo(pScrn);
	return Success;
}

/**
 * PS3GetOverlayPortAttribute
 * 
 * @param pScrn unused
 * @param attribute attribute to be read
 * @param value value of attribute will be stored in this pointer
 * @param data port from which attribute will be read
 * @return Success, if queried attribute exists
 */
static int
PS3GetOverlayPortAttribute(ScrnInfoPtr pScrn, Atom attribute,
			   INT32 *value, pointer data)
{
	PS3PortPrivPtr pPriv = (PS3PortPrivPtr)data;

	if (attribute == xvBrightness)
		*value = pPriv->brightness;
	else if (attribute == xvDoubleBuffer)
		*value = (pPriv->doubleBuffer) ? 1 : 0;
	else if (attribute == xvContrast)
		*value = pPriv->contrast;
	else if (attribute == xvSaturation)
		*value = pPriv->saturation;
	else if (attribute == xvHue)
		*value = pPriv->hue;
	else
		return BadMatch;

	return Success;
}

/**
 * PS3SetBlitPortAttribute
 * sets the attribute "attribute" of port "data" to value "value"
 * supported attributes:
 * - xvSyncToVBlank (values: 0,1)
 * - xvSetDefaults (values: NA; SyncToVBlank will be set, if hardware supports it)
 * 
 * @param pScrenInfo
 * @param attribute attribute to set
 * @param value value to which attribute is to be set
 * @param data port from which the attribute is to be set
 * 
 * @return Success, if setting is successful
 * BadValue/BadMatch, if value/attribute are ips3alid
 */
static int
PS3SetBlitPortAttribute(ScrnInfoPtr pScrn, Atom attribute,
			INT32 value, pointer data)
{
	PS3PortPrivPtr pPriv = (PS3PortPrivPtr)data;
	PS3Ptr           pPS3 = PS3PTR(pScrn);
#if 0
	if ((attribute == xvSyncToVBlank) && pPS3->WaitVSyncPossible) {
		if ((value < 0) || (value > 1))
			return BadValue;
		pPriv->SyncToVBlank = value;
	} else
		if (attribute == xvSetDefaults) {
			pPriv->SyncToVBlank = pPS3->WaitVSyncPossible;
		} else
			return BadMatch;
#endif
	return Success;
}

/**
 * PS3GetBlitPortAttribute
 * reads the value of attribute "attribute" from port "data" into INT32 "*value"
 * currently only one attribute supported: xvSyncToVBlank
 * 
 * @param pScrn unused
 * @param attribute attribute to be read
 * @param value value of attribute will be stored here
 * @param data port from which attribute will be read
 * @return Success, if queried attribute exists
 */
static int
PS3GetBlitPortAttribute(ScrnInfoPtr pScrn, Atom attribute,
			INT32 *value, pointer data)
{
	PS3PortPrivPtr pPriv = (PS3PortPrivPtr)data;

	if(attribute == xvSyncToVBlank)
		*value = (pPriv->SyncToVBlank) ? 1 : 0;
	else
		return BadMatch;

	return Success;
}


/**
 * QueryBestSize
 * used by client applications to ask the driver:
 * how would you actually scale a video of dimensions
 * vid_w, vid_h, if i wanted you to scale it to dimensions
 * drw_w, drw_h?
 * function stores actual scaling size in pointers p_w, p_h.
 * 
 * 
 * @param pScrn unused
 * @param motion unused
 * @param vid_w width of source video
 * @param vid_h height of source video
 * @param drw_w desired scaled width as requested by client
 * @param drw_h desired scaled height as requested by client
 * @param p_w actual scaled width as the driver is capable of
 * @param p_h actual scaled height as the driver is capable of
 * @param data unused
 */
static void
PS3QueryBestSize(ScrnInfoPtr pScrn, Bool motion,
		 short vid_w, short vid_h, 
		 short drw_w, short drw_h, 
		 unsigned int *p_w, unsigned int *p_h, 
		 pointer data)
{
	if(vid_w > (drw_w << 3))
		drw_w = vid_w >> 3;
	if(vid_h > (drw_h << 3))
		drw_h = vid_h >> 3;

	*p_w = drw_w;
	*p_h = drw_h; 
}

/**
 * PS3CopyData420
 * used to cops3ert YV12 to YUY2 for the blitter
 * 
 * @param src1 source buffer of luma
 * @param src2 source buffer of chroma1
 * @param src3 source buffer of chroma2
 * @param dst1 destination buffer
 * @param srcPitch pitch of src1
 * @param srcPitch2 pitch of src2, src3
 * @param dstPitch pitch of dst1
 * @param h number of lines to copy
 * @param w length of lines to copy
 */
static inline void PS3CopyData420(unsigned char *src1, unsigned char *src2,
				  unsigned char *src3, unsigned char *dst1,
				  int srcPitch, int srcPitch2,
				  int dstPitch,
				  int h, int w)
{
	CARD32 *dst;
	CARD8 *s1, *s2, *s3;
	int i, j;

	w >>= 1;

	for (j = 0; j < h; j++) {
		dst = (CARD32*)dst1;
		s1 = src1;  s2 = src2;  s3 = src3;
		i = w;

		while (i > 4) {
#if X_BYTE_ORDER == X_BIG_ENDIAN
			dst[0] = (s1[0] << 24) | (s1[1] << 8) | (s3[0] << 16) | s2[0];
			dst[1] = (s1[2] << 24) | (s1[3] << 8) | (s3[1] << 16) | s2[1];
			dst[2] = (s1[4] << 24) | (s1[5] << 8) | (s3[2] << 16) | s2[2];
			dst[3] = (s1[6] << 24) | (s1[7] << 8) | (s3[3] << 16) | s2[3];
#else
			dst[0] = s1[0] | (s1[1] << 16) | (s3[0] << 8) | (s2[0] << 24);
			dst[1] = s1[2] | (s1[3] << 16) | (s3[1] << 8) | (s2[1] << 24);
			dst[2] = s1[4] | (s1[5] << 16) | (s3[2] << 8) | (s2[2] << 24);
			dst[3] = s1[6] | (s1[7] << 16) | (s3[3] << 8) | (s2[3] << 24);
#endif
			dst += 4; s2 += 4; s3 += 4; s1 += 8;
			i -= 4;
		}

		while (i--) {
#if X_BYTE_ORDER == X_BIG_ENDIAN
			dst[0] = (s1[0] << 24) | (s1[1] << 8) | (s3[0] << 16) | s2[0];
#else
			dst[0] = s1[0] | (s1[1] << 16) | (s3[0] << 8) | (s2[0] << 24);
#endif
			dst++; s2++; s3++;
			s1 += 2;
		}

		dst1 += dstPitch;
		src1 += srcPitch;
		if (j & 1) {
			src2 += srcPitch2;
			src3 += srcPitch2;
		}
	}
}

/**
 * PS3CopyPS312ColorPlanes
 * Used to cops3ert YV12 color planes to PS312 (interleaved UV) for the overlay
 * 
 * @param src1 source buffer of chroma1
 * @param dst1 destination buffer
 * @param h number of lines to copy
 * @param w length of lines to copy
 * @param id source pixel format (YV12 or I420)
 */
static inline void PS3CopyPS312ColorPlanes(unsigned char *src1, unsigned char * src2, unsigned char *dst, int dstPitch, int srcPitch2, 
					   int h, int w)
{
	
	int i,j,l,e;
	
	w >>= 1;
	h >>= 1;
	l = w >> 1;
	e = w & 1;
	for ( j = 0; j < h; j++ ) 
	{
		unsigned char * us = src1;
		unsigned char * vs = src2;
		unsigned int * vuvud = (unsigned int *) dst;
		for ( i = 0; i < l; i++ )
		{
#if X_BYTE_ORDER == X_BIG_ENDIAN
			*vuvud++ = (vs[0]<<24) | (us[0]<<16) | (vs[1]<<8) | us[1];
#else
			*vuvud++ = vs[0] | (us[0]<<8) | (vs[1]<<16) | (us[1]<<24);
#endif
			us+=2;
			vs+=2;
		}
		if (e)  {
			unsigned short *vud = (unsigned short *) vuvud;
#if X_BYTE_ORDER == X_BIG_ENDIAN
			*vud = (vs[0]<<8) | (us[0] << 0);
#else
			*vud = vs[0] | (us[0]<<8);
#endif
		}
		dst += dstPitch ;
		src1 += srcPitch2;
		src2 += srcPitch2;
	}	

}


static int PS3_set_dimensions(ScrnInfoPtr pScrn, int action_flags, INT32 * xa, INT32 * xb, INT32 * ya, INT32 * yb, 
			      short * src_x, short * src_y, short * src_w, short * src_h,
			      short * drw_x, short * drw_y, short * drw_w, short * drw_h,
			      int * left, int * top, int * right, int * bottom,
			      BoxRec * dstBox, 
			      int * npixels, int * nlines,
			      RegionPtr clipBoxes, short width, short height
	)
{
	
	if ( action_flags & USE_OVERLAY ) 
	{ /* overlay hardware scaler limitation - copied from ps3, UNCHECKED*/
		if (*src_w > (*drw_w << 3))
			*drw_w = *src_w >> 3;
		if (*src_h > (*drw_h << 3))
			*drw_h = *src_h >> 3;
	}
	

	/* Clip */
	*xa = *src_x;
	*xb = *src_x + *src_w;
	*ya = *src_y;
	*yb = *src_y + *src_h;

	dstBox->x1 = *drw_x;
	dstBox->x2 = *drw_x + *drw_w;
	dstBox->y1 = *drw_y;
	dstBox->y2 = *drw_y + *drw_h;

	if (!xf86XVClipVideoHelper(dstBox, xa, xb, ya, yb, clipBoxes,
				   width, height))
		return -1;

	if ( action_flags & USE_OVERLAY )
	{
		dstBox->x1 -= pScrn->frameX0;
		dstBox->x2 -= pScrn->frameX0;
		dstBox->y1 -= pScrn->frameY0;
		dstBox->y2 -= pScrn->frameY0;
	}
		
	
	
	/* Convert fixed point to integer, as xf86XVClipVideoHelper probably turns its parameter into fixed point values */
	*left = (*xa) >> 16;
	if (*left < 0) *left = 0;
	*top = (*ya) >> 16;
	if (*top < 0) *top = 0;
	*right = (*xb) >> 16;
	if (*right > width) *right = width;
	*bottom = (*yb) >> 16;
	if (*bottom > height) *bottom = height;
	
	if ( action_flags & IS_YV12 )
	{
		*left &= ~1; //even "left", even "top", even number of pixels per line and even number of lines
		*npixels = ((*right + 1) & ~1) - *left;
		*top &= ~1;
	        *nlines = ((*bottom + 1) & ~1) - *top;
	}
	else if ( action_flags & IS_YUY2 )
	{
		*left &= ~1; //even "left"
		*npixels = ((*right + 1) & ~1) - *left; //even number of pixels per line
		*nlines = *bottom - *top; 
		*left <<= 1; //16bpp
	}
	
	return 0;
}

static int PS3_calculate_pitches_and_mem_size(int action_flags, int * srcPitch, int * srcPitch2, int * dstPitch, 
					      int * s2offset, int * s3offset, 
					      int * newFBSize, int * newTTSize,
					      int * line_len, int npixels, int nlines, int width, int height)
{
	int tmp;
		
	if ( action_flags & IS_YV12 ) 
	{	/*YV12 or I420*/
		*srcPitch = (width + 3) & ~3;	/* of luma */
		*s2offset = *srcPitch * height;
		*srcPitch2 = ((width >> 1) + 3) & ~3; /*of chroma*/
		*s3offset = (*srcPitch2 * (height >> 1)) + *s2offset;
		*dstPitch = (npixels + 63) &~ 63; /*luma and chroma pitch*/
		*line_len = npixels;
		*newFBSize = nlines * *dstPitch + (nlines >> 1) * *dstPitch;
		*newTTSize = nlines * *dstPitch + (nlines >> 1) * *dstPitch;
	}
	else if ( action_flags & IS_YUY2 )
	{
		*srcPitch = width << 1; /* one luma, one chroma per pixel */
		*dstPitch = npixels << 1;
		*line_len = npixels << 1;
		*newFBSize = nlines * *dstPitch;
		*newTTSize = nlines * *line_len;
	}
	
	if ( action_flags & COPS3ERT_TO_YUY2 )
	{
		*dstPitch = ((npixels << 1) + 63) & ~63;
		*line_len = npixels << 1;
		*newFBSize = nlines * *dstPitch;
		*newTTSize = nlines * *line_len;
	}
	
	if ( action_flags & SWAP_UV ) 
	{ //I420 swaps U and V
		tmp = *s2offset;
		*s2offset = *s3offset;
		*s3offset = tmp;
	}
	
	if ( action_flags & USE_OVERLAY ) // overlay double buffering ...
                (*newFBSize) <<= 1; // ... means double the amount of VRAM needed
	
	return 0;
}


/**
 * PS3_set_action_flags
 * This function computes the action flags from the input image,
 * that is, it decides what PS3PutImage and its helpers must do.
 * This eases readability by avoiding lots of switch-case statements in the core PS3PutImage
 */
static void PS3_set_action_flags(PS3Ptr pPS3, ScrnInfoPtr pScrn, DrawablePtr pDraw, PS3PortPrivPtr pPriv, int id, int * action_flags)
{
	*action_flags = 0;
	if ( id == FOURCC_YUY2 || id == FOURCC_UYVY )
		*action_flags |= IS_YUY2;
	
	if ( id == FOURCC_YV12 || id == FOURCC_I420 )
		*action_flags |= IS_YV12;
	
	if ( !pPriv -> blitter )
		*action_flags |= USE_OVERLAY;
	
#ifdef COMPOSITE
	WindowPtr pWin = NULL;
		
	if (!noCompositeExtension && WindowDrawable(pDraw->type)) 
	{
		pWin = (WindowPtr)pDraw;
	}
			
	if ( pWin )
		if ( pWin->redirectDraw )
			*action_flags &= ~USE_OVERLAY;
				
#endif
		
	if ( ! ( *action_flags & USE_OVERLAY) )
	{
		if ( id == FOURCC_YV12 || id == FOURCC_I420 )
		{ /*The blitter does not handle YV12 natively*/
			*action_flags |= COPS3ERT_TO_YUY2;
		}
	}

	
}


/**
 * PS3PutImage
 * PutImage is "the" important function of the Xv extension.
 * a client (e.g. video player) calls this function for every
 * image (of the video) to be displayed. this function then
 * scales and displays the image.
 * 
 * @param pScrn screen which hold the port where the image is put
 * @param src_x source point in the source image to start displaying from
 * @param src_y see above
 * @param src_w width of the source image to display
 * @param src_h see above
 * @param drw_x  screen point to display to
 * @param drw_y
 * @param drw_w width of the screen drawable
 * @param drw_h
 * @param id pixel format of image
 * @param buf pointer to buffer containing the source image
 * @param width total width of the source image we are passed
 * @param height 
 * @param Sync unused
 * @param clipBoxes ??
 * @param data pointer to port 
 * @param pDraw drawable pointer
 */
static int
PS3PutImage(ScrnInfoPtr  pScrn, short src_x, short src_y,
	    short drw_x, short drw_y,
	    short src_w, short src_h, 
	    short drw_w, short drw_h,
	    int id,
	    unsigned char *buf, 
	    short width, short height, 
	    Bool         Sync, /*FIXME: need to honor the Sync*/
	    RegionPtr    clipBoxes,
	    pointer      data,
	    DrawablePtr  pDraw
	)
{
	PS3PortPrivPtr pPriv = (PS3PortPrivPtr)data;
	PS3Ptr pPS3 = PS3PTR(pScrn);
	INT32 xa = 0, xb = 0, ya = 0, yb = 0; //source box
	int newFBSize = 0, newTTSize = 0; //size to allocate in VRAM and in GART respectively
	int offset = 0, s2offset = 0, s3offset = 0; //card VRAM offset, source offsets for U and V planes
	int srcPitch = 0, srcPitch2 = 0, dstPitch = 0; //source pitch, source pitch of U and V planes in case of YV12, VRAM destination pitch
	int top = 0, left = 0, right = 0, bottom = 0, npixels = 0, nlines = 0; //position of the given source data (using src_*), number of pixels and lines we are interested in
	Bool skip = FALSE;
	BoxRec dstBox;
	CARD32 tmp = 0;
	int line_len = 0; //length of a line, like npixels, but in bytes 
	int DMAoffset = 0; //additional VRAM offset to start the DMA copy to
	int UVDMAoffset = 0;
	unsigned char * video_mem_destination = NULL;  
	int action_flags; //what shall we do?
	
	
	PS3_set_action_flags(pPS3, pScrn, pDraw, pPriv, id, &action_flags);
	
	if ( PS3_set_dimensions(pScrn, action_flags, &xa, &xb, &ya, &yb, 
				&src_x,  &src_y, &src_w, &src_h,
				&drw_x, &drw_y, &drw_w, &drw_h, 
				&left, &top, &right, &bottom, &dstBox, 
				&npixels, &nlines,
				clipBoxes, width, height ) )
	{
		return Success;
	}
	

	if ( PS3_calculate_pitches_and_mem_size(action_flags,
						&srcPitch, &srcPitch2,
						&dstPitch, 
						&s2offset, &s3offset, 
						& newFBSize, &newTTSize ,
						&line_len ,
						npixels, nlines,
						width, height) )
	{
		return BadImplementation;
	}
	
	/* There are some cases (tvtime with overscan for example) where the input image is larger (width/height) than 
	   the source rectangle for the overlay (src_w, src_h). In those cases, we try to do something optimal by uploading only 
	   the necessary data. */
	if ( action_flags & IS_YUY2 )
	{
		buf += (top * srcPitch) + left;
		DMAoffset += left + (top * dstPitch);
	}
		
	if ( action_flags & IS_YV12 )
	{
		tmp = ((top >> 1) * srcPitch2) + (left >> 1);
		s2offset += tmp;
		s3offset += tmp;
		
		if ( action_flags & COPS3ERT_TO_YUY2 )
		{
			DMAoffset += (left << 1) + (top * dstPitch);
		}
			
		else
		{
			//real YV12 - we offset only the luma plane, and copy the whole color plane, for easiness
			DMAoffset += left + (top * dstPitch);
			UVDMAoffset += left + (top >> 1) * dstPitch;
		}	
	}
	
	pPriv->video_mem = PS3AllocateVideoMemory(pScrn, pPriv->video_mem, 
						  newFBSize);
	if (!pPriv->video_mem)
		return BadAlloc;

	offset = 0; //pPriv->video_mem->offset;
#if 0	
	/*The overlay supports hardware double buffering. We handle this here*/
	if (pPriv->doubleBuffer) {
		int mask = 1 << (pPriv->currentBuffer << 2);
		/* overwrite the newest buffer if there's not one free */
		if (ps3ReadVIDEO(pPS3, PS3_PVIDEO_BUFFER) & mask) {
			if (!pPriv->currentBuffer)
				offset += newFBSize >> 1;
			skip = TRUE;
		} 
		else 
			if (pPriv->currentBuffer)
				offset += newFBSize >> 1;
	}
#endif
	if (newTTSize <= pPS3->xv_size)
	{
		unsigned char *dst = (unsigned char *) pPS3->xv_base;
		int i = 0;
			
		/* Upload to GART */
		if ( action_flags & IS_YV12)
		{

			PS3CopyData420(buf + (top * srcPitch) + left,
				       buf + s2offset, buf + s3offset,
				       dst, srcPitch, srcPitch2,
				       line_len, nlines, npixels);
			
		}
		else 
		{
			for ( i=0; i < nlines; i++)
			{
				memcpy(dst, buf, line_len);
				dst += line_len;
				buf += srcPitch;
			}
		}
		
		
	}
	else { 
		ErrorF("gart too small :)");
	} //CPU copy
		

	if (!skip) 
	{
		PS3PutBlitImage(pScrn, offset, id,
				dstPitch, &dstBox,
				0, 0, xb, yb,
				npixels, nlines,
				src_w, src_h, drw_w, drw_h,
				clipBoxes, pDraw);
	}
	return Success;
}

/**
 * QueryImageAttributes
 * 
 * calculates
 * - size (memory required to store image),
 * - pitches,
 * - offsets
 * of image
 * depending on colorspace (id) and dimensions (w,h) of image
 * values of
 * - w,
 * - h
 * may be adjusted as needed
 * 
 * @param pScrn unused
 * @param id colorspace of image
 * @param w pointer to width of image
 * @param h pointer to height of image
 * @param pitches pitches[i] = length of a scanline in plane[i]
 * @param offsets offsets[i] = offset of plane i from the beginning of the image
 * @return size of the memory required for the XvImage queried
 */
static int
PS3QueryImageAttributes(ScrnInfoPtr pScrn, int id, 
			unsigned short *w, unsigned short *h, 
			int *pitches, int *offsets)
{
	int size, tmp;

	if (*w > IMAGE_MAX_W)
		*w = IMAGE_MAX_W;
	if (*h > IMAGE_MAX_H)
		*h = IMAGE_MAX_H;

	*w = (*w + 1) & ~1; // width rounded up to an even number
	if (offsets)
		offsets[0] = 0;

	switch (id) {
	case FOURCC_YV12:
		*h = (*h + 1) & ~1; // height rounded up to an even number
		size = (*w + 3) & ~3; // width rounded up to a multiple of 4
		if (pitches)
			pitches[0] = size; // width rounded up to a multiple of 4
		size *= *h;
		if (offsets)
			offsets[1] = size; // number of pixels in "rounded up" image
		tmp = ((*w >> 1) + 3) & ~3; // width/2 rounded up to a multiple of 4
		if (pitches)
			pitches[1] = pitches[2] = tmp; // width/2 rounded up to a multiple of 4
		tmp *= (*h >> 1); // 1/4*number of pixels in "rounded up" image
		size += tmp; // 5/4*number of pixels in "rounded up" image
		if (offsets)
			offsets[2] = size; // 5/4*number of pixels in "rounded up" image
		size += tmp; // = 3/2*number of pixels in "rounded up" image
		break;
	case FOURCC_UYVY:
	case FOURCC_YUY2:
		size = *w << 1; // 2*width
		if (pitches)
			pitches[0] = size; // 2*width
		size *= *h; // 2*width*height
		break;
	default:
		*w = *h = size = 0;
		break;
	}

	return size;
}


/**
 * PS3SetupBlitVideo
 * this function does all the work setting up a blit port
 * 
 * @return blit port
 */
static XF86VideoAdaptorPtr
PS3SetupBlitVideo (ScreenPtr pScreen)
{
	ScrnInfoPtr         pScrn = xf86Screens[pScreen->myNum];
	PS3Ptr               pPS3       = PS3PTR(pScrn);
	XF86VideoAdaptorPtr adapt;
	PS3PortPrivPtr       pPriv;
	int i;

	if (!(adapt = calloc(1, sizeof(XF86VideoAdaptorRec) +
			      sizeof(PS3PortPrivRec) +
			      (sizeof(DevUnion) * NUM_BLIT_PORTS)))) {
		return NULL;
	}

	adapt->type		= XvWindowMask | XvInputMask | XvImageMask;
	adapt->flags		= 0;
	adapt->name		= "PS3 Video Blitter";
	adapt->nEncodings	= 1;
	adapt->pEncodings	= &DummyEncoding;
	adapt->nFormats		= NUM_FORMATS_ALL;
	adapt->pFormats		= PS3Formats;
	adapt->nPorts		= NUM_BLIT_PORTS;
	adapt->pPortPrivates	= (DevUnion*)(&adapt[1]);

	pPriv = (PS3PortPrivPtr)(&adapt->pPortPrivates[NUM_BLIT_PORTS]);
	for(i = 0; i < NUM_BLIT_PORTS; i++)
		adapt->pPortPrivates[i].ptr = (pointer)(pPriv);

	adapt->pAttributes = PS3BlitAttributes;
	adapt->nAttributes = NUM_BLIT_ATTRIBUTES;

	adapt->pImages			= PS3Images;
	adapt->nImages			= NUM_IMAGES_ALL;
	adapt->PutVideo			= NULL;
	adapt->PutStill			= NULL;
	adapt->GetVideo			= NULL;
	adapt->GetStill			= NULL;
	adapt->StopVideo		= PS3StopBlitVideo;
	adapt->SetPortAttribute		= PS3SetBlitPortAttribute;
	adapt->GetPortAttribute		= PS3GetBlitPortAttribute;
	adapt->QueryBestSize		= PS3QueryBestSize;
	adapt->PutImage			= PS3PutImage;
	adapt->QueryImageAttributes	= PS3QueryImageAttributes;

	pPriv->videoStatus		= 0;
	pPriv->blitter			= TRUE;
	pPriv->doubleBuffer		= FALSE;
	pPriv->SyncToVBlank		= TRUE;//pPS3->WaitVSyncPossible;

	pPS3->blitAdaptor		= adapt;
	xvSyncToVBlank			= MAKE_ATOM("XV_SYNC_TO_VBLANK");

	return adapt;
}

/**
 * PS3ChipsetHasOverlay
 * 
 * newer chips don't support overlay anymore.
 * overlay feature is emulated via textures.
 * 
 * @param pPS3 
 * @return true, if chipset supports overlay
 */
static Bool
PS3ChipsetHasOverlay(PS3Ptr pPS3)
{
	return FALSE;
}

/**
 * PS3InitVideo
 * tries to initialize one new overlay port and one new blit port
 * and add them to the list of ports on screen "pScreen".
 * 
 * @param pScreen
 * @see PS3SetupOverlayVideo(ScreenPtr pScreen)
 * @see PS3SetupBlitVideo(ScreenPtr pScreen)
 */
void PS3InitVideo (ScreenPtr pScreen)
{
	ScrnInfoPtr          pScrn = xf86Screens[pScreen->myNum];
	PS3Ptr                pPS3 = PS3PTR(pScrn);
	XF86VideoAdaptorPtr *adaptors, *newAdaptors = NULL;
	XF86VideoAdaptorPtr  blitAdaptor = NULL;
	int                  num_adaptors;

	/*
	 * Driving the blitter requires the DMA FIFO. Using the FIFO
	 * without accel causes DMA errors. While the overlay might
	 * might work without accel, we also disable it for now when
	 * acceleration is disabled:
	 */
	if (pScrn->bitsPerPixel != 8 && !pPS3->NoAccel) {
		blitAdaptor    = PS3SetupBlitVideo(pScreen);
	}

	num_adaptors = xf86XVListGenericAdaptors(pScrn, &adaptors);
	if(blitAdaptor) {
		int size = num_adaptors;

		if(blitAdaptor)    size++;

		newAdaptors = malloc(size * sizeof(XF86VideoAdaptorPtr *));
		if(newAdaptors) {
			if(num_adaptors) {
				memcpy(newAdaptors, adaptors, num_adaptors *
				       sizeof(XF86VideoAdaptorPtr));
			}


			if(blitAdaptor) {
				newAdaptors[num_adaptors] = blitAdaptor;
				num_adaptors++;
			}

			adaptors = newAdaptors;
		}
	}

	if (num_adaptors)
		xf86XVScreenInit(pScreen, adaptors, num_adaptors);
	if (newAdaptors)
		free(newAdaptors);
}

