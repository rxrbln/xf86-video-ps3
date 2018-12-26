 /***************************************************************************\
|*                                                                           *|
|*       Copyright 2003 NVIDIA, Corporation.  All rights reserved.           *|
|*                                                                           *|
|*     NOTICE TO USER:   The source code  is copyrighted under  U.S. and     *|
|*     international laws.  Users and possessors of this source code are     *|
|*     hereby granted a nonexclusive,  royalty-free copyright license to     *|
|*     use this code in individual and commercial software.                  *|
|*                                                                           *|
|*     Any use of this source code must include,  in the user documenta-     *|
|*     tion and  internal comments to the code,  notices to the end user     *|
|*     as follows:                                                           *|
|*                                                                           *|
|*       Copyright 2003 NVIDIA, Corporation.  All rights reserved.           *|
|*                                                                           *|
|*     NVIDIA, CORPORATION MAKES NO REPRESENTATION ABOUT THE SUITABILITY     *|
|*     OF  THIS SOURCE  CODE  FOR ANY PURPOSE.  IT IS  PROVIDED  "AS IS"     *|
|*     WITHOUT EXPRESS OR IMPLIED WARRANTY OF ANY KIND.  NVIDIA, CORPOR-     *|
|*     ATION DISCLAIMS ALL WARRANTIES  WITH REGARD  TO THIS SOURCE CODE,     *|
|*     INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY, NONINFRINGE-     *|
|*     MENT,  AND FITNESS  FOR A PARTICULAR PURPOSE.   IN NO EVENT SHALL     *|
|*     NVIDIA, CORPORATION  BE LIABLE FOR ANY SPECIAL,  INDIRECT,  INCI-     *|
|*     DENTAL, OR CONSEQUENTIAL DAMAGES,  OR ANY DAMAGES  WHATSOEVER RE-     *|
|*     SULTING FROM LOSS OF USE,  DATA OR PROFITS,  WHETHER IN AN ACTION     *|
|*     OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,  ARISING OUT OF     *|
|*     OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOURCE CODE.     *|
|*                                                                           *|
|*     U.S. Government  End  Users.   This source code  is a "commercial     *|
|*     item,"  as that  term is  defined at  48 C.F.R. 2.101 (OCT 1995),     *|
|*     consisting  of "commercial  computer  software"  and  "commercial     *|
|*     computer  software  documentation,"  as such  terms  are  used in     *|
|*     48 C.F.R. 12.212 (SEPT 1995)  and is provided to the U.S. Govern-     *|
|*     ment only as  a commercial end item.   Consistent with  48 C.F.R.     *|
|*     12.212 and  48 C.F.R. 227.7202-1 through  227.7202-4 (JUNE 1995),     *|
|*     all U.S. Government End Users  acquire the source code  with only     *|
|*     those rights set forth herein.                                        *|
|*                                                                           *|
 \***************************************************************************/

