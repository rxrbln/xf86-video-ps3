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

#include <stdint.h>
#include "nouveau_class.h"
#include "nv_shaders.h"

#include <sys/time.h>
#include <string.h>
#include <unistd.h>

//#define TRACE() ErrorF("%s\n", __FUNCTION__);
#define TRACE()

//#define FALLBACK(msg) ErrorF("%s: FALLBACK: " msg, __FUNCTION__);
#define FALLBACK(msg) 

#define RAMIN_BASE    0x0ff80000 /* RAMIN address in upper video RAM */
#define RAMHT_OFFSET  0x10000    /* offset of hash table relative to RAMIN */

static void ramin_read_line256(PS3Ptr pPS3, unsigned int addr)
{
	PS3DmaStart(pPS3, PS3ScaledImageChannel,  0x184, 1);
	PS3DmaNext(pPS3, PS3DmaFB);
	PS3DmaStart(pPS3, PS3ScaledImageChannel, 0x198, 1);
	PS3DmaNext(pPS3, PS3ContextSurfaces);
	PS3DmaStart(pPS3, PS3ContextSurfacesChannel, 0x300, 1);
	PS3DmaNext(pPS3, 0x0000000a );
	PS3DmaStart(pPS3, PS3ContextSurfacesChannel, 0x30c, 1);
	PS3DmaNext(pPS3, 0 );
	PS3DmaStart(pPS3, PS3ContextSurfacesChannel, 0x304, 1);
	PS3DmaNext(pPS3, 0x01000100);
	PS3DmaStart(pPS3, PS3ScaledImageChannel, 0x2fc, 9);
	PS3DmaNext(pPS3, 0x00000001);
	PS3DmaNext(pPS3, 0x00000003);
	PS3DmaNext(pPS3, 0x00000003);
	PS3DmaNext(pPS3, 0x00000000);
	PS3DmaNext(pPS3, 0x00010040); /* 64x1 */
	PS3DmaNext(pPS3, 0x00000000);
	PS3DmaNext(pPS3, 0x00010040); /* 64x1 */
	PS3DmaNext(pPS3, 0x00100000);
	PS3DmaNext(pPS3, 0x00100000);
	PS3DmaStart(pPS3, PS3ScaledImageChannel, 0x400, 4 );
	PS3DmaNext(pPS3, 0x00010040); /* 64x1 */
	PS3DmaNext(pPS3, 0x00020100); /* pitch = 256, corner */
	PS3DmaNext(pPS3, RAMIN_BASE + addr);
	PS3DmaNext(pPS3, 0x00000000);
	PS3DmaKickoff(pPS3);
	PS3Sync(pPS3);
	// wait for end of blit (cannot use notifier yet)
	usleep(1000);
}

static void ramin_write_line256(PS3Ptr pPS3, unsigned int addr)
{
	PS3DmaStart(pPS3, PS3ScaledImageChannel,  0x184, 1);
	PS3DmaNext(pPS3, PS3DmaFB);
	PS3DmaStart(pPS3, PS3ScaledImageChannel, 0x198, 1);
	PS3DmaNext(pPS3, PS3ContextSurfaces);
	PS3DmaStart(pPS3, PS3ContextSurfacesChannel, 0x300, 1);
	PS3DmaNext(pPS3, 0x0000000a);
	PS3DmaStart(pPS3, PS3ContextSurfacesChannel, 0x30c, 1);
	PS3DmaNext(pPS3, RAMIN_BASE + addr);
	PS3DmaStart(pPS3, PS3ContextSurfacesChannel, 0x304, 1);
	PS3DmaNext(pPS3, 0x01000100);
	PS3DmaStart(pPS3, PS3ScaledImageChannel, 0x2fc, 9);
	PS3DmaNext(pPS3, 0x00000001);
	PS3DmaNext(pPS3, 0x00000003);
	PS3DmaNext(pPS3, 0x00000003);
	PS3DmaNext(pPS3, 0x00000000);
	PS3DmaNext(pPS3, 0x00010040); /* 64x1 */
	PS3DmaNext(pPS3, 0x00000000);
	PS3DmaNext(pPS3, 0x00010040); /* 64x1 */
	PS3DmaNext(pPS3, 0x00100000);
	PS3DmaNext(pPS3, 0x00100000);
	PS3DmaStart(pPS3, PS3ScaledImageChannel, 0x400, 4);
	PS3DmaNext(pPS3, 0x00010040 ); /* 64x1 */
	PS3DmaNext(pPS3, 0x00020100 ); /* pitch = 256, corner */
	PS3DmaNext(pPS3, 0x00000000 );
	PS3DmaNext(pPS3, 0x00000000 );
	PS3DmaKickoff(pPS3);
	PS3Sync(pPS3);
	// wait for end of blit (cannot use notifier yet)
	usleep(1000);
}

static void ramin_write_dword_to_offset(PS3Ptr pPS3, CARD32 addr, CARD32 data)
{
	CARD32 off = addr & 0xff;
	CARD32 *vram = (CARD32 *) pPS3->vram_base;

	// copy aligned line to the begin of framebuffer
	ramin_read_line256(pPS3, addr - off);

	// patch with data, GPU is little-endian
	vram[off / 4] = endian(data);

	// copy data back
	ramin_write_line256(pPS3, addr - off);
}

#define RAMHT_BITS 11

static CARD32 hash_handle(int channel, CARD32 handle)
{
	CARD32 hash = 0;
	int i;

//	ErrorF("ch%d handle=0x%08x\n", channel, handle);
	
	for (i = 32; i > 0; i -= RAMHT_BITS) {
		hash ^= (handle & ((1 << RAMHT_BITS) - 1));
		handle >>= RAMHT_BITS;
	}
	
	hash ^= channel << (RAMHT_BITS - 4);
	hash <<= 3;

//	ErrorF("hash=0x%08x\n", hash);
	
	return hash;
}

