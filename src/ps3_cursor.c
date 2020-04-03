/*
 * Copyright (c) 2003 NVIDIA, Corporation
 * Copyright (c) 2019 René Rebe <rene@exactcode.de>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

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
#include "ps3.h"

#include "cursorstr.h"

#include <sys/ioctl.h>
#include <errno.h>
#include <linux/types.h>
#include <linux/fb.h>
#include <asm/ps3fb.h>

/****************************************************************************\
*                                                                            *
*                          HW Cursor Entrypoints                             *
*                                                                            *
\****************************************************************************/

#define TRANSPARENT_PIXEL   0

#define ConvertToRGB555(c) \
(((c & 0xf80000) >> 9 ) | ((c & 0xf800) >> 6 ) | ((c & 0xf8) >> 3 ) | 0x8000)

#define ConvertToRGB888(c) (c | 0xff000000)

#define BYTE_SWAP_32(c)  ((c & 0xff000000) >> 24) |  \
                         ((c & 0xff0000) >> 8) |     \
                         ((c & 0xff00) << 8) |       \
                         ((c & 0xff) << 24)


static void 
ConvertCursor1555(PS3Ptr pNv, CARD32 *src, CARD16 *dst)
{
    CARD32 b, m;
    int i, j;
    
    for ( i = 0; i < 32; i++ ) {
        b = *src++;
        m = *src++;
        for ( j = 0; j < 32; j++ ) {
#if X_BYTE_ORDER == X_BIG_ENDIAN
            if ( m & 0x80000000)
                *dst = ( b & 0x80000000) ? pNv->curFg : pNv->curBg;
            else
                *dst = TRANSPARENT_PIXEL;
            b <<= 1;
            m <<= 1;
#else
            if ( m & 1 )
                *dst = ( b & 1) ? pNv->curFg : pNv->curBg;
            else
                *dst = TRANSPARENT_PIXEL;
            b >>= 1;
            m >>= 1;
#endif
            dst++;
        }
    }
}


static void
ConvertCursor8888(PS3Ptr pNv, CARD32 *src, CARD32 *dst)
{
    CARD32 b, m;
    int i, j;
   
    for ( i = 0; i < 128; i++ ) {
        b = *src++;
        m = *src++;
        for ( j = 0; j < 32; j++ ) {
#if X_BYTE_ORDER == X_BIG_ENDIAN
            if ( m & 0x80000000)
                *dst = ( b & 0x80000000) ? pNv->curFg : pNv->curBg;
            else
                *dst = TRANSPARENT_PIXEL;
            b <<= 1;
            m <<= 1;
#else
            if ( m & 1 )
                *dst = ( b & 1) ? pNv->curFg : pNv->curBg;
            else
                *dst = TRANSPARENT_PIXEL;
            b >>= 1;
            m >>= 1;
#endif
            dst++;
        }
    }
}


static void
TransformCursor (PS3Ptr pNv)
{
    CARD32 *tmp;
    int i, dwords;

    /* convert to color cursor */
    if(pNv->alphaCursor) {
       dwords = 64 * 64;
       if(!(tmp = calloc(1, dwords * 4))) return;
       ConvertCursor8888(pNv, pNv->curImage, tmp);
    } else {
       dwords = (32 * 32) >> 1;
       if(!(tmp = calloc(1, dwords * 4))) return;
       ConvertCursor1555(pNv, pNv->curImage, (CARD16*)tmp);
    }

    for(i = 0; i < dwords; i++)
        pNv->CURSOR[i] = tmp[i];

    free(tmp);
}

static void
NVLoadCursorImage(ScrnInfoPtr pScrn, unsigned char *src)
{
    PS3Ptr pNv = PS3PTR(pScrn);

    /* save copy of image for color changes */
    memcpy(pNv->curImage, src, (pNv->alphaCursor) ? 1024 : 256);

    TransformCursor(pNv);
}

static void
NVSetCursorPosition(ScrnInfoPtr pScrn, int x, int y)
{
    PS3Ptr pPS3 = PS3PTR(pScrn);
    int ret, val = (y << 16) | (x & 0xffff);
    ret = ioctl(pPS3->fd, PS3FB_IOCTL_CURSOR_POS, val);
    if (ret < 0) {
	ErrorF("ioctl: %s", strerror(errno));
    }
}

static void
NVSetCursorColors(ScrnInfoPtr pScrn, int bg, int fg)
{
    PS3Ptr pNv = PS3PTR(pScrn);
    CARD32 fore, back;

    if(pNv->alphaCursor) {
        fore = ConvertToRGB888(fg);
        back = ConvertToRGB888(bg);
#if X_BYTE_ORDER == X_BIG_ENDIAN
#if 0
        if((pNv->Chipset & 0x0ff0) == 0x0110) {
           fore = BYTE_SWAP_32(fore);
           back = BYTE_SWAP_32(back);
        }
#endif
#endif
    } else {
        fore = ConvertToRGB555(fg);
        back = ConvertToRGB555(bg);
#if X_BYTE_ORDER == X_BIG_ENDIAN
#if 0
        if((pNv->Chipset & 0x0ff0) == 0x0110) {
           fore = ((fore & 0xff) << 8) | (fore >> 8);
           back = ((back & 0xff) << 8) | (back >> 8);
        }
#endif
#endif
   }

    if ((pNv->curFg != fore) || (pNv->curBg != back)) {
        pNv->curFg = fore;
        pNv->curBg = back;
            
        TransformCursor(pNv);
    }
}


