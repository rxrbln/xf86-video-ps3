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

//#include "nv_include.h"
#include "xf86.h"
#include "exa.h"

#include "ps3_gpu.h"
#include "ps3.h"
#include "ps3_dma.h"

#include "nouveau_reg.h"

#include <sys/time.h>

Bool
PS3AccelGetCtxSurf2DFormatFromPixmap(PixmapPtr pPix, int *fmt_ret)
{
	switch (pPix->drawable.bitsPerPixel) {
	case 32:
		*fmt_ret = SURFACE_FORMAT_A8R8G8B8;
		break;
	case 24:
		*fmt_ret = SURFACE_FORMAT_X8R8G8B8;
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

CARD32
PS3AccelGetPixmapOffset(PixmapPtr pPix)
{
	ScrnInfoPtr pScrn = xf86Screens[pPix->drawable.pScreen->myNum];
	PS3Ptr pPS3 = PS3PTR(pScrn);
	unsigned long offset;

	offset = exaGetPixmapOffset(pPix);
	if (offset >= pPS3->gpu->vram_size) {
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
	ErrorF("%s\n", __FUNCTION__);
	PS3Sync(xf86Screens[pScreen->myNum]);
}

static Bool PS3ExaPrepareSolid(PixmapPtr pPixmap,
			      int   alu,
			      Pixel planemask,
			      Pixel fg)
{
	ErrorF("%s\n", __FUNCTION__);
	return FALSE;
}

static void PS3ExaSolid (PixmapPtr pPixmap, int x1, int y1, int x2, int y2)
{
	ErrorF("%s\n", __FUNCTION__);
}

static void PS3ExaDoneSolid (PixmapPtr pPixmap)
{
	ErrorF("%s\n", __FUNCTION__);
}

static CARD32 src_size, src_pitch, src_offset;

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

	ErrorF("%s %d %d %d\n", __FUNCTION__, dx, dy, alu);

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
		    NV10_SCALED_IMAGE_FROM_MEMORY_SET_DMA_IN_MEMORY, 1);
	PS3DmaNext (pPS3, PS3DmaFB);

	PS3DmaStart(pPS3, PS3ScaledImageChannel, STRETCH_BLIT_FORMAT, 2);
	PS3DmaNext (pPS3, srcFormat);
	PS3DmaNext (pPS3, STRETCH_BLIT_OPERATION_COPY);

	src_size = ((pSrcPixmap->drawable.width+3)&~3) |
		(pSrcPixmap->drawable.height << 16);
	src_pitch  = exaGetPixmapPitch(pSrcPixmap)
		| (STRETCH_BLIT_SRC_FORMAT_ORIGIN_CORNER << 16)
		| (STRETCH_BLIT_SRC_FORMAT_FILTER_POINT_SAMPLE << 24);
	src_offset = PS3AccelGetPixmapOffset(pSrcPixmap);


	ErrorF("%s sfmt=%d dfmt=%d dpitch=%d spitch=%d soffset=0x%x doffset=0x%x\n", __FUNCTION__,
	       srcFormat, dstFormat,
	       exaGetPixmapPitch(pDstPixmap),
	       exaGetPixmapPitch(pSrcPixmap),
	       PS3AccelGetPixmapOffset(pSrcPixmap),
	       PS3AccelGetPixmapOffset(pDstPixmap));

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

	ErrorF("%s from (%d,%d) to (%d,%d) size %dx%d\n", __FUNCTION__,
	       srcX, srcY, dstX, dstY, width, height);

	PS3DmaStart(pPS3, PS3ScaledImageChannel, STRETCH_BLIT_CLIP_POINT, 6);
	PS3DmaNext (pPS3, (dstY << 16) | dstX);
	PS3DmaNext (pPS3, (height  << 16) | width);
	PS3DmaNext (pPS3, (dstY << 16) | dstX);
	PS3DmaNext (pPS3, (height  << 16) | width);
	PS3DmaNext (pPS3, 0x00100000);
	PS3DmaNext (pPS3, 0x00100000);

	PS3DmaStart(pPS3, PS3ScaledImageChannel,
		    NV04_SCALED_IMAGE_FROM_MEMORY_SIZE, 4);
	PS3DmaNext (pPS3, src_size);
	PS3DmaNext (pPS3, src_pitch);
	PS3DmaNext (pPS3, src_offset);
	PS3DmaNext (pPS3, ((srcY*16 + 8) << 16) | (srcX*16 + 8));

	PS3DmaKickoff(pPS3); 
}

static void PS3ExaDoneCopy (PixmapPtr pDstPixmap)
{
	ScrnInfoPtr pScrn = xf86Screens[pDstPixmap->drawable.pScreen->myNum];
	PS3Ptr pPS3 = PS3PTR(pScrn);
	CARD32 format;

	ErrorF("%s\n", __FUNCTION__);

	format = SURFACE_FORMAT_X8R8G8B8;

	PS3DmaStart(pPS3, PS3ContextSurfacesChannel, SURFACE_FORMAT, 1);
	PS3DmaNext (pPS3, format);

	PS3Sync(pScrn);

	exaMarkSync(pDstPixmap->drawable.pScreen);
}

static Bool PS3DownloadFromScreen(PixmapPtr pSrc,
				 int x,  int y,
				 int w,  int h,
				 char *dst,  int dst_pitch)
{
	ErrorF("%s\n", __FUNCTION__);
	return FALSE;
}

static Bool PS3UploadToScreen(PixmapPtr pDst,
			     int x, int y, int w, int h,
			     char *src, int src_pitch)
{
	ErrorF("%s\n", __FUNCTION__);
	return FALSE;
}

Bool PS3ExaInit(ScreenPtr pScreen) 
{
	ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
	PS3Ptr pPS3 = PS3PTR(pScrn);

	ErrorF("%s\n", __FUNCTION__);

	if(!(pPS3->EXADriverPtr = (ExaDriverPtr) xnfcalloc(sizeof(ExaDriverRec), 1))) {
		pPS3->NoAccel = TRUE;
		return FALSE;
	}

	pPS3->EXADriverPtr->exa_major = EXA_VERSION_MAJOR;
	pPS3->EXADriverPtr->exa_minor = EXA_VERSION_MINOR;

	pPS3->EXADriverPtr->memoryBase		= pPS3->gpu->vram_base;
	pPS3->EXADriverPtr->offScreenBase	=
		pScrn->virtualX * pScrn->virtualY*(pScrn->bitsPerPixel/8); 
	pPS3->EXADriverPtr->memorySize		= pPS3->gpu->vram_size;
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

#if 0 // TODO
	pPS3->EXADriverPtr->CheckComposite   = PS3EXACheckComposite;
	pPS3->EXADriverPtr->PrepareComposite = PS3EXAPrepareComposite;
	pPS3->EXADriverPtr->Composite        = PS3EXAComposite;
	pPS3->EXADriverPtr->DoneComposite    = PS3EXADoneComposite;
#endif

	return exaDriverInit(pScreen, pPS3->EXADriverPtr);
}