static void ramin_write_ramht_entry(PS3Ptr pPS3, 
				    CARD32 handle, CARD32 offset,
				    CARD32 engine, CARD32 channel)
{
	CARD32 hash = hash_handle(channel, handle);
	CARD32 off = hash & 0xff;
	CARD32 *vram = (CARD32 *) pPS3->vram_base;

	// copy aligned line to the begin of framebuffer
	ramin_read_line256(pPS3, RAMHT_OFFSET + hash - off);

	// patch with data, GPU is little-endian
	vram[off / 4 + 0] = endian(handle);
	vram[off / 4 + 1] = endian((channel << 23) |
				   (engine << 20)  |
				   (offset >> 4));

	// copy data back
	ramin_write_line256(pPS3, RAMHT_OFFSET + hash - off);
}

static void ramin_write_gfx_entry(PS3Ptr pPS3, CARD32 offset, CARD32 object[8])
{
	CARD32 off = offset & 0xff;
	CARD32 *vram = (CARD32 *) pPS3->vram_base;
	int i;

	// copy aligned line to the begin of framebuffer
	ramin_read_line256(pPS3, offset - off);

	// patch with data, GPU is little-endian
	for (i = 0; i < 8; i++)
		vram[off / 4 + i] = endian(object[i]);

	// copy data back
	ramin_write_line256(pPS3, offset - off);
}

#define DMA_CLASS_2D		0x02
#define DMA_CLASS_3D		0x3d
#define DMA_CLASS_NOTIFY	0x03
#define DMA_PRESENT		(1UL << 12)
#define DMA_LINEAR		(1UL << 13)
#define DMA_ACCESS_RW		(0UL << 14)
#define DMA_ACCESS_RO		(1UL << 14)
#define DMA_ACCESS_WO		(2UL << 14)
#define DMA_TARGET_NV		(0UL << 16)
#define DMA_TARGET_NV_TILED	(1UL << 16)
#define DMA_TARGET_PCI		(2UL << 16)
#define DMA_TARGET_AGP		(3UL << 16)

static void ramin_write_dma_entry(PS3Ptr pPS3, CARD32 offset,
				  CARD32 class,
				  CARD32 address,
				  CARD32 limit)
{
	CARD32 off = offset & 0xff;
	CARD32 *vram = (CARD32 *) pPS3->vram_base;
	int i;

	// copy aligned line to the begin of framebuffer
	ramin_read_line256(pPS3, offset - off);

	// patch with data, GPU is little-endian
	vram[off / 4 + 0] = endian(((address & 0xfff) << 20) | class);
	vram[off / 4 + 1] = endian(limit);
	vram[off / 4 + 2] = endian((address & ~0xfff) | 3);
	vram[off / 4 + 3] = endian((address & ~0xfff) | 3);

	// copy data back
	ramin_write_line256(pPS3, offset - off);
}

static void NV40_LoadTex(PS3Ptr pPS3)
{
	CARD32 i;
	CARD32 unit = 0;
	CARD32 offset = 20 * 1024 * 1024;
	CARD32 width = 128, height = 128;
	unsigned char *fbmem = (unsigned char *) pPS3->vram_base;

	for (i = 0; i < width * height * 4; i += 4 )
	{
		fbmem[i + offset + 0] = (rand() & 127); // A
		fbmem[i + offset + 1] = 0; // R
		fbmem[i + offset + 2] = 255;   // G
		fbmem[i + offset + 3] = 0; // B
	}

	CARD32 swz =  
	NV40TCL_TEX_SWIZZLE_S0_X_S1 | NV40TCL_TEX_SWIZZLE_S0_Y_S1 |
  	NV40TCL_TEX_SWIZZLE_S0_Z_S1 | NV40TCL_TEX_SWIZZLE_S0_W_S1 |
  	NV40TCL_TEX_SWIZZLE_S1_X_X | NV40TCL_TEX_SWIZZLE_S1_Y_Y |
  	NV40TCL_TEX_SWIZZLE_S1_Z_Z | NV40TCL_TEX_SWIZZLE_S1_W_W;

  	PS3DmaStart(pPS3, PS3TCLChannel, NV40TCL_TEX_OFFSET(unit), 8);
	PS3DmaNext(pPS3, offset);

	PS3DmaNext(pPS3, 
		   NV40TCL_TEX_FORMAT_FORMAT_A8R8G8B8 | 
		   NV40TCL_TEX_FORMAT_LINEAR | 
		   NV40TCL_TEX_FORMAT_DIMS_2D | 
		   NV40TCL_TEX_FORMAT_DMA0 |
		   NV40TCL_TEX_FORMAT_NO_BORDER | (0x8000) |
		   (1 << NV40TCL_TEX_FORMAT_MIPMAP_COUNT_SHIFT));

	PS3DmaNext(pPS3, 
		   NV40TCL_TEX_WRAP_S_REPEAT |
		   NV40TCL_TEX_WRAP_T_REPEAT |
		   NV40TCL_TEX_WRAP_R_REPEAT);

	PS3DmaNext(pPS3, NV40TCL_TEX_ENABLE_ENABLE);
	PS3DmaNext(pPS3, swz);
         
	PS3DmaNext(pPS3, 
		   NV40TCL_TEX_FILTER_MIN_LINEAR |
		   NV40TCL_TEX_FILTER_MAG_LINEAR | 0x3fd6);
	PS3DmaNext(pPS3, (width << 16) | height);
	PS3DmaNext(pPS3, 0); /* border ARGB */
 	PS3DmaStart(pPS3, PS3TCLChannel, NV40TCL_TEX_SIZE1(unit), 1);

	PS3DmaNext(pPS3, 
		   (1 << NV40TCL_TEX_SIZE1_DEPTH_SHIFT) |
		   width * 4);
}

void NV40_LoadVtxProg(PS3Ptr pPS3, nv_shader_t *shader)
{
	CARD32 i;

	PS3DmaStart(pPS3, PS3TCLChannel, NV40TCL_VP_UPLOAD_FROM_ID, 1);
	PS3DmaNext(pPS3, 0);

	for (i = 0; i < shader->size; i += 4) 
	{
		PS3DmaStart(pPS3, PS3TCLChannel, NV40TCL_VP_UPLOAD_INST(0), 4);
		PS3DmaNext(pPS3, shader->data[i + 0]);
		PS3DmaNext(pPS3, shader->data[i + 1]);
		PS3DmaNext(pPS3, shader->data[i + 2]);
		PS3DmaNext(pPS3, shader->data[i + 3]);
	}

	PS3DmaStart(pPS3, PS3TCLChannel, NV40TCL_VP_START_FROM_ID, 1);
	PS3DmaNext(pPS3, 0);

	PS3DmaStart(pPS3, PS3TCLChannel, NV40TCL_VP_ATTRIB_EN, 2);
	PS3DmaNext(pPS3, shader->card_priv.NV30VP.vp_in_reg);
	PS3DmaNext(pPS3, shader->card_priv.NV30VP.vp_out_reg);

	PS3DmaStart(pPS3, PS3TCLChannel, 0x1478, 1);
	PS3DmaNext(pPS3, 0);
}

