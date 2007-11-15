/*
 * Copyright 2007 Ben Skeggs
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF
 * OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "xf86.h"
#include "exa.h"
#include "xf86xv.h"
#include "ps3.h"
#include "ps3_dma.h"

#include <stdint.h>
#include "nouveau_class.h"
#include "nv_shaders.h"

#include <sys/time.h>
#include <string.h>

typedef struct nv_pict_surface_format {
	int	 pict_fmt;
	uint32_t card_fmt;
} nv_pict_surface_format_t;

typedef struct nv_pict_texture_format {
	int	 pict_fmt;
	uint32_t card_fmt;
	uint32_t card_swz;
} nv_pict_texture_format_t;

typedef struct nv_pict_op {
	Bool	 src_alpha;
	Bool	 dst_alpha;
	uint32_t src_card_op;
	uint32_t dst_card_op;
} nv_pict_op_t;

typedef struct nv40_exa_state {
	Bool have_mask;

	struct {
		PictTransformPtr transform;
		float width;
		float height;
	} unit[2];
} nv40_exa_state_t;
static nv40_exa_state_t exa_state;
#define NV40EXA_STATE nv40_exa_state_t *state = &exa_state

static nv_pict_surface_format_t
NV40SurfaceFormat[] = {
	{ PICT_a8r8g8b8	, NV40TCL_RT_FORMAT_COLOR_A8R8G8B8 },
	{ PICT_x8r8g8b8	, NV40TCL_RT_FORMAT_COLOR_X8R8G8B8 },
	{ PICT_r5g6b5	, NV40TCL_RT_FORMAT_COLOR_R5G6B5   },
	{ PICT_a8       , NV40TCL_RT_FORMAT_COLOR_B8       },
	{ -1, ~0 }
};

static nv_pict_surface_format_t *
NV40_GetPictSurfaceFormat(int format)
{
	int i = 0;

	while (NV40SurfaceFormat[i].pict_fmt != -1) {
		if (NV40SurfaceFormat[i].pict_fmt == format)
			return &NV40SurfaceFormat[i];
		i++;
	}

	return NULL;
}

enum {
	NV40EXA_FPID_PASS_COL0 = 0,
	NV40EXA_FPID_PASS_TEX0 = 1,
	NV40EXA_FPID_COMPOSITE_MASK = 2,
	NV40EXA_FPID_COMPOSITE_MASK_SA_CA = 3,
	NV40EXA_FPID_COMPOSITE_MASK_CA = 4,
	NV40EXA_FPID_MAX = 5
} NV40EXA_FPID;

static nv_shader_t *nv40_fp_map[NV40EXA_FPID_MAX] = {
	&nv30_fp_pass_col0,
	&nv30_fp_pass_tex0,
	&nv30_fp_composite_mask,
	&nv30_fp_composite_mask_sa_ca,
	&nv30_fp_composite_mask_ca
};

static nv_shader_t *nv40_fp_map_a8[NV40EXA_FPID_MAX];

void
NV40EXAHackupA8Shaders(void)
{
	int s;

	for (s = 0; s < NV40EXA_FPID_MAX; s++) {
		nv_shader_t *def, *a8;

		def = nv40_fp_map[s];
		a8 = xcalloc(1, sizeof(nv_shader_t));
		a8->card_priv.NV30FP.num_regs = def->card_priv.NV30FP.num_regs;
		a8->size = def->size + 4;
		memcpy(a8->data, def->data, def->size * sizeof(uint32_t));
		nv40_fp_map_a8[s] = a8;

		a8->data[a8->size - 8 + 0] &= ~0x00000081;
		a8->data[a8->size - 4 + 0]  = 0x01401e81;
		a8->data[a8->size - 4 + 1]  = 0x1c9dfe00;
		a8->data[a8->size - 4 + 2]  = 0x0001c800;
		a8->data[a8->size - 4 + 3]  = 0x0001c800;
	}
}

#define _(r,tf,ts0x,ts0y,ts0z,ts0w,ts1x,ts1y,ts1z,ts1w)                        \
  {                                                                            \
  PICT_##r, NV40TCL_TEX_FORMAT_FORMAT_##tf,                                    \
  NV40TCL_TEX_SWIZZLE_S0_X_##ts0x | NV40TCL_TEX_SWIZZLE_S0_Y_##ts0y |          \
  NV40TCL_TEX_SWIZZLE_S0_Z_##ts0z | NV40TCL_TEX_SWIZZLE_S0_W_##ts0w |          \
  NV40TCL_TEX_SWIZZLE_S1_X_##ts1x | NV40TCL_TEX_SWIZZLE_S1_Y_##ts1y |          \
  NV40TCL_TEX_SWIZZLE_S1_Z_##ts1z | NV40TCL_TEX_SWIZZLE_S1_W_##ts1w,           \
  }
static nv_pict_texture_format_t
NV40TextureFormat[] = {
        _(a8r8g8b8, A8R8G8B8,   S1,   S1,   S1,   S1, X, Y, Z, W),
        _(x8r8g8b8, A8R8G8B8,   S1,   S1,   S1,  ONE, X, Y, Z, W),
        _(x8b8g8r8, A8R8G8B8,   S1,   S1,   S1,  ONE, Z, Y, X, W),
        _(a1r5g5b5, A1R5G5B5,   S1,   S1,   S1,   S1, X, Y, Z, W),
        _(x1r5g5b5, A1R5G5B5,   S1,   S1,   S1,  ONE, X, Y, Z, W),
        _(  r5g6b5,   R5G6B5,   S1,   S1,   S1,   S1, X, Y, Z, W),
        _(      a8,       L8, ZERO, ZERO, ZERO,   S1, X, X, X, X),
        { -1, ~0, ~0 }
};
#undef _

static nv_pict_texture_format_t *
NV40_GetPictTextureFormat(int format)
{
	int i = 0;

	while (NV40TextureFormat[i].pict_fmt != -1) {
		if (NV40TextureFormat[i].pict_fmt == format)
			return &NV40TextureFormat[i];
		i++;
	}

	return NULL;
}

#define SF(bf) (NV40TCL_BLEND_FUNC_SRC_RGB_##bf |                              \
		NV40TCL_BLEND_FUNC_SRC_ALPHA_##bf)
#define DF(bf) (NV40TCL_BLEND_FUNC_DST_RGB_##bf |                              \
		NV40TCL_BLEND_FUNC_DST_ALPHA_##bf)
static nv_pict_op_t 
NV40PictOp[] = {
/* Clear       */ { 0, 0, SF(               ZERO), DF(               ZERO) },
/* Src         */ { 0, 0, SF(                ONE), DF(               ZERO) },
/* Dst         */ { 0, 0, SF(               ZERO), DF(                ONE) },
/* Over        */ { 1, 0, SF(                ONE), DF(ONE_MINUS_SRC_ALPHA) },
/* OverReverse */ { 0, 1, SF(ONE_MINUS_DST_ALPHA), DF(                ONE) },
/* In          */ { 0, 1, SF(          DST_ALPHA), DF(               ZERO) },
/* InReverse   */ { 1, 0, SF(               ZERO), DF(          SRC_ALPHA) },
/* Out         */ { 0, 1, SF(ONE_MINUS_DST_ALPHA), DF(               ZERO) },
/* OutReverse  */ { 1, 0, SF(               ZERO), DF(ONE_MINUS_SRC_ALPHA) },
/* Atop        */ { 1, 1, SF(          DST_ALPHA), DF(ONE_MINUS_SRC_ALPHA) },
/* AtopReverse */ { 1, 1, SF(ONE_MINUS_DST_ALPHA), DF(          SRC_ALPHA) },
/* Xor         */ { 1, 1, SF(ONE_MINUS_DST_ALPHA), DF(ONE_MINUS_SRC_ALPHA) },
/* Add         */ { 0, 0, SF(                ONE), DF(                ONE) }
};