/*
  Exa Modifications (c) Lars Knoll (lars@trolltech.com)
  PS3 Modifications (c) Vivien Chappelier (vivien.chappelier@free.fr)
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

//#include "nv_include.h"
#include "xf86.h"
#include "exa.h"
#include "xf86xv.h"
#include "ps3.h"
#include "ps3_dma.h"

#include "nouveau_reg.h"

#include <sys/time.h>
#include <string.h>
#include <unistd.h>

//#define TRACE() ErrorF("%s\n", __FUNCTION__);
#define TRACE()

Bool
PS3AccelGetCtxSurf2DFormatFromPixmap(PixmapPtr pPix, int *fmt_ret)
{
	switch (pPix->drawable.bitsPerPixel) {
	case 32:
		*fmt_ret = SURFACE_FORMAT_A8R8G8B8;
		break;
	case 24:
		*fmt_ret = SURFACE_FORMAT_A8R8G8B8; // TEMP
		break;
	case 16:
		*fmt_ret = SURFACE_FORMAT_R5G6B5;
		break;
	case 8:
		*fmt_ret = SURFACE_FORMAT_Y8;
		break;
	default:
		return FALSE;
	}

	return TRUE;
}

Bool
PS3AccelGetCtxSurf2DFormatFromPicture(PicturePtr pPict, int *fmt_ret)
{
	switch (pPict->format) {
	case PICT_a8r8g8b8:
		*fmt_ret = SURFACE_FORMAT_A8R8G8B8;
		break;
	case PICT_x8r8g8b8:
		*fmt_ret = SURFACE_FORMAT_X8R8G8B8;
		break;
	case PICT_r5g6b5:
		*fmt_ret = SURFACE_FORMAT_R5G6B5;
		break;
	case PICT_a8:
		*fmt_ret = SURFACE_FORMAT_Y8;
		break;
	default:
		return FALSE;
	}

	return TRUE;
}

CARD32
PS3AccelGetPixmapOffset(PixmapPtr pPix)
{
	ScrnInfoPtr pScrn = xf86Screens[pPix->drawable.pScreen->myNum];
	PS3Ptr pPS3 = PS3PTR(pScrn);
	unsigned long offset;

	offset = exaGetPixmapOffset(pPix);
	if (offset >= pPS3->vram_size) {
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
			   "AII, passed bad pixmap: offset 0x%lx\n",
			   offset);
		return 0;
	}

	return offset;
}

Bool
PS3AccelSetCtxSurf2D(PixmapPtr psPix, PixmapPtr pdPix, int format)
{
	ScrnInfoPtr pScrn = xf86Screens[psPix->drawable.pScreen->myNum];
	PS3Ptr pPS3 = PS3PTR(pScrn);

	PS3DmaStart(pPS3, PS3ContextSurfacesChannel, SURFACE_FORMAT, 4);
	PS3DmaNext (pPS3, format);
	PS3DmaNext (pPS3, ((CARD32)exaGetPixmapPitch(pdPix) << 16) |
			 (CARD32)exaGetPixmapPitch(psPix));
	PS3DmaNext (pPS3, PS3AccelGetPixmapOffset(psPix));
	PS3DmaNext (pPS3, PS3AccelGetPixmapOffset(pdPix));

	return TRUE;
}

static void PS3ExaWaitMarker(ScreenPtr pScreen, int marker)
{
//	ErrorF("%s\n", __FUNCTION__);
	PS3Sync(xf86Screens[pScreen->myNum]);
}

static Bool PS3ExaPrepareSolid(PixmapPtr pPixmap,
			      int   alu,
			      Pixel planemask,
			      Pixel fg)
{
	TRACE();
	return FALSE;
}

static void PS3ExaSolid (PixmapPtr pPixmap, int x1, int y1, int x2, int y2)
{
	TRACE();
}

static void PS3ExaDoneSolid (PixmapPtr pPixmap)
{
	TRACE();
}

static CARD32 copy_src_size, copy_src_pitch, copy_src_offset;

static Bool PS3ExaPrepareCopy(PixmapPtr pSrcPixmap,
			     PixmapPtr pDstPixmap,
			     int       dx,
			     int       dy,
			     int       alu,
			     Pixel     planemask)
{
	ScrnInfoPtr pScrn = xf86Screens[pDstPixmap->drawable.pScreen->myNum];
	PS3Ptr pPS3 = PS3PTR(pScrn);
	int srcFormat, dstFormat;

//	ErrorF("%s %d %d %d\n", __FUNCTION__, dx, dy, alu);

	if (pSrcPixmap->drawable.bitsPerPixel !=
			pDstPixmap->drawable.bitsPerPixel)
		return FALSE;

	planemask |= ~0 << pDstPixmap->drawable.bitsPerPixel;
	if (planemask != ~0 || alu != GXcopy)
		return FALSE;

	switch (pSrcPixmap->drawable.bitsPerPixel) {
	case 32:
		srcFormat = STRETCH_BLIT_FORMAT_A8R8G8B8;
		break;
	case 24:
		srcFormat = STRETCH_BLIT_FORMAT_X8R8G8B8;
		break;
	case 16:
		srcFormat = STRETCH_BLIT_FORMAT_DEPTH16;
		break;
	case 8:
		srcFormat = STRETCH_BLIT_FORMAT_DEPTH8;
		break;
	default:
		return FALSE;
	}

	if (!PS3AccelGetCtxSurf2DFormatFromPixmap(pDstPixmap, &dstFormat))
		return FALSE;
	if (!PS3AccelSetCtxSurf2D(pSrcPixmap, pDstPixmap, dstFormat))
		return FALSE;

	/* screen to screen copy */
	PS3DmaStart(pPS3, PS3ScaledImageChannel,
		    NV04_SCALED_IMAGE_FROM_MEMORY_DMA_IMAGE, 1);
	PS3DmaNext (pPS3, PS3DmaFB);

	PS3DmaStart(pPS3, PS3ScaledImageChannel, STRETCH_BLIT_FORMAT, 2);
	PS3DmaNext (pPS3, srcFormat);
	PS3DmaNext (pPS3, STRETCH_BLIT_OPERATION_COPY);

	copy_src_size = ((pSrcPixmap->drawable.width+3)&~3) |
		(pSrcPixmap->drawable.height << 16);
	copy_src_pitch  = exaGetPixmapPitch(pSrcPixmap)
		| (STRETCH_BLIT_SRC_FORMAT_ORIGIN_CORNER << 16)
		| (STRETCH_BLIT_SRC_FORMAT_FILTER_POINT_SAMPLE << 24);
	copy_src_offset = PS3AccelGetPixmapOffset(pSrcPixmap);