int NV40_LoadFragProg(PS3Ptr pPS3, nv_shader_t *shader)
{
	CARD32 i;
	CARD32 offset = 10 * 1024 * 1024;
	CARD32 *fb = (CARD32 *) pPS3->vram_base;
	CARD32 *fpmem = (CARD32 *) pPS3->fpMem;
	static int next_hw_id_offset = 0;

	if (!shader->hw_id)
	{
		for (i = 0; i < shader->size; i++) {
		   fpmem[next_hw_id_offset + i] = endian_fp(shader->data[i]);
//		   ErrorF("FP: %08x\n", fpmem[next_hw_id_offset + i]);
		}
		
		shader->hw_id  = fpmem - fb;
		shader->hw_id += next_hw_id_offset;
		shader->hw_id *= 4;

		next_hw_id_offset += shader->size;
		next_hw_id_offset = (next_hw_id_offset + 63) & ~63;
	}

//	ErrorF("frag prog 0x%x \n", shader->hw_id );
	PS3DmaStart(pPS3, PS3TCLChannel, NV40TCL_FP_ADDRESS, 1);
	PS3DmaNext(pPS3,  shader->hw_id | NV40TCL_FP_ADDRESS_DMA0);
	PS3DmaStart(pPS3, PS3TCLChannel, NV40TCL_FP_CONTROL, 1);
	PS3DmaNext(pPS3,
		   (shader->card_priv.NV30FP.num_regs << NV40TCL_FP_CONTROL_TEMP_COUNT_SHIFT));
}

#define CV_OUT( sx,sy, sz, tx, ty) do {                                \
	PS3DmaStart(pPS3, PS3TCLChannel, NV40TCL_VTX_ATTR_4F_X(0), 4); \
	PS3DmaFloat(pPS3, sx); PS3DmaFloat(pPS3, sy);                  \
	PS3DmaFloat(pPS3, sz); PS3DmaFloat(pPS3, 1.0f);                \
	PS3DmaStart(pPS3, PS3TCLChannel, NV40TCL_VTX_ATTR_2F_X(8), 2); \
	PS3DmaFloat(pPS3, tx); PS3DmaFloat(pPS3, ty);                  \
} while(0)

static int NV40_EmitGeometry(PS3Ptr pPS3)
{
	CARD32 i;
	PS3DmaStart(pPS3, PS3TCLChannel, NV40TCL_BEGIN_END, 1);
	PS3DmaNext(pPS3, NV40TCL_BEGIN_END_TRIANGLES);
	float sin_tab[] = { 0, 0.866025403784439, -0.866025403784438 };
	float cos_tab[] = { 1, 0.5, -0.5 };
	static float t = 0;

	t += 0.25;
	
	for( i = 0; i < 3; ++i )
	{
		float si = sin_tab[i];
		float co = cos_tab[i];
	    
		float x1 = 200.0f + t, y1 = 80.0f;
		float x2 = -200.0f, y2 = 10.0f;
		float x3 = -200.0f, y3 = 150.0f;	    
	    
		CV_OUT( 256.0f + x1 * co + y1 * si,
			256.0f - x1 * si + y1 * co,
			1.0f, 0.5f, 0.0f  );
		CV_OUT( 256.0f + x2 * co + y2 * si,
			256.0f - x2 * si + y2 * co,
			0.0f, 0.0f, 1.0f  );
		CV_OUT( 256.0f + x3 * co + y3 * si,
			256.0f - x3 * si + y3 * co,
			0.0f, 1.0f, 1.0f  );
	}

	PS3DmaStart(pPS3, PS3TCLChannel, NV40TCL_BEGIN_END, 1);
	PS3DmaNext(pPS3, NV40TCL_BEGIN_END_STOP);
}

static CARD32 PS3TCLObject[] = {
	0x00004097, // 0x40 for NV40, 0x97 - 3D engine
	0x00000000,
	0x01000000, // big endian
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
};

static CARD32 PS3ImageBlitObject[] = {
	0x0000009f, // 0x9f - Image Blit
	0x00000000,
	0x01000000, // big endian
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
};

static CARD32 PS3RopObject[] = {
	0x00000043, // 0x43 - Context Rop
	0x00000000,
	0x01000000, // big endian
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
};

static CARD32 PS3RectangleObject[] = {
	NV04_GDI_RECTANGLE_TEXT,
	0x00000000,
	0x01000000, // big endian
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
};

static void create_TCL_instance(PS3Ptr pPS3)
{
	static unsigned long offset = 0x50200;

	ramin_write_ramht_entry(pPS3, PS3TCL, offset, 1, 0);
	ramin_write_gfx_entry(pPS3, offset, PS3TCLObject);
}

static void create_ImageBlit_instance(PS3Ptr pPS3)
{
	static unsigned long offset = 0x50220;

	ramin_write_ramht_entry(pPS3, PS3ImageBlit, offset, 1, 0);
	ramin_write_gfx_entry(pPS3, offset, PS3ImageBlitObject);
}

static void create_Rectangle_instance(PS3Ptr pPS3)
{
	static unsigned long offset = 0x50240;

	ramin_write_ramht_entry(pPS3, PS3Rectangle, offset, 1, 0);
	ramin_write_gfx_entry(pPS3, offset, PS3RectangleObject);
}

static void create_Rop_instance(PS3Ptr pPS3)
{
	static unsigned long offset = 0x50260;

	ramin_write_ramht_entry(pPS3, PS3Rop, offset, 1, 0);
	ramin_write_gfx_entry(pPS3, offset, PS3RopObject);
}

