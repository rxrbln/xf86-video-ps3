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

#ifdef ENABLE_NV30EXA

#include "nv_include.h"
#include "nv_shaders.h"

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

static void
NV40EXAHackupA8Shaders(ScrnInfoPtr pScrn)
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

static void
NV40_LoadVtxProg(ScrnInfoPtr pScrn, nv_shader_t *shader)
{
	NVPtr pNv = NVPTR(pScrn);
	static int next_hw_id = 0;
	int i;

	if (!shader->hw_id) {
		shader->hw_id = next_hw_id;

		BEGIN_RING(Nv3D, NV40TCL_VP_UPLOAD_FROM_ID, 1);
		OUT_RING  ((shader->hw_id));
		for (i=0; i<shader->size; i+=4) {
			BEGIN_RING(Nv3D, NV40TCL_VP_UPLOAD_INST(0), 4);
			OUT_RING  (shader->data[i + 0]);
			OUT_RING  (shader->data[i + 1]);
			OUT_RING  (shader->data[i + 2]);
			OUT_RING  (shader->data[i + 3]);
			next_hw_id++;
		}
	}

	BEGIN_RING(Nv3D, NV40TCL_VP_START_FROM_ID, 1);
	OUT_RING  ((shader->hw_id));

	BEGIN_RING(Nv3D, NV40TCL_VP_ATTRIB_EN, 2);
	OUT_RING  (shader->card_priv.NV30VP.vp_in_reg);
	OUT_RING  (shader->card_priv.NV30VP.vp_out_reg);
}

static void
NV40_LoadFragProg(ScrnInfoPtr pScrn, nv_shader_t *shader)
{
	NVPtr pNv = NVPTR(pScrn);
	static NVAllocRec *fp_mem = NULL;
	static int next_hw_id_offset = 0;

	if (!fp_mem) {
		fp_mem = NVAllocateMemory(pNv, NOUVEAU_MEM_FB, 0x1000);
		if (!fp_mem) {
			xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
				   "Couldn't alloc fragprog buffer!\n");
			return;
		}
	}

	if (!shader->hw_id) {
		uint32_t *map = fp_mem->map + next_hw_id_offset;
		int i;

		for (i = 0; i < shader->size; i++) {
			uint32_t data = shader->data[i];
#if (X_BYTE_ORDER != X_LITTLE_ENDIAN)
			data = ((data >> 16) | ((data & 0xffff) << 16));
#endif
			map[i] = data;
		}

		shader->hw_id  = fp_mem->offset;
		shader->hw_id += next_hw_id_offset;

		next_hw_id_offset += (shader->size * sizeof(uint32_t));
		next_hw_id_offset = (next_hw_id_offset + 63) & ~63;
	}

	BEGIN_RING(Nv3D, NV40TCL_FP_ADDRESS, 1);
	OUT_RING  (shader->hw_id | NV40TCL_FP_ADDRESS_DMA0);
	BEGIN_RING(Nv3D, NV40TCL_FP_CONTROL, 1);
	OUT_RING  (shader->card_priv.NV30FP.num_regs <<
		   NV40TCL_FP_CONTROL_TEMP_COUNT_SHIFT);
}

static void
NV40_SetupBlend(ScrnInfoPtr pScrn, nv_pict_op_t *blend,
		PictFormatShort dest_format, Bool component_alpha)
{
	NVPtr pNv = NVPTR(pScrn);
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
		BEGIN_RING(Nv3D, NV40TCL_BLEND_ENABLE, 1);
		OUT_RING  (0);
	} else {
		BEGIN_RING(Nv3D, NV40TCL_BLEND_ENABLE, 5);
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
	NVPtr pNv = NVPTR(pScrn);
	nv_pict_texture_format_t *fmt;
	NV40EXA_STATE;

	fmt = NV40_GetPictTextureFormat(pPict->format);
	if (!fmt)
		return FALSE;

	BEGIN_RING(Nv3D, NV40TCL_TEX_OFFSET(unit), 8);
	OUT_RING  (NVAccelGetPixmapOffset(pPix));
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
	BEGIN_RING(Nv3D, NV40TCL_TEX_SIZE1(unit), 1);
	OUT_RING  ((1 << NV40TCL_TEX_SIZE1_DEPTH_SHIFT) |
		   (uint32_t)exaGetPixmapPitch(pPix));

	state->unit[unit].width		= (float)pPix->drawable.width;
	state->unit[unit].height	= (float)pPix->drawable.height;
	state->unit[unit].transform	= pPict->transform;

	return TRUE;
}

static Bool
NV40_SetupSurface(ScrnInfoPtr pScrn, PixmapPtr pPix, PictFormatShort format)
{
	NVPtr pNv = NVPTR(pScrn);
	nv_pict_surface_format_t *fmt;

	fmt = NV40_GetPictSurfaceFormat(format);
	if (!fmt) {
		ErrorF("AIII no format\n");
		return FALSE;
	}

        uint32_t pitch = (uint32_t)exaGetPixmapPitch(pPix);

	BEGIN_RING(Nv3D, NV40TCL_RT_FORMAT, 3);
	OUT_RING  (NV40TCL_RT_FORMAT_TYPE_LINEAR |
		   NV40TCL_RT_FORMAT_ZETA_Z24S8 |
		   fmt->card_fmt);
	OUT_RING  (pitch);
	OUT_RING  (NVAccelGetPixmapOffset(pPix));

	return TRUE;
}