/*

	ErrorF("%s sfmt=%d dfmt=%d dpitch=%d spitch=%d soffset=0x%x doffset=0x%x\n", __FUNCTION__,
	       srcFormat, dstFormat,
	       exaGetPixmapPitch(pDstPixmap),
	       exaGetPixmapPitch(pSrcPixmap),
	       PS3AccelGetPixmapOffset(pSrcPixmap),
	       PS3AccelGetPixmapOffset(pDstPixmap));
*/
	return TRUE;
}

static void PS3ExaCopy(PixmapPtr pDstPixmap,
		      int	srcX,
		      int	srcY,
		      int	dstX,
		      int	dstY,
		      int	width,
		      int	height)
{
	ScrnInfoPtr pScrn = xf86Screens[pDstPixmap->drawable.pScreen->myNum];
	PS3Ptr pPS3 = PS3PTR(pScrn);
/*
	ErrorF("%s from (%d,%d) to (%d,%d) size %dx%d\n", __FUNCTION__,
	       srcX, srcY, dstX, dstY, width, height);
*/
	PS3DmaStart(pPS3, PS3ScaledImageChannel, STRETCH_BLIT_CLIP_POINT, 6);
	PS3DmaNext (pPS3, (dstY << 16) | dstX);
	PS3DmaNext (pPS3, (height  << 16) | width);
	PS3DmaNext (pPS3, (dstY << 16) | dstX);
	PS3DmaNext (pPS3, (height  << 16) | width);
	PS3DmaNext (pPS3, 0x00100000);
	PS3DmaNext (pPS3, 0x00100000);

	PS3DmaStart(pPS3, PS3ScaledImageChannel,
		    NV04_SCALED_IMAGE_FROM_MEMORY_SIZE, 4);
	PS3DmaNext (pPS3, copy_src_size);
	PS3DmaNext (pPS3, copy_src_pitch);
	PS3DmaNext (pPS3, copy_src_offset);
	PS3DmaNext (pPS3, ((srcY*16 + 8) << 16) | (srcX*16 + 8));

	PS3DmaKickoff(pPS3); 
}

static void PS3ExaDoneCopy (PixmapPtr pDstPixmap)
{
	ScrnInfoPtr pScrn = xf86Screens[pDstPixmap->drawable.pScreen->myNum];
	PS3Ptr pPS3 = PS3PTR(pScrn);
	CARD32 format;

//	TRACE();

	format = SURFACE_FORMAT_X8R8G8B8;

	PS3DmaStart(pPS3, PS3ContextSurfacesChannel, SURFACE_FORMAT, 1);
	PS3DmaNext (pPS3, format);

	PS3Sync(pScrn);

	exaMarkSync(pDstPixmap->drawable.pScreen);
}

static inline Bool
PS3AccelDownloadM2MF(ScrnInfoPtr pScrn, char *dst, CARD32 src_offset,
				     int dst_pitch, int src_pitch,
				     int line_len, int line_count)
{
	PS3Ptr pPS3 = PS3PTR(pScrn);

	while (line_count) {
		char *src = (char *) pPS3->iof_base;
		int lc, i;

		if (line_count * line_len <= pPS3->iof_size) {
			lc = line_count;
		} else {
			lc = pPS3->iof_size / line_len;
			if (lc > line_count)
				lc = line_count;
		}

		/* HW limitations */
		if (lc > 2047)
			lc = 2047;

		PS3DmaStart(pPS3, PS3MemFormatDownloadChannel,
				NV_MEMORY_TO_MEMORY_FORMAT_OFFSET_IN, 8);
		PS3DmaNext (pPS3, (CARD32) src_offset);
		PS3DmaNext (pPS3, (CARD32) pPS3->iof_offset);
		PS3DmaNext (pPS3, src_pitch);
		PS3DmaNext (pPS3, line_len);
		PS3DmaNext (pPS3, line_len);
		PS3DmaNext (pPS3, lc);
		PS3DmaNext (pPS3, (1<<8)|1);
		PS3DmaNext (pPS3, 0);

		PS3DmaKickoff(pPS3);
		PS3Sync(pScrn);

		if (dst_pitch == line_len) {
			memcpy(dst, src, dst_pitch * lc);
			dst += dst_pitch * lc;
		} else {
			for (i = 0; i < lc; i++) {
				memcpy(dst, src, line_len);
				src += line_len;
				dst += dst_pitch;
			}
		}

		line_count -= lc;
		src_offset += lc * src_pitch;
	}

	return TRUE;
}