static void create_DmaNotifier_instance(PS3Ptr pPS3)
{
	static unsigned long offset = 0x40170;

	ramin_write_ramht_entry(pPS3, PS3DmaNotifier, offset, 1, 0);
	ramin_write_dma_entry(pPS3, offset,
			      DMA_CLASS_NOTIFY |
			      DMA_PRESENT | DMA_LINEAR |
			      DMA_ACCESS_RW | DMA_TARGET_NV,
			      (unsigned long) pPS3->dmaNotifier -
			      (unsigned long) pPS3->vram_base, 63);
}

static void bind_TCL_instance(PS3Ptr pPS3)
{
	PS3DmaStart(pPS3, PS3TCLChannel, 0, 1);
	PS3DmaNext(pPS3, PS3TCL);
}

static void bind_ImageBlit_instance(PS3Ptr pPS3)
{
	PS3DmaStart(pPS3, PS3ImageBlitChannel, 0, 1);
	PS3DmaNext(pPS3, PS3ImageBlit);
}

static void bind_Rectangle_instance(PS3Ptr pPS3)
{
	PS3DmaStart(pPS3, PS3RectangleChannel, 0, 1);
	PS3DmaNext(pPS3, PS3Rectangle);
}

static void init_ImageBlit_instance(PS3Ptr pPS3)
{
        BEGIN_RING(PS3ImageBlitChannel, NV_IMAGE_BLIT_DMA_NOTIFY, 1);
        OUT_RING  (PS3DmaNotifier);
        BEGIN_RING(PS3ImageBlitChannel, NV_IMAGE_BLIT_COLOR_KEY, 1);
        OUT_RING  (PS3NullObject);
        BEGIN_RING(PS3ImageBlitChannel, NV_IMAGE_BLIT_SURFACE, 1);
        OUT_RING  (PS3ContextSurfaces);
        BEGIN_RING(PS3ImageBlitChannel, NV_IMAGE_BLIT_CLIP_RECTANGLE, 1);
        OUT_RING  (PS3NullObject);
        BEGIN_RING(PS3ImageBlitChannel, NV_IMAGE_BLIT_OPERATION, 1);
        OUT_RING  (NV_IMAGE_BLIT_OPERATION_SRCCOPY);

	BEGIN_RING(PS3ImageBlitChannel, 0x0120, 3);
	OUT_RING  (0);
	OUT_RING  (1);
	OUT_RING  (2);
}

static void init_Rectangle_instance(PS3Ptr pPS3)
{
	BEGIN_RING(PS3RectangleChannel, NV04_GDI_RECTANGLE_TEXT_DMA_NOTIFY, 1);
        OUT_RING  (PS3DmaNotifier);
        BEGIN_RING(PS3RectangleChannel, NV04_GDI_RECTANGLE_TEXT_DMA_FONTS, 1);
        OUT_RING  (PS3NullObject);
        BEGIN_RING(PS3RectangleChannel, NV04_GDI_RECTANGLE_TEXT_SURFACE, 1);
        OUT_RING  (PS3ContextSurfaces);
        BEGIN_RING(PS3RectangleChannel, NV04_GDI_RECTANGLE_TEXT_ROP, 1);
        OUT_RING  (PS3Rop);
        BEGIN_RING(PS3RectangleChannel, NV04_GDI_RECTANGLE_TEXT_PATTERN, 1);
        OUT_RING  (PS3NullObject);
        BEGIN_RING(PS3RectangleChannel, NV04_GDI_RECTANGLE_TEXT_OPERATION, 1);
        OUT_RING  (NV04_GDI_RECTANGLE_TEXT_OPERATION_ROP_AND);
        BEGIN_RING(PS3RectangleChannel, NV04_GDI_RECTANGLE_TEXT_MONOCHROME_FORMAT, 1);
        OUT_RING  (NV04_GDI_RECTANGLE_TEXT_MONOCHROME_FORMAT_LE);
}

#define SF(bf) (NV40TCL_BLEND_FUNC_SRC_RGB_##bf |                              \
                NV40TCL_BLEND_FUNC_SRC_ALPHA_##bf)
#define DF(bf) (NV40TCL_BLEND_FUNC_DST_RGB_##bf |                              \
                NV40TCL_BLEND_FUNC_DST_ALPHA_##bf)