static Bool
NV40EXACheckCompositeTexture(PicturePtr pPict)
{
	nv_pict_texture_format_t *fmt;
	int w = pPict->pDrawable->width;
	int h = pPict->pDrawable->height;

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
	NVPtr pNv = NVPTR(pScrn);
	nv_pict_op_t *blend;
	int fpid = NV40EXA_FPID_PASS_COL0;
	NV40EXA_STATE;

	blend = NV40_GetPictOpRec(op);

	NV40_SetupBlend(pScrn, blend, pdPict->format,
			(pmPict && pmPict->componentAlpha &&
			 PICT_FORMAT_RGB(pmPict->format)));

	NV40_SetupSurface(pScrn, pdPix, pdPict->format);
	NV40EXATexture(pScrn, psPix, psPict, 0);

	NV40_LoadVtxProg(pScrn, &nv40_vp_exa_render);
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
		NV40_LoadFragProg(pScrn, nv40_fp_map_a8[fpid]);
	else
		NV40_LoadFragProg(pScrn, nv40_fp_map[fpid]);

	/* Appears to be some kind of cache flush, needed here at least
	 * sometimes.. funky text rendering otherwise :)
	 */
	BEGIN_RING(Nv3D, NV40TCL_TEX_CACHE_CTL, 1);
	OUT_RING  (2);
	BEGIN_RING(Nv3D, NV40TCL_TEX_CACHE_CTL, 1);
	OUT_RING  (1);

	BEGIN_RING(Nv3D, NV40TCL_BEGIN_END, 1);
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
	BEGIN_RING(Nv3D, NV40TCL_VTX_ATTR_2F_X(8), 4);                         \
	OUT_RINGf ((sx)); OUT_RINGf ((sy));                                    \
	OUT_RINGf ((mx)); OUT_RINGf ((my));                                    \
	BEGIN_RING(Nv3D, NV40TCL_VTX_ATTR_2I(0), 1);                           \
	OUT_RING  (((dy)<<16)|(dx));                                           \
} while(0)
#define CV_OUT(sx,sy,dx,dy) do {                                               \
	BEGIN_RING(Nv3D, NV40TCL_VTX_ATTR_2F_X(8), 2);                         \
	OUT_RINGf ((sx)); OUT_RINGf ((sy));                                    \
	BEGIN_RING(Nv3D, NV40TCL_VTX_ATTR_2I(0), 1);                           \
	OUT_RING  (((dy)<<16)|(dx));                                           \
} while(0)

void
NV40EXAComposite(PixmapPtr pdPix, int srcX , int srcY,
				  int maskX, int maskY,
				  int dstX , int dstY,
				  int width, int height)
{
	ScrnInfoPtr pScrn = xf86Screens[pdPix->drawable.pScreen->myNum];
	NVPtr pNv = NVPTR(pScrn);
	float sX0, sX1, sY0, sY1;
	float mX0, mX1, mY0, mY1;
	NV40EXA_STATE;

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
	NVPtr pNv = NVPTR(pScrn);

	BEGIN_RING(Nv3D, NV40TCL_BEGIN_END, 1);
	OUT_RING  (NV40TCL_BEGIN_END_STOP);
}