static nv_pict_op_t *
NV40_GetPictOpRec(int op)
{
	if (op >= PictOpSaturate)
		return NULL;
	return &NV40PictOp[op];
}

#if 0
#define FALLBACK(fmt,args...) do {					\
	ErrorF("FALLBACK %s:%d> " fmt, __func__, __LINE__, ##args);	\
	return FALSE;							\
} while(0)
#else
#define FALLBACK(fmt,args...) do { \
	return FALSE;              \
} while(0)
#endif
//#define TRACE() ErrorF("%s:%d\n", __func__, __LINE__)
#define TRACE()

extern void NV40_LoadVtxProg(PS3Ptr pPS3, nv_shader_t *shader);
extern int NV40_LoadFragProg(PS3Ptr pPS3, nv_shader_t *shader);

static void
NV40_SetupBlend(ScrnInfoPtr pScrn, nv_pict_op_t *blend,
		CARD32 dest_format, Bool component_alpha)
{
	PS3Ptr pPS3 = PS3PTR(pScrn);
	uint32_t sblend, dblend;

	sblend = blend->src_card_op;
	dblend = blend->dst_card_op;

	if (blend->dst_alpha) {
		if (!PICT_FORMAT_A(dest_format)) {
			if (sblend == SF(DST_ALPHA)) {
				sblend = SF(ONE);
			} else if (sblend == SF(ONE_MINUS_DST_ALPHA)) {
				sblend = SF(ZERO);
			}
		} else if (dest_format == PICT_a8) {
			if (sblend == SF(DST_ALPHA)) {
				sblend = SF(DST_COLOR);
			} else if (sblend == SF(ONE_MINUS_DST_ALPHA)) {
				sblend = SF(ONE_MINUS_DST_COLOR);
			}
		}
	}

	if (blend->src_alpha && (component_alpha || dest_format == PICT_a8)) {
		if (dblend == DF(SRC_ALPHA)) {
			dblend = DF(SRC_COLOR);
		} else if (dblend == DF(ONE_MINUS_SRC_ALPHA)) {
			dblend = DF(ONE_MINUS_SRC_COLOR);
		}
	}

	if (sblend == SF(ONE) && dblend == DF(ZERO)) {
		BEGIN_RING(PS3TCLChannel, NV40TCL_BLEND_ENABLE, 1);
		OUT_RING  (0);
	} else {
		BEGIN_RING(PS3TCLChannel, NV40TCL_BLEND_ENABLE, 5);
		OUT_RING  (1);
		OUT_RING  (sblend);
		OUT_RING  (dblend);
		OUT_RING  (0x00000000);
		OUT_RING  (NV40TCL_BLEND_EQUATION_ALPHA_FUNC_ADD |
			   NV40TCL_BLEND_EQUATION_RGB_FUNC_ADD);
	}
}