static void init_TCL_instance(ScrnInfoPtr pScrn)
{
	PS3Ptr pPS3 = PS3PTR(pScrn);
	CARD32 *fbmem = (CARD32 *) pPS3->vram_base;
	int i;

	PS3DmaStart(pPS3, PS3TCLChannel, NV40TCL_DMA_NOTIFY, 1);
	PS3DmaNext(pPS3, PS3DmaNotifier);
	PS3DmaStart(pPS3, PS3TCLChannel, NV40TCL_DMA_TEXTURE0, 1);
	PS3DmaNext(pPS3, PS3DmaFB);
	PS3DmaStart(pPS3, PS3TCLChannel, NV40TCL_DMA_COLOR0, 2);
	PS3DmaNext(pPS3, PS3DmaFB);
	PS3DmaNext(pPS3, PS3DmaFB);
	PS3DmaStart(pPS3, PS3TCLChannel, NV40TCL_DMA_ZETA, 1 );
	PS3DmaNext(pPS3, PS3DmaFB);

	/* voodoo */
	PS3DmaStart(pPS3, PS3TCLChannel, 0x1ea4, 3);
	PS3DmaNext(pPS3, 0x00000010);
	PS3DmaNext(pPS3, 0x01000100);
	PS3DmaNext(pPS3, 0xff800006);
	PS3DmaStart(pPS3, PS3TCLChannel, 0x1fc4, 1);
	PS3DmaNext(pPS3, 0x06144321);
	PS3DmaStart(pPS3, PS3TCLChannel, 0x1fc8, 2);
	PS3DmaNext(pPS3, 0xedcba987);
	PS3DmaNext(pPS3, 0x00000021);
	PS3DmaStart(pPS3, PS3TCLChannel, 0x1fd0, 1);
	PS3DmaNext(pPS3, 0x00171615);
	PS3DmaStart(pPS3, PS3TCLChannel, 0x1fd4, 1);
	PS3DmaNext(pPS3, 0x001b1a19);
	PS3DmaStart(pPS3, PS3TCLChannel, 0x1ef8, 1);
	PS3DmaNext(pPS3, 0x0020ffff);
	PS3DmaStart(pPS3, PS3TCLChannel, 0x1d64, 1);
	PS3DmaNext(pPS3, 0x00d30000);
	PS3DmaStart(pPS3, PS3TCLChannel, 0x1e94, 1);
	PS3DmaNext(pPS3, 0x00000001);

	PS3DmaStart(pPS3, PS3TCLChannel, NV40TCL_VIEWPORT_TRANSLATE_X, 8);
	PS3DmaFloat(pPS3, 0.0);
	PS3DmaFloat(pPS3, 0.0);
	PS3DmaFloat(pPS3, 0.0);
	PS3DmaFloat(pPS3, 0.0);
	PS3DmaFloat(pPS3, 1.0);
	PS3DmaFloat(pPS3, 1.0);
	PS3DmaFloat(pPS3, 1.0);
	PS3DmaFloat(pPS3, 0.0);

	/* default 3D state */
	PS3DmaStart(pPS3, PS3TCLChannel, NV40TCL_STENCIL_FRONT_ENABLE, 1);
	PS3DmaNext(pPS3, 0);
	PS3DmaStart(pPS3, PS3TCLChannel, NV40TCL_STENCIL_BACK_ENABLE, 1);
	PS3DmaNext(pPS3, 0);
	PS3DmaStart(pPS3, PS3TCLChannel, NV40TCL_ALPHA_TEST_ENABLE, 1);
	PS3DmaNext(pPS3, 0);
	PS3DmaStart(pPS3, PS3TCLChannel, NV40TCL_DEPTH_WRITE_ENABLE, 2);
	PS3DmaNext(pPS3, 0);
	PS3DmaNext(pPS3, 0); 
	PS3DmaStart(pPS3, PS3TCLChannel, NV40TCL_COLOR_MASK, 1);
	PS3DmaNext(pPS3, 0x01010101); /* TR,TR,TR,TR */
	PS3DmaStart(pPS3, PS3TCLChannel, NV40TCL_CULL_FACE_ENABLE, 1);
	PS3DmaNext(pPS3, 0);
	PS3DmaStart(pPS3, PS3TCLChannel, NV40TCL_BLEND_ENABLE, 5);
	PS3DmaNext(pPS3, 1);
	PS3DmaNext(pPS3, SF(SRC_ALPHA));
	PS3DmaNext(pPS3, DF(ONE_MINUS_SRC_ALPHA));
	PS3DmaNext(pPS3, 0);
	PS3DmaNext(pPS3, NV40TCL_BLEND_EQUATION_ALPHA_FUNC_ADD |
                         NV40TCL_BLEND_EQUATION_RGB_FUNC_ADD);
	PS3DmaStart(pPS3, PS3TCLChannel, NV40TCL_COLOR_LOGIC_OP_ENABLE, 2);
	PS3DmaNext(pPS3, 0);
	PS3DmaNext(pPS3, NV40TCL_COLOR_LOGIC_OP_COPY);
	PS3DmaStart(pPS3, PS3TCLChannel, NV40TCL_DITHER_ENABLE, 1);
	PS3DmaNext(pPS3, 0);
	PS3DmaStart(pPS3, PS3TCLChannel, NV40TCL_SHADE_MODEL, 1);
	PS3DmaNext(pPS3, NV40TCL_SHADE_MODEL_SMOOTH);
	PS3DmaStart(pPS3, PS3TCLChannel, NV40TCL_POLYGON_OFFSET_FACTOR,2);
	PS3DmaFloat(pPS3, 0.0);
	PS3DmaFloat(pPS3, 0.0);
	PS3DmaStart(pPS3, PS3TCLChannel, NV40TCL_POLYGON_MODE_FRONT, 2);
	PS3DmaNext(pPS3, NV40TCL_POLYGON_MODE_FRONT_FILL);
	PS3DmaNext(pPS3, NV40TCL_POLYGON_MODE_BACK_FILL);
	PS3DmaStart(pPS3, PS3TCLChannel, NV40TCL_POLYGON_STIPPLE_PATTERN(0), 0x20);
	for (i=0;i<0x20;i++)
		PS3DmaNext(pPS3, 0xFFFFFFFF);
	for (i=0;i<16;i++) {
		PS3DmaStart(pPS3, PS3TCLChannel, NV40TCL_TEX_ENABLE(i), 1);
		PS3DmaNext(pPS3, 0);
	}

	PS3DmaStart(pPS3, PS3TCLChannel, 0x1d78, 1);
	PS3DmaNext(pPS3, 0x110);

	PS3DmaStart(pPS3, PS3TCLChannel, NV40TCL_RT_ENABLE, 1);
	PS3DmaNext(pPS3, NV40TCL_RT_ENABLE_COLOR0);

	PS3DmaStart(pPS3, PS3TCLChannel, NV40TCL_RT_HORIZ, 2);
	PS3DmaNext(pPS3, (pScrn->virtualX << 16));
	PS3DmaNext(pPS3, (pScrn->virtualY << 16));
	PS3DmaStart(pPS3, PS3TCLChannel, NV40TCL_SCISSOR_HORIZ, 2);
	PS3DmaNext(pPS3, (pScrn->virtualX << 16));
	PS3DmaNext(pPS3, (pScrn->virtualY << 16));
	PS3DmaStart(pPS3, PS3TCLChannel, NV40TCL_VIEWPORT_HORIZ, 2);
	PS3DmaNext(pPS3, (pScrn->virtualX << 16));
	PS3DmaNext(pPS3, (pScrn->virtualY << 16));
	PS3DmaStart(pPS3, PS3TCLChannel, NV40TCL_VIEWPORT_CLIP_HORIZ(0), 2);
	PS3DmaNext(pPS3, (pScrn->virtualX << 16));
	PS3DmaNext(pPS3, (pScrn->virtualY << 16));

	PS3DmaStart(pPS3, PS3TCLChannel, NV40TCL_ZETA_OFFSET, 1);
	PS3DmaNext(pPS3, 0);
	PS3DmaStart(pPS3, PS3TCLChannel, NV40TCL_ZETA_PITCH, 1);
	PS3DmaNext(pPS3, pPS3->lineLength);

	PS3DmaStart(pPS3, PS3TCLChannel, NV40TCL_RT_FORMAT, 3);
	PS3DmaNext(pPS3, NV40TCL_RT_FORMAT_TYPE_LINEAR |
		   NV40TCL_RT_FORMAT_ZETA_Z16 |
		   NV40TCL_RT_FORMAT_COLOR_A8R8G8B8);
	PS3DmaNext(pPS3, pPS3->lineLength);
	PS3DmaNext(pPS3, 0);
}