#define NV40TCL_CHIPSET_4X_MASK 0x00000baf
#define NV44TCL_CHIPSET_4X_MASK 0x00005450
Bool
NVAccelInitNV40TCL(ScrnInfoPtr pScrn)
{
	NVPtr pNv = NVPTR(pScrn);
	static int have_object = FALSE;
	uint32_t class = 0, chipset;
	int i;

	NV40EXAHackupA8Shaders(pScrn);

	chipset = (nvReadMC(pNv, 0) >> 20) & 0xff;
	if ((chipset & 0xf0) != NV_ARCH_40)
		return TRUE;
	chipset &= 0xf;

	if (NV40TCL_CHIPSET_4X_MASK & (1<<chipset))
		class = NV40TCL;
	else if (NV44TCL_CHIPSET_4X_MASK & (1<<chipset))
		class = NV44TCL;
	else {
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
			   "NV40EXA: Unknown chipset NV4%1x\n", chipset);
		return FALSE;
	}

	if (!have_object) {
		if (!NVDmaCreateContextObject(pNv, Nv3D, class))
			return FALSE;
		have_object = TRUE;
	}

	BEGIN_RING(Nv3D, NV40TCL_DMA_NOTIFY, 1);
	OUT_RING  (NvDmaNotifier0);
	BEGIN_RING(Nv3D, NV40TCL_DMA_TEXTURE0, 1);
	OUT_RING  (NvDmaFB);
	BEGIN_RING(Nv3D, NV40TCL_DMA_COLOR0, 2);
	OUT_RING  (NvDmaFB);
	OUT_RING  (NvDmaFB);

	/* voodoo */
	BEGIN_RING(Nv3D, 0x1ea4, 3);
	OUT_RING  (0x00000010);
	OUT_RING  (0x01000100);
	OUT_RING  (0xff800006);
	BEGIN_RING(Nv3D, 0x1fc4, 1);
	OUT_RING  (0x06144321);
	BEGIN_RING(Nv3D, 0x1fc8, 2);
	OUT_RING  (0xedcba987);
	OUT_RING  (0x00000021);
	BEGIN_RING(Nv3D, 0x1fd0, 1);
	OUT_RING  (0x00171615);
	BEGIN_RING(Nv3D, 0x1fd4, 1);
	OUT_RING  (0x001b1a19);
	BEGIN_RING(Nv3D, 0x1ef8, 1);
	OUT_RING  (0x0020ffff);
	BEGIN_RING(Nv3D, 0x1d64, 1);
	OUT_RING  (0x00d30000);
	BEGIN_RING(Nv3D, 0x1e94, 1);
	OUT_RING  (0x00000001);

	BEGIN_RING(Nv3D, NV40TCL_VIEWPORT_TRANSLATE_X, 8);
	OUT_RINGf (0.0);
	OUT_RINGf (0.0);
	OUT_RINGf (0.0);
	OUT_RINGf (0.0);
	OUT_RINGf (1.0);
	OUT_RINGf (1.0);
	OUT_RINGf (1.0);
	OUT_RINGf (0.0);

	/* default 3D state */
	/*XXX: replace with the same state that the DRI emits on startup */
	BEGIN_RING(Nv3D, NV40TCL_STENCIL_FRONT_ENABLE, 1);
	OUT_RING  (0);
	BEGIN_RING(Nv3D, NV40TCL_STENCIL_BACK_ENABLE, 1);
	OUT_RING  (0);
	BEGIN_RING(Nv3D, NV40TCL_ALPHA_TEST_ENABLE, 1);
	OUT_RING  (0);
	BEGIN_RING(Nv3D, NV40TCL_DEPTH_WRITE_ENABLE, 2);
	OUT_RING  (0);
	OUT_RING  (0); 
	BEGIN_RING(Nv3D, NV40TCL_COLOR_MASK, 1);
	OUT_RING  (0x01010101); /* TR,TR,TR,TR */
	BEGIN_RING(Nv3D, NV40TCL_CULL_FACE_ENABLE, 1);
	OUT_RING  (0);
	BEGIN_RING(Nv3D, NV40TCL_BLEND_ENABLE, 1);
	OUT_RING  (0);
	BEGIN_RING(Nv3D, NV40TCL_COLOR_LOGIC_OP_ENABLE, 2);
	OUT_RING  (0);
	OUT_RING  (NV40TCL_COLOR_LOGIC_OP_COPY);
	BEGIN_RING(Nv3D, NV40TCL_DITHER_ENABLE, 1);
	OUT_RING  (0);
	BEGIN_RING(Nv3D, NV40TCL_SHADE_MODEL, 1);
	OUT_RING  (NV40TCL_SHADE_MODEL_SMOOTH);
	BEGIN_RING(Nv3D, NV40TCL_POLYGON_OFFSET_FACTOR,2);
	OUT_RINGf (0.0);
	OUT_RINGf (0.0);
	BEGIN_RING(Nv3D, NV40TCL_POLYGON_MODE_FRONT, 2);
	OUT_RING  (NV40TCL_POLYGON_MODE_FRONT_FILL);
	OUT_RING  (NV40TCL_POLYGON_MODE_BACK_FILL);
	BEGIN_RING(Nv3D, NV40TCL_POLYGON_STIPPLE_PATTERN(0), 0x20);
	for (i=0;i<0x20;i++)
		OUT_RING  (0xFFFFFFFF);
	for (i=0;i<16;i++) {
		BEGIN_RING(Nv3D, NV40TCL_TEX_ENABLE(i), 1);
		OUT_RING  (0);
	}

	BEGIN_RING(Nv3D, 0x1d78, 1);
	OUT_RING  (0x110);

	BEGIN_RING(Nv3D, NV40TCL_RT_ENABLE, 1);
	OUT_RING  (NV40TCL_RT_ENABLE_COLOR0);

	BEGIN_RING(Nv3D, NV40TCL_RT_HORIZ, 2);
	OUT_RING  ((4096 << 16));
	OUT_RING  ((4096 << 16));
	BEGIN_RING(Nv3D, NV40TCL_SCISSOR_HORIZ, 2);
	OUT_RING  ((4096 << 16));
	OUT_RING  ((4096 << 16));
	BEGIN_RING(Nv3D, NV40TCL_VIEWPORT_HORIZ, 2);
	OUT_RING  ((4096 << 16));
	OUT_RING  ((4096 << 16));
	BEGIN_RING(Nv3D, NV40TCL_VIEWPORT_CLIP_HORIZ(0), 2);
	OUT_RING  ((4095 << 16));
	OUT_RING  ((4095 << 16));

	return TRUE;
}

#endif /* ENABLE_NV30EXA */