int NVShowHideCursor (PS3Ptr pPS3, int   ShowHide)
{
    int	ret = ioctl(pPS3->fd, PS3FB_IOCTL_CURSOR_ENABLE, ShowHide);
    ErrorF("ioctl enable: %d\n", ret);
    if (ret < 0) {
	ErrorF("ioctl: %s", strerror(errno));
    }

    return ShowHide;
}

static void 
NVShowCursor(ScrnInfoPtr pScrn)
{
    PS3Ptr pPS3 = PS3PTR(pScrn);
 
    int ret = ioctl(pPS3->fd, PS3FB_IOCTL_CURSOR_OFFS, pPS3->cursor_start);
    if (ret < 0) {
        ErrorF("ioctl offset: %s", strerror(errno)); 
    }

    /* Enable cursor */
    NVShowHideCursor(pPS3, 1);
}

static void
NVHideCursor(ScrnInfoPtr pScrn)
{
    PS3Ptr pNv = PS3PTR(pScrn);
    /* Disable cursor */
    NVShowHideCursor(pNv, 0);
}

static Bool 
NVUseHWCursor(ScreenPtr pScreen, CursorPtr pCurs)
{
    return TRUE;
}

#ifdef ARGB_CURSOR
static Bool 
NVUseHWCursorARGB(ScreenPtr pScreen, CursorPtr pCurs)
{
    if((pCurs->bits->width <= 64) && (pCurs->bits->height <= 64))
        return TRUE;

    return FALSE;
}

static void
NVLoadCursorARGB(ScrnInfoPtr pScrn, CursorPtr pCurs)
{
    PS3Ptr pNv = PS3PTR(pScrn);
    CARD32 *image = pCurs->bits->argb;
    CARD32 *dst = (CARD32*)pNv->CURSOR;
    CARD32 alpha, tmp;
    int x, y, w, h;

    w = pCurs->bits->width;
    h = pCurs->bits->height;

    if (1) {  /* premultiply */
       for(y = 0; y < h; y++) {
          for(x = 0; x < w; x++) {
             alpha = *image >> 24;
             if(alpha == 0xff)
                tmp = *image;
             else {
                tmp = (alpha << 24) |
                         (((*image & 0xff) * alpha) / 255) |
                        ((((*image & 0xff00) * alpha) / 255) & 0xff00) |
                       ((((*image & 0xff0000) * alpha) / 255) & 0xff0000); 
             }
             image++;
#if X_BYTE_ORDER == X_BIG_ENDIAN
             *dst++ = BYTE_SWAP_32(tmp);
#else
             *dst++ = tmp;
#endif
         }
         for(; x < 64; x++)
             *dst++ = 0;
      }
    } else {
       for(y = 0; y < h; y++) {
          for(x = 0; x < w; x++)
              *dst++ = *image++;
          for(; x < 64; x++)
              *dst++ = 0;
       }
    }

    if(y < 64)
      memset(dst, 0, 64 * (64 - y) * 4);
}
#endif

Bool 
NVCursorInit(ScreenPtr pScreen)
{
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
    PS3Ptr pNv = PS3PTR(pScrn);
    xf86CursorInfoPtr infoPtr;

    infoPtr = xf86CreateCursorInfoRec();
    if(!infoPtr) return FALSE;
    
    pNv->CursorInfoRec = infoPtr;

    if(pNv->alphaCursor)
       infoPtr->MaxWidth = infoPtr->MaxHeight = 64;
    else
       infoPtr->MaxWidth = infoPtr->MaxHeight = 32;

    infoPtr->Flags = HARDWARE_CURSOR_TRUECOLOR_AT_8BPP |
                     HARDWARE_CURSOR_SOURCE_MASK_INTERLEAVE_32; 
    infoPtr->SetCursorColors = NVSetCursorColors;
    infoPtr->SetCursorPosition = NVSetCursorPosition;
    infoPtr->LoadCursorImage = NVLoadCursorImage;
    infoPtr->HideCursor = NVHideCursor;
    infoPtr->ShowCursor = NVShowCursor;
    infoPtr->UseHWCursor = NVUseHWCursor;

#ifdef ARGB_CURSOR
    if(pNv->alphaCursor) {
       infoPtr->UseHWCursorARGB = NVUseHWCursorARGB;
       infoPtr->LoadCursorARGB = NVLoadCursorARGB;
    }
#endif

    return xf86InitCursor(pScreen, infoPtr);
}