static void setup_TCL(ScrnInfoPtr pScrn)
{
	PS3Ptr pPS3 = PS3PTR(pScrn);

	create_DmaNotifier_instance(pPS3);
	create_Rop_instance(pPS3);

	BEGIN_RING(PS3MemFormatDownloadChannel,
		   NV_MEMORY_TO_MEMORY_FORMAT_DMA_NOTIFY, 1);
	OUT_RING  (PS3DmaNotifier);

	BEGIN_RING(PS3MemFormatUploadChannel,
		   NV_MEMORY_TO_MEMORY_FORMAT_DMA_NOTIFY, 1);
	OUT_RING  (PS3DmaNotifier);

	create_ImageBlit_instance(pPS3);
	bind_ImageBlit_instance(pPS3);
	init_ImageBlit_instance(pPS3);

	create_Rectangle_instance(pPS3);
	bind_Rectangle_instance(pPS3);
	init_Rectangle_instance(pPS3);

	create_TCL_instance(pPS3);
	bind_TCL_instance(pPS3);

	init_TCL_instance(pScrn);

	NV40_LoadTex(pPS3);
	NV40_LoadVtxProg(pPS3, &nv40_vp);
	NV40_LoadFragProg(pPS3, &nv30_fp);
	PS3DmaKickoff(pPS3);
	PS3Sync(pPS3);
}

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

static CARD32 rectFormat(DrawablePtr pDrawable)
{
        switch(pDrawable->bitsPerPixel) {
        case 32:
        case 24:
                return NV04_GDI_RECTANGLE_TEXT_COLOR_FORMAT_A8R8G8B8;
                break;
        case 16:
                return NV04_GDI_RECTANGLE_TEXT_COLOR_FORMAT_A16R5G6B5;
                break;
        default:
                return NV04_GDI_RECTANGLE_TEXT_COLOR_FORMAT_A8R8G8B8;
                break;
        }
}

static void PS3ExaWaitMarker(ScreenPtr pScreen, int marker)
{
	ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
	PS3Ptr pPS3 = PS3PTR(pScrn);

	PS3Sync(pPS3);
}

static inline Bool PS3AccelMemcpyRect(char *dst, const char *src, int height,
                       int dst_pitch, int src_pitch, int line_len)
{
        if ((src_pitch == line_len) && (src_pitch == dst_pitch)) {
                memcpy(dst, src, line_len*height);
        } else {
                while (height--) {
                        memcpy(dst, src, line_len);
                        src += src_pitch;
                        dst += dst_pitch;
                }
        }

        return TRUE;
}

static void *PS3ExaPixmapMap(PixmapPtr pPix)
{
        ScrnInfoPtr pScrn = xf86Screens[pPix->drawable.pScreen->myNum];
        PS3Ptr pPS3 = PS3PTR(pScrn);
        void *map;

        map = (void *) pPS3->vram_base + exaGetPixmapOffset(pPix);
        return map;
}

static Bool PS3ExaPrepareSolid(PixmapPtr pPixmap,
			      int   alu,
			      Pixel planemask,
			      Pixel fg)
{
        ScrnInfoPtr pScrn = xf86Screens[pPixmap->drawable.pScreen->myNum];
        PS3Ptr pPS3 = PS3PTR(pScrn);
        int fmt;

	TRACE();

        planemask |= ~0 << pPixmap->drawable.bitsPerPixel;
        if (planemask != ~0 || alu != GXcopy) {
		FALLBACK("alu not supported yet (%02x)\n");
		return FALSE;
	} else{
                BEGIN_RING(PS3RectangleChannel, NV04_GDI_RECTANGLE_TEXT_OPERATION, 1);
                OUT_RING  (3); /* SRCCOPY */
        }

        if (!PS3AccelGetCtxSurf2DFormatFromPixmap(pPixmap, &fmt))
                return FALSE;

        /* When SURFACE_FORMAT_A8R8G8B8 is used with GDI_RECTANGLE_TEXT, the 
         * alpha channel gets forced to 0xFF for some reason.  We're using 
         * SURFACE_FORMAT_Y32 as a workaround
         */
        if (fmt == NV04_CONTEXT_SURFACES_2D_FORMAT_A8R8G8B8)
                fmt = NV04_CONTEXT_SURFACES_2D_FORMAT_Y32;

        if (!PS3AccelSetCtxSurf2D(pPixmap, pPixmap, fmt))
                return FALSE;

        BEGIN_RING(PS3RectangleChannel, NV04_GDI_RECTANGLE_TEXT_COLOR_FORMAT, 1);
        OUT_RING  (rectFormat(&pPixmap->drawable));
        BEGIN_RING(PS3RectangleChannel, NV04_GDI_RECTANGLE_TEXT_COLOR1_A, 1);
        OUT_RING  (fg);

	return TRUE;
}

static void PS3ExaSolid (PixmapPtr pPixmap, int x1, int y1, int x2, int y2)
{
        ScrnInfoPtr pScrn = xf86Screens[pPixmap->drawable.pScreen->myNum];
        PS3Ptr pPS3 = PS3PTR(pScrn);
        int width = x2 - x1;
        int height = y2 - y1;

	TRACE();

//	ErrorF("Solid %d,%d-%d,%d\n", x1, y1, x2, y2);

        BEGIN_RING(PS3RectangleChannel,
                   NV04_GDI_RECTANGLE_TEXT_UNCLIPPED_RECTANGLE_POINT(0), 2);
        OUT_RING  ((x1 << 16) | y1);
        OUT_RING  ((width << 16) | height);

	FIRE_RING();
}