static Bool PS3DownloadFromScreen(PixmapPtr pSrc,
				 int x,  int y,
				 int w,  int h,
				 char *dst,  int dst_pitch)
{
	ScrnInfoPtr pScrn = xf86Screens[pSrc->drawable.pScreen->myNum];
	PS3Ptr pPS3 = PS3PTR(pScrn);
	int src_offset, src_pitch, cpp, offset;
	const char *src;

	ErrorF("%s (%d,%d-%dx%d) to %p pitch %d\n", __FUNCTION__,
	       x, y, w, h, dst, dst_pitch);

	src_offset = PS3AccelGetPixmapOffset(pSrc);
	src_pitch  = exaGetPixmapPitch(pSrc);
	cpp = pSrc->drawable.bitsPerPixel >> 3;
	offset = (y * src_pitch) + (x * cpp);

	PS3AccelDownloadM2MF(pScrn, dst,
			     src_offset + offset,
			     dst_pitch, src_pitch, w * cpp, h);
	return TRUE;
}

static inline Bool
PS3AccelUploadM2MF(ScrnInfoPtr pScrn, CARD32 dst_offset, const char *src,
				     int dst_pitch, int src_pitch,
				     int line_len, int line_count)
{
	PS3Ptr pPS3 = PS3PTR(pScrn);

	while (line_count) {
		char *dst = (char *) pPS3->iof_base;
		int lc, i;

		/* Determine max amount of data we can DMA at once */
		if (line_count * line_len <= pPS3->iof_size) {
			lc = line_count;
		} else {
			lc = pPS3->iof_size / line_len;
			if (lc > line_count)
				lc = line_count;
		}

		/* HW limitations */
		if (lc > 2047)
			lc = 2047;

		/* Upload to IOF area */
		if (src_pitch == line_len) {
			memcpy(dst, src, src_pitch * lc);
			src += src_pitch * lc;
		} else {
			for (i = 0; i < lc; i++) {
				memcpy(dst, src, line_len);
				src += src_pitch;
				dst += line_len;
			}
		}

		/* DMA to VRAM */
		PS3DmaStart(pPS3, PS3MemFormatUploadChannel,
				NV_MEMORY_TO_MEMORY_FORMAT_OFFSET_IN, 8);
		PS3DmaNext (pPS3, (CARD32) pPS3->iof_offset);
		PS3DmaNext (pPS3, (CARD32) dst_offset);
		PS3DmaNext (pPS3, line_len);
		PS3DmaNext (pPS3, dst_pitch);
		PS3DmaNext (pPS3, line_len);
		PS3DmaNext (pPS3, lc);
		PS3DmaNext (pPS3, (1<<8)|1);
		PS3DmaNext (pPS3, 0);

		PS3DmaKickoff(pPS3);
		PS3Sync(pScrn);

		dst_offset += lc * dst_pitch;
		line_count -= lc;
	}

	return TRUE;
}

static Bool PS3UploadToScreen(PixmapPtr pDst,
			     int x, int y, int w, int h,
			     char *src, int src_pitch)
{
	ScrnInfoPtr pScrn = xf86Screens[pDst->drawable.pScreen->myNum];
	PS3Ptr pPS3 = PS3PTR(pScrn);
	int dst_offset, dst_pitch, cpp;
	char *dst;

//	ErrorF("%s (%d,%d-%dx%d) from %p pitch %d\n", __FUNCTION__,
//	       x, y, w, h, src, src_pitch);

	dst_offset = PS3AccelGetPixmapOffset(pDst);
	dst_pitch  = exaGetPixmapPitch(pDst);
	cpp = pDst->drawable.bitsPerPixel >> 3;

	/* ImageFromCPU transfer */
/* TODO: ImageFromCPU
	if (w*h*cpp<16*1024)
	{
		int fmt;

		if (PS3AccelGetCtxSurf2DFormatFromPixmap(pDst, &fmt)) {
			if (PS3AccelUploadIFC(pScrn, src, src_pitch, pDst, fmt,
						    x, y, w, h, cpp)) {
				exaMarkSync(pDst->drawable.pScreen);
				return TRUE;
			}
		}
	}
*/

	/* DMA transfer */
	dst_offset += (y * dst_pitch) + (x * cpp);
	PS3AccelUploadM2MF(pScrn, dst_offset, src, dst_pitch,
			   src_pitch, w * cpp, h);

	return TRUE;
}