static Bool
NV40EXATexture(ScrnInfoPtr pScrn, PixmapPtr pPix, PicturePtr pPict, int unit)
{
	PS3Ptr pPS3 = PS3PTR(pScrn);
	nv_pict_texture_format_t *fmt;
	NV40EXA_STATE;

	fmt = NV40_GetPictTextureFormat(pPict->format);
	if (!fmt)
		return FALSE;

	BEGIN_RING(PS3TCLChannel, NV40TCL_TEX_OFFSET(unit), 8);
	OUT_RING  (PS3AccelGetPixmapOffset(pPix));
	OUT_RING  (fmt->card_fmt | NV40TCL_TEX_FORMAT_LINEAR |
		   NV40TCL_TEX_FORMAT_DIMS_2D | NV40TCL_TEX_FORMAT_DMA0 |
		   NV40TCL_TEX_FORMAT_NO_BORDER | (0x8000) |
		   (1 << NV40TCL_TEX_FORMAT_MIPMAP_COUNT_SHIFT));
	if (pPict->repeat && pPict->repeatType == RepeatNormal) {
		OUT_RING  (NV40TCL_TEX_WRAP_S_REPEAT |
			   NV40TCL_TEX_WRAP_T_REPEAT |
			   NV40TCL_TEX_WRAP_R_REPEAT);
	} else {
		OUT_RING  (NV40TCL_TEX_WRAP_S_CLAMP_TO_EDGE |
			   NV40TCL_TEX_WRAP_T_CLAMP_TO_EDGE |
			   NV40TCL_TEX_WRAP_R_CLAMP_TO_EDGE);
	}
	OUT_RING  (NV40TCL_TEX_ENABLE_ENABLE);
	OUT_RING  (fmt->card_swz);
	if (pPict->filter == PictFilterBilinear) {
		OUT_RING  (NV40TCL_TEX_FILTER_MIN_LINEAR |
			   NV40TCL_TEX_FILTER_MAG_LINEAR |
			   0x3fd6);
	} else {
		OUT_RING  (NV40TCL_TEX_FILTER_MIN_NEAREST |
			   NV40TCL_TEX_FILTER_MAG_NEAREST |
			   0x3fd6);
	}
	OUT_RING  ((pPix->drawable.width << 16) | pPix->drawable.height);
	OUT_RING  (0); /* border ARGB */
	BEGIN_RING(PS3TCLChannel, NV40TCL_TEX_SIZE1(unit), 1);
	OUT_RING  ((1 << NV40TCL_TEX_SIZE1_DEPTH_SHIFT) |
		   (uint32_t)exaGetPixmapPitch(pPix));

	state->unit[unit].width		= (float)pPix->drawable.width;
	state->unit[unit].height	= (float)pPix->drawable.height;
	state->unit[unit].transform	= pPict->transform;

	return TRUE;
}