static void PS3ExaDoneSolid (PixmapPtr pPixmap)
{
	ScrnInfoPtr pScrn = xf86Screens[pPixmap->drawable.pScreen->myNum];
        PS3Ptr pPS3 = PS3PTR(pScrn);

	TRACE();
#if 1
	PS3NotifierReset(pPS3);
	PS3DmaStart(pPS3, PS3RectangleChannel, 0x104, 1 );
	PS3DmaNext(pPS3, 0);
	PS3DmaStart(pPS3, PS3RectangleChannel, 0x100, 1 );
	PS3DmaNext(pPS3, 0);

	FIRE_RING();

	if (!PS3NotifierWaitStatus(pPS3, 0, 2000))
		ErrorF("%s: failed\n", __FUNCTION__);
#endif
}

static Bool PS3ExaPrepareCopy_2(PixmapPtr pSrcPixmap,
				PixmapPtr pDstPixmap,
				int       dx,
				int       dy,
				int       alu,
				Pixel     planemask)
{
	ScrnInfoPtr pScrn = xf86Screens[pSrcPixmap->drawable.pScreen->myNum];
        PS3Ptr pPS3 = PS3PTR(pScrn);
        int fmt;
	int w, h;

	w = pSrcPixmap->drawable.width;
	h = pSrcPixmap->drawable.height;

//	ErrorF("%s %d %d %d %d %d\n", __FUNCTION__, dx, dy, w, h, alu);

	if (pSrcPixmap->drawable.bitsPerPixel !=
	    pDstPixmap->drawable.bitsPerPixel) {
		FALLBACK("different bpp\n");
		return FALSE;
	}

	planemask |= ~0 << pDstPixmap->drawable.bitsPerPixel;
	if (planemask != ~0 || alu != GXcopy) {
		FALLBACK("not copy or planemask\n");
		return FALSE;
	}

	if (!PS3AccelGetCtxSurf2DFormatFromPixmap(pDstPixmap, &fmt))
                return FALSE;
        if (!PS3AccelSetCtxSurf2D(pSrcPixmap, pDstPixmap, fmt))
                return FALSE;

        return TRUE;
}

static void PS3ExaCopy_2(PixmapPtr pDstPixmap,
			 int	srcX,
			 int	srcY,
			 int	dstX,
			 int	dstY,
			 int	width,
			 int	height)
{
	ScrnInfoPtr pScrn = xf86Screens[pDstPixmap->drawable.pScreen->myNum];
        PS3Ptr pPS3 = PS3PTR(pScrn);

//	ErrorF("%s from (%d,%d) to (%d,%d) size %dx%d\n", __FUNCTION__,
//	       srcX, srcY, dstX, dstY, width, height);

	BEGIN_RING(PS3ImageBlitChannel, NV_IMAGE_BLIT_POINT_IN, 3);
	OUT_RING  ((srcY << 16) | srcX);
	OUT_RING  ((dstY << 16) | dstX);
	OUT_RING  ((height  << 16) | width);

	FIRE_RING();
}

static void PS3ExaDoneCopy_2(PixmapPtr pDstPixmap)
{
	ScrnInfoPtr pScrn = xf86Screens[pDstPixmap->drawable.pScreen->myNum];
        PS3Ptr pPS3 = PS3PTR(pScrn);

	PS3NotifierReset(pPS3);
	PS3DmaStart(pPS3, PS3ImageBlitChannel, 0x104, 1 );
	PS3DmaNext(pPS3, 0);
	PS3DmaStart(pPS3, PS3ImageBlitChannel, 0x100, 1 );
	PS3DmaNext(pPS3, 0);

	FIRE_RING();

	if (!PS3NotifierWaitStatus(pPS3, 0, 2000))
		ErrorF("%s: failed\n", __FUNCTION__);
}

static CARD32 copy_src_size, copy_src_pitch, copy_src_offset, copy_dx, copy_dy;

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

	int w, h;


	copy_dx = dx;
	copy_dy = dy;

	if (dx < 0)
		return FALSE;
	if (dy < 0)
		return FALSE;

//	ErrorF("%s %d %d %d %d %d\n", __FUNCTION__, dx, dy, w, h, alu);

	if (pSrcPixmap->drawable.bitsPerPixel !=
	    pDstPixmap->drawable.bitsPerPixel) {
		FALLBACK("different bpp\n");
		return FALSE;
	}

	planemask |= ~0 << pDstPixmap->drawable.bitsPerPixel;
	if (planemask != ~0 || alu != GXcopy) {
		FALLBACK("not copy or planemask\n");
		return FALSE;
	}

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
		FALLBACK("unsupported source format\n");
		return FALSE;
	}

	if (!PS3AccelGetCtxSurf2DFormatFromPixmap(pDstPixmap, &dstFormat)) {
		FALLBACK("cannot get context surface format\n");
		return FALSE;
	}
	if (!PS3AccelSetCtxSurf2D(pSrcPixmap, pDstPixmap, dstFormat)) {
		FALLBACK("cannot set context surface format\n");
		return FALSE;
	}

//	ErrorF("%s %d %d %d %d %d\n", __FUNCTION__, dx, dy, w, h, alu);

	/* screen to screen copy */
	PS3DmaStart(pPS3, PS3ScaledImageChannel,
	NV03_SCALED_IMAGE_FROM_MEMORY_DMA_NOTIFY, 1);
	PS3DmaNext(pPS3, PS3DmaNotifier);

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