static Bool PS3CheckComposite(int	op,
			     PicturePtr pSrcPicture,
			     PicturePtr pMaskPicture,
			     PicturePtr pDstPicture)
{
	CARD32 ret = 0;

	TRACE();

	/* PictOpOver doesn't work correctly. The HW command assumes
	 * non premuliplied alpha
	 */
	if (pMaskPicture)
		ret = 0x1;
	else if (op != PictOpOver &&  op != PictOpSrc)
		ret = 0x2;
	else if (!pSrcPicture->pDrawable)
		ret = 0x4;
	else if (pSrcPicture->transform || pSrcPicture->repeat)
		ret = 0x8;
	else if (pSrcPicture->alphaMap || pDstPicture->alphaMap)
		ret = 0x10;
	else if (pSrcPicture->format != PICT_a8r8g8b8 &&
			pSrcPicture->format != PICT_x8r8g8b8 &&
			pSrcPicture->format != PICT_r5g6b5)
		ret = 0x20;
	else if (pDstPicture->format != PICT_a8r8g8b8 &&
			pDstPicture->format != PICT_x8r8g8b8 &&
			pDstPicture->format != PICT_r5g6b5)
		ret = 0x40;

	return ret == 0;
}

static CARD32 src_size, src_pitch, src_offset;

static Bool PS3PrepareComposite(int	  op,
			       PicturePtr pSrcPicture,
			       PicturePtr pMaskPicture,
			       PicturePtr pDstPicture,
			       PixmapPtr  pSrc,
			       PixmapPtr  pMask,
			       PixmapPtr  pDst)
{
	ScrnInfoPtr pScrn = xf86Screens[pSrcPicture->pDrawable->pScreen->myNum];
	PS3Ptr pPS3 = PS3PTR(pScrn);
	int srcFormat, dstFormat;

	TRACE();

	if (pSrcPicture->format == PICT_a8r8g8b8)
		srcFormat = STRETCH_BLIT_FORMAT_A8R8G8B8;
	else if (pSrcPicture->format == PICT_x8r8g8b8)
		srcFormat = STRETCH_BLIT_FORMAT_X8R8G8B8;
	else if (pSrcPicture->format == PICT_r5g6b5)
		srcFormat = STRETCH_BLIT_FORMAT_DEPTH16;
	else
		return FALSE;

	if (!PS3AccelGetCtxSurf2DFormatFromPicture(pDstPicture, &dstFormat))
		return FALSE;
	if (!PS3AccelSetCtxSurf2D(pDst, pDst, dstFormat))
		return FALSE;

	/* blend does not work anymore on NV4x hardware :( */
	if (op != PictOpSrc)
		return FALSE;

	/* memory to screen copy */
	PS3DmaStart(pPS3, PS3ScaledImageChannel,
		    NV04_SCALED_IMAGE_FROM_MEMORY_DMA_IMAGE, 1);
	PS3DmaNext (pPS3, PS3DmaFB);

	PS3DmaStart(pPS3, PS3ScaledImageChannel, STRETCH_BLIT_FORMAT, 2);
	PS3DmaNext (pPS3, srcFormat);
	PS3DmaNext (pPS3, (op == PictOpSrc) ? STRETCH_BLIT_OPERATION_COPY : STRETCH_BLIT_OPERATION_BLEND);
	PS3DmaNext (pPS3, STRETCH_BLIT_OPERATION_COPY);

	src_size = ((pSrcPicture->pDrawable->width+3)&~3) |
		(pSrcPicture->pDrawable->height << 16);
	src_pitch  = exaGetPixmapPitch(pSrc)
		| (STRETCH_BLIT_SRC_FORMAT_ORIGIN_CORNER << 16)
		| (STRETCH_BLIT_SRC_FORMAT_FILTER_POINT_SAMPLE << 24);
	src_offset = PS3AccelGetPixmapOffset(pSrc);

	return TRUE;
}