static Bool
NV40_SetupSurface(ScrnInfoPtr pScrn, PixmapPtr pPix, CARD32 format)
{
	PS3Ptr pPS3 = PS3PTR(pScrn);
	nv_pict_surface_format_t *fmt;

	fmt = NV40_GetPictSurfaceFormat(format);
	if (!fmt) {
		ErrorF("AIII no format\n");
		return FALSE;
	}

        uint32_t pitch = (uint32_t)exaGetPixmapPitch(pPix);

	BEGIN_RING(PS3TCLChannel, NV40TCL_RT_FORMAT, 3);
	OUT_RING  (NV40TCL_RT_FORMAT_TYPE_LINEAR |
		   NV40TCL_RT_FORMAT_ZETA_Z24S8 |
		   fmt->card_fmt);
	OUT_RING  (pitch);
	OUT_RING  (PS3AccelGetPixmapOffset(pPix));

	return TRUE;
}

static Bool
NV40EXACheckCompositeTexture(PicturePtr pPict)
{
	nv_pict_texture_format_t *fmt;
	int w = pPict->pDrawable->width;
	int h = pPict->pDrawable->height;

	TRACE();

	if ((w > 4096) || (h > 4096))
		FALLBACK("picture too large, %dx%d\n", w, h);

	fmt = NV40_GetPictTextureFormat(pPict->format);
	if (!fmt)
		FALLBACK("picture format 0x%08x not supported\n",
				pPict->format);

	if (pPict->filter != PictFilterNearest &&
	    pPict->filter != PictFilterBilinear)
		FALLBACK("filter 0x%x not supported\n", pPict->filter);

	if (pPict->repeat &&
	    (pPict->repeat != RepeatNormal && pPict->repeatType != RepeatNone))
		FALLBACK("repeat 0x%x not supported\n", pPict->repeatType);

	return TRUE;
}