/*	ErrorF("%s sfmt=%d dfmt=%d dpitch=%d spitch=%d soffset=0x%x doffset=0x%x\n", __FUNCTION__,
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

//	ErrorF("%s from (%d,%d) to (%d,%d) size %dx%d\n", __FUNCTION__,
//	       srcX, srcY, dstX, dstY, width, height);

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
	PS3DmaNext (pPS3, ((srcY*16 + 8) << 16) | ((srcX*16 + 8) & 0xffff));
	PS3DmaKickoff(pPS3); 

// TEMP
#if 0
	PS3NotifierReset(pPS3);
	ErrorF("xnotifier = %08x,%08x,%08x,%08x\n",
	       pPS3->dmaNotifier[0],
	       pPS3->dmaNotifier[1],
	       pPS3->dmaNotifier[2],
	       pPS3->dmaNotifier[3]);
	PS3DmaStart(pPS3, PS3ScaledImageChannel, 0x104, 1 );
	PS3DmaNext(pPS3, 0);
	PS3DmaStart(pPS3, PS3ScaledImageChannel, 0x100, 1 );
	PS3DmaNext(pPS3, 0);

	PS3DmaKickoff(pPS3); 
	if (PS3NotifierWaitStatus(pPS3, 0, 2000))
		ErrorF("success\n");
	else
		ErrorF("failed\n");
// TEMP
	ErrorF("1notifier = %08x,%08x,%08x,%08x\n",
	       pPS3->dmaNotifier[0],
	       pPS3->dmaNotifier[1],
	       pPS3->dmaNotifier[2],
	       pPS3->dmaNotifier[3]);

// TEMP
	NV40_EmitGeometry(pPS3);
	PS3DmaKickoff(pPS3); 
#endif
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

	PS3Sync(pPS3);

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

		PS3NotifierReset(pPS3);
                BEGIN_RING(PS3MemFormatDownloadChannel,
			   NV_MEMORY_TO_MEMORY_FORMAT_NOTIFY, 1);
                OUT_RING  (0);
                BEGIN_RING(PS3MemFormatDownloadChannel, 0x100, 1);
                OUT_RING  (0);
                FIRE_RING();
                if (!PS3NotifierWaitStatus(pPS3, 0, 2000))
                        return FALSE;

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

//	ErrorF("%s (%d,%d-%dx%d) to %p pitch %d\n", __FUNCTION__,
//	       x, y, w, h, dst, dst_pitch);

	src_offset = PS3AccelGetPixmapOffset(pSrc);
	src_pitch  = exaGetPixmapPitch(pSrc);
	cpp = pSrc->drawable.bitsPerPixel >> 3;
	offset = (y * src_pitch) + (x * cpp);

#if 0
	/* fallback to memcpy transfer */
        src = PS3ExaPixmapMap(pSrc) + offset;
        exaWaitSync(pSrc->drawable.pScreen);
        if (PS3AccelMemcpyRect(dst, src, h, dst_pitch, src_pitch, w*cpp)) {
		ErrorF("using memcpy for DFS\n");
                return TRUE;
	}
#endif

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

		PS3NotifierReset(pPS3);
                BEGIN_RING(PS3MemFormatUploadChannel,
			   NV_MEMORY_TO_MEMORY_FORMAT_NOTIFY, 1);
                OUT_RING  (0);
                BEGIN_RING(PS3MemFormatUploadChannel, 0x100, 1);
                OUT_RING  (0);
                FIRE_RING();
                if (!PS3NotifierWaitStatus(pPS3, 0, 2000))
                        return FALSE;

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

#if 0
        /* fallback to memcpy-based transfer */
        dst = PS3ExaPixmapMap(pDst) + (y * dst_pitch) + (x * cpp);
        exaWaitSync(pDst->drawable.pScreen);
        if (PS3AccelMemcpyRect(dst, src, h, dst_pitch, src_pitch, w*cpp)) {
		ErrorF("using memcpy for UTS\n");
                return TRUE;
	}
#endif

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

extern Bool NV40EXACheckComposite(int op, PicturePtr psPict,
				  PicturePtr pmPict,
				  PicturePtr pdPict);
extern Bool NV40EXAPrepareComposite(int op, PicturePtr psPict,
				    PicturePtr pmPict,
				    PicturePtr pdPict,
				    PixmapPtr  psPix,
				    PixmapPtr  pmPix,
				    PixmapPtr  pdPix);
extern void NV40EXAComposite(PixmapPtr pdPix,
			     int srcX , int srcY,
			     int maskX, int maskY,
			     int dstX , int dstY,
			     int width, int height);
extern void NV40EXADoneComposite(PixmapPtr pdPix);

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
		((2 * pPS3->fboff +
		  pScrn->displayWidth * pScrn->virtualY *
		  (pScrn->bitsPerPixel/8) + 63) & ~63);
	pPS3->EXADriverPtr->memorySize		= pPS3->vram_size;
	pPS3->EXADriverPtr->pixmapOffsetAlign	= 256; 
	pPS3->EXADriverPtr->pixmapPitchAlign	= 64; 
	pPS3->EXADriverPtr->flags		= EXA_OFFSCREEN_PIXMAPS;
	pPS3->EXADriverPtr->maxX		= 32768;
	pPS3->EXADriverPtr->maxY		= 32768;

	pPS3->EXADriverPtr->WaitMarker = PS3ExaWaitMarker;

	/* Install default hooks */
	pPS3->EXADriverPtr->DownloadFromScreen = PS3DownloadFromScreen; 
	pPS3->EXADriverPtr->UploadToScreen = PS3UploadToScreen; 

	pPS3->EXADriverPtr->PrepareCopy = PS3ExaPrepareCopy_2;
	pPS3->EXADriverPtr->Copy = PS3ExaCopy_2;
	pPS3->EXADriverPtr->DoneCopy = PS3ExaDoneCopy_2;

	pPS3->EXADriverPtr->PrepareSolid = PS3ExaPrepareSolid;
	pPS3->EXADriverPtr->Solid = PS3ExaSolid;
	pPS3->EXADriverPtr->DoneSolid = PS3ExaDoneSolid;

	pPS3->EXADriverPtr->CheckComposite   = NV40EXACheckComposite;
	pPS3->EXADriverPtr->PrepareComposite = NV40EXAPrepareComposite;
	pPS3->EXADriverPtr->Composite        = NV40EXAComposite;
	pPS3->EXADriverPtr->DoneComposite    = NV40EXADoneComposite;

	/* Reserve FB memory for DMA notifier and fragment programs */
	pPS3->dmaNotifier = (CARD32 *) ((unsigned long ) pPS3->vram_base +
					pPS3->EXADriverPtr->offScreenBase);
	pPS3->fpMem = (CARD32 *) ((unsigned long) pPS3->dmaNotifier + 64);
	pPS3->EXADriverPtr->offScreenBase	+= 0x1000;
	pPS3->EXADriverPtr->memorySize		-= 0x1000;

	/* Initialize 3D context */
	setup_TCL(pScrn);

	/* Prepare A8 shaders */
	NV40EXAHackupA8Shaders();

	return exaDriverInit(pScreen, pPS3->EXADriverPtr);
}