static void PS3Composite(PixmapPtr pDst,
			int	  srcX,
			int	  srcY,
			int	  maskX,
			int	  maskY,
			int	  dstX,
			int	  dstY,
			int	  width,
			int	  height)
{
	ScrnInfoPtr pScrn = xf86Screens[pDst->drawable.pScreen->myNum];
	PS3Ptr pPS3 = PS3PTR(pScrn);

	TRACE();

	PS3DmaStart(pPS3, PS3ScaledImageChannel, STRETCH_BLIT_CLIP_POINT, 6);
	PS3DmaNext (pPS3, dstX | (dstY << 16));
	PS3DmaNext (pPS3, width | (height << 16));
	PS3DmaNext (pPS3, dstX | (dstY << 16));
	PS3DmaNext (pPS3, width | (height << 16));
	PS3DmaNext (pPS3, 1<<20);
	PS3DmaNext (pPS3, 1<<20);

	PS3DmaStart(pPS3, PS3ScaledImageChannel, STRETCH_BLIT_SRC_SIZE, 4);
	PS3DmaNext (pPS3, src_size);
	PS3DmaNext (pPS3, src_pitch);
	PS3DmaNext (pPS3, src_offset);
	PS3DmaNext (pPS3, srcX | (srcY<<16));

	PS3DmaKickoff(pPS3);
}

static void PS3DoneComposite (PixmapPtr pDst)
{
	ScrnInfoPtr pScrn = xf86Screens[pDst->drawable.pScreen->myNum];
	PS3Ptr pPS3 = PS3PTR(pScrn);
	CARD32 format;

	TRACE();

	format = SURFACE_FORMAT_X8R8G8B8;

	PS3DmaStart(pPS3, PS3ContextSurfacesChannel, SURFACE_FORMAT, 1);
	PS3DmaNext (pPS3, format);

	exaMarkSync(pDst->drawable.pScreen);
}

Bool PS3ExaInit(ScreenPtr pScreen) 
{
	ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
	PS3Ptr pPS3 = PS3PTR(pScrn);

	TRACE();

	if(!(pPS3->EXADriverPtr = (ExaDriverPtr) xnfcalloc(sizeof(ExaDriverRec), 1))) {
		pPS3->NoAccel = TRUE;
		return FALSE;
	}

	pPS3->EXADriverPtr->exa_major = EXA_VERSION_MAJOR;
	pPS3->EXADriverPtr->exa_minor = EXA_VERSION_MINOR;

	pPS3->EXADriverPtr->memoryBase		= (void *) pPS3->vram_base;
	pPS3->EXADriverPtr->offScreenBase	=
		pScrn->virtualX * pScrn->virtualY*(pScrn->bitsPerPixel/8); 
	pPS3->EXADriverPtr->memorySize		= pPS3->vram_size;
	pPS3->EXADriverPtr->pixmapOffsetAlign	= 256; 
	pPS3->EXADriverPtr->pixmapPitchAlign	= 64; 
	pPS3->EXADriverPtr->flags		= EXA_OFFSCREEN_PIXMAPS;
	pPS3->EXADriverPtr->maxX			= 32768;
	pPS3->EXADriverPtr->maxY			= 32768;

	pPS3->EXADriverPtr->WaitMarker = PS3ExaWaitMarker;

	/* Install default hooks */
	pPS3->EXADriverPtr->DownloadFromScreen = PS3DownloadFromScreen; 
	pPS3->EXADriverPtr->UploadToScreen = PS3UploadToScreen; 

	pPS3->EXADriverPtr->PrepareCopy = PS3ExaPrepareCopy;
	pPS3->EXADriverPtr->Copy = PS3ExaCopy;
	pPS3->EXADriverPtr->DoneCopy = PS3ExaDoneCopy;

	pPS3->EXADriverPtr->PrepareSolid = PS3ExaPrepareSolid;
	pPS3->EXADriverPtr->Solid = PS3ExaSolid;
	pPS3->EXADriverPtr->DoneSolid = PS3ExaDoneSolid;

	pPS3->EXADriverPtr->CheckComposite   = PS3CheckComposite;
	pPS3->EXADriverPtr->PrepareComposite = PS3PrepareComposite;
	pPS3->EXADriverPtr->Composite        = PS3Composite;
	pPS3->EXADriverPtr->DoneComposite    = PS3DoneComposite;

	return exaDriverInit(pScreen, pPS3->EXADriverPtr);
}