Bool
NV40EXACheckComposite(int op, PicturePtr psPict,
			      PicturePtr pmPict,
			      PicturePtr pdPict)
{
	nv_pict_surface_format_t *fmt;
	nv_pict_op_t *opr;

	TRACE();

	opr = NV40_GetPictOpRec(op);
	if (!opr)
		FALLBACK("unsupported blend op 0x%x\n", op);

	fmt = NV40_GetPictSurfaceFormat(pdPict->format);
	if (!fmt)
		FALLBACK("dst picture format 0x%08x not supported\n",
				pdPict->format);

	if (!NV40EXACheckCompositeTexture(psPict))
		FALLBACK("src picture\n");
	if (pmPict) {
		if (pmPict->componentAlpha && 
		    PICT_FORMAT_RGB(pmPict->format) &&
		    opr->src_alpha && opr->src_card_op != SF(ZERO))
			FALLBACK("mask CA + SA\n");
		if (!NV40EXACheckCompositeTexture(pmPict))
			FALLBACK("mask picture\n");
	}

	return TRUE;
}

Bool
NV40EXAPrepareComposite(int op, PicturePtr psPict,
				PicturePtr pmPict,
				PicturePtr pdPict,
				PixmapPtr  psPix,
				PixmapPtr  pmPix,
				PixmapPtr  pdPix)
{
	ScrnInfoPtr pScrn = xf86Screens[psPix->drawable.pScreen->myNum];
	PS3Ptr pPS3 = PS3PTR(pScrn);
	nv_pict_op_t *blend;
	int fpid = NV40EXA_FPID_PASS_COL0;
	NV40EXA_STATE;

	TRACE();


	if (pdPict->format == PICT_a8) {
		FALLBACK("do not support a8 dest format\n");
	}

	blend = NV40_GetPictOpRec(op);

	NV40_SetupBlend(pScrn, blend, pdPict->format,
			(pmPict && pmPict->componentAlpha &&
			 PICT_FORMAT_RGB(pmPict->format)));

	NV40_SetupSurface(pScrn, pdPix, pdPict->format);
	NV40EXATexture(pScrn, psPix, psPict, 0);

	NV40_LoadVtxProg(pPS3, &nv40_vp_exa_render);

	if (pmPict) {
		NV40EXATexture(pScrn, pmPix, pmPict, 1);
		if (pmPict->componentAlpha && PICT_FORMAT_RGB(pmPict->format)) {
			if (blend->src_alpha)
				fpid = NV40EXA_FPID_COMPOSITE_MASK_SA_CA;
			else
				fpid = NV40EXA_FPID_COMPOSITE_MASK_CA;
		} else {
			fpid = NV40EXA_FPID_COMPOSITE_MASK;
		}
		state->have_mask = TRUE;
	} else {
		fpid = NV40EXA_FPID_PASS_TEX0;
		state->have_mask = FALSE;
	}

	if (pdPict->format == PICT_a8)
		NV40_LoadFragProg(pPS3, nv40_fp_map_a8[fpid]);
	else
		NV40_LoadFragProg(pPS3, nv40_fp_map[fpid]);

	/* Appears to be some kind of cache flush, needed here at least
	 * sometimes.. funky text rendering otherwise :)
	 */
	BEGIN_RING(PS3TCLChannel, NV40TCL_TEX_CACHE_CTL, 1);
	OUT_RING  (2);
	BEGIN_RING(PS3TCLChannel, NV40TCL_TEX_CACHE_CTL, 1);
	OUT_RING  (1);

	BEGIN_RING(PS3TCLChannel, NV40TCL_BEGIN_END, 1);
	OUT_RING  (NV40TCL_BEGIN_END_QUADS);

	return TRUE;
}

#define xFixedToFloat(v) \
	((float)xFixedToInt((v)) + ((float)xFixedFrac(v) / 65536.0))

static void
NV40EXATransformCoord(PictTransformPtr t, int x, int y, float sx, float sy,
					  float *x_ret, float *y_ret)
{
	PictVector v;

	if (t) {
		v.vector[0] = IntToxFixed(x);
		v.vector[1] = IntToxFixed(y);
		v.vector[2] = xFixed1;
		PictureTransformPoint(t, &v);
		*x_ret = xFixedToFloat(v.vector[0]) / sx;
		*y_ret = xFixedToFloat(v.vector[1]) / sy;
	} else {
		*x_ret = (float)x / sx;
		*y_ret = (float)y / sy;
	}
}

#define CV_OUTm(sx,sy,mx,my,dx,dy) do {                                        \
	BEGIN_RING(PS3TCLChannel, NV40TCL_VTX_ATTR_2F_X(8), 4);                         \
	OUT_RINGf ((sx)); OUT_RINGf ((sy));                                    \
	OUT_RINGf ((mx)); OUT_RINGf ((my));                                    \
	BEGIN_RING(PS3TCLChannel, NV40TCL_VTX_ATTR_2I(0), 1);                           \
	OUT_RING  (((dy)<<16)|(dx));                                           \
} while(0)
#define CV_OUT(sx,sy,dx,dy) do {                                               \
	BEGIN_RING(PS3TCLChannel, NV40TCL_VTX_ATTR_2F_X(8), 2);                         \
	OUT_RINGf ((sx)); OUT_RINGf ((sy));                                    \
	BEGIN_RING(PS3TCLChannel, NV40TCL_VTX_ATTR_2I(0), 1);                           \
	OUT_RING  (((dy)<<16)|(dx));                                           \
} while(0)

void
NV40EXAComposite(PixmapPtr pdPix, int srcX , int srcY,
				  int maskX, int maskY,
				  int dstX , int dstY,
				  int width, int height)
{
	ScrnInfoPtr pScrn = xf86Screens[pdPix->drawable.pScreen->myNum];
	PS3Ptr pPS3 = PS3PTR(pScrn);
	float sX0, sX1, sY0, sY1;
	float mX0, mX1, mY0, mY1;
	NV40EXA_STATE;

	TRACE();

	NV40EXATransformCoord(state->unit[0].transform, srcX, srcY,
			      state->unit[0].width,
			      state->unit[0].height, &sX0, &sY0);
	NV40EXATransformCoord(state->unit[0].transform,
			      srcX + width, srcY + height,
			      state->unit[0].width,
			      state->unit[0].height, &sX1, &sY1);

	if (state->have_mask) {
		NV40EXATransformCoord(state->unit[1].transform, maskX, maskY,
				      state->unit[1].width,
				      state->unit[1].height, &mX0, &mY0);
		NV40EXATransformCoord(state->unit[1].transform,
				      maskX + width, maskY + height,
				      state->unit[1].width,
				      state->unit[1].height, &mX1, &mY1);
		CV_OUTm(sX0 , sY0 , mX0, mY0, dstX        ,          dstY);
		CV_OUTm(sX1 , sY0 , mX1, mY0, dstX + width,          dstY);
		CV_OUTm(sX1 , sY1 , mX1, mY1, dstX + width, dstY + height);
		CV_OUTm(sX0 , sY1 , mX0, mY1, dstX        , dstY + height);
	} else {
		CV_OUT(sX0 , sY0 , dstX        ,          dstY);
		CV_OUT(sX1 , sY0 , dstX + width,          dstY);
		CV_OUT(sX1 , sY1 , dstX + width, dstY + height);
		CV_OUT(sX0 , sY1 , dstX        , dstY + height);
	}

	FIRE_RING();
}

void
NV40EXADoneComposite(PixmapPtr pdPix)
{
	ScrnInfoPtr pScrn = xf86Screens[pdPix->drawable.pScreen->myNum];
	PS3Ptr pPS3 = PS3PTR(pScrn);

	BEGIN_RING(PS3TCLChannel, NV40TCL_BEGIN_END, 1);
	OUT_RING  (NV40TCL_BEGIN_END_STOP);
}
