#ifndef __NV_SHADERS_H__
#define __NV_SHADERS_H__

#define NV_SHADER_MAX_PROGRAM_LENGTH 256

typedef struct nv_shader {
	uint32_t hw_id;
	uint32_t size;
	union {
		struct {
			uint32_t vp_in_reg;
			uint32_t vp_out_reg;
		} NV30VP;
		struct  {
			uint32_t num_regs;
		} NV30FP;
	} card_priv;
	uint32_t data[NV_SHADER_MAX_PROGRAM_LENGTH];
} nv_shader_t;

/*******************************************************************************
 * NV40/G70 vertex shaders
 */

static nv_shader_t nv40_vp_exa_render = {
  .card_priv.NV30VP.vp_in_reg  = 0x00000309,
  .card_priv.NV30VP.vp_out_reg = 0x0000c001,
  .size = (3*4),
  .data = {
    /* MOV result.position, vertex.position */
    0x40041c6c, 0x0040000d, 0x8106c083, 0x6041ff80,
    /* MOV result.texcoord[0], vertex.texcoord[0] */
    0x401f9c6c, 0x0040080d, 0x8106c083, 0x6041ff9c,
    /* MOV result.texcoord[1], vertex.texcoord[1] */
    0x401f9c6c, 0x0040090d, 0x8106c083, 0x6041ffa1,
  }
};

// TESTING
static nv_shader_t nv40_vp = {
  .card_priv.NV30VP.vp_in_reg  = 0x00000309,
  .card_priv.NV30VP.vp_out_reg = 0x0000c001,
  .size = (3*4),
  .data = {
    /* MOV result.position, vertex.position */
    0x40041c6c, 0x0040000d, 0x8106c083, 0x6041ff80,
    /* MOV result.texcoord[0], vertex.texcoord[0] */
    0x401f9c6c, 0x0040080d, 0x8106c083, 0x6041ff9c,
    /* MOV result.texcoord[1], vertex.texcoord[1] */
    0x401f9c6c, 0x0040090d, 0x8106c083, 0x6041ffa1,
  }
};



/*******************************************************************************
 * NV30/NV40/G70 fragment shaders
 */

static nv_shader_t nv30_fp_pass_col0 = {
  .card_priv.NV30FP.num_regs = 2,
  .size = (1*4),
  .data = {
    /* MOV R0, fragment.color */
    0x01403e81, 0x1c9dc801, 0x0001c800, 0x3fe1c800, 
  }
};

static nv_shader_t nv30_fp_pass_tex0 = {
  .card_priv.NV30FP.num_regs = 2,
  .size = (2*4),
  .data = {
    /* TEX R0, fragment.texcoord[0], texture[0], 2D */
    0x17009e00, 0x1c9dc801, 0x0001c800, 0x3fe1c800,
    /* MOV R0, R0 */
    0x01401e81, 0x1c9dc800, 0x0001c800, 0x0001c800,
  }
};

static nv_shader_t nv30_fp_composite_mask = {
  .card_priv.NV30FP.num_regs = 2,
  .size = (3*4),
  .data = {
    /* TEXC0 R1.w         , fragment.texcoord[1], texture[1], 2D */
    0x1702b102, 0x1c9dc801, 0x0001c800, 0x3fe1c800,
    /* TEX   R0 (NE0.wwww), fragment.texcoord[0], texture[0], 2D */
    0x17009e00, 0x1ff5c801, 0x0001c800, 0x3fe1c800,
    /* MUL   R0           , R0, R1.w */
    0x02001e81, 0x1c9dc800, 0x0001fe04, 0x0001c800,
  }
};

static nv_shader_t nv30_fp_composite_mask_sa_ca = {
  .card_priv.NV30FP.num_regs = 2,
  .size = (3*4),
  .data = {
    /* TEXC0 R1.w         , fragment.texcoord[0], texture[0], 2D */
    0x17009102, 0x1c9dc801, 0x0001c800, 0x3fe1c800,
    /* TEX   R0 (NE0.wwww), fragment.texcoord[1], texture[1], 2D */
    0x1702be00, 0x1ff5c801, 0x0001c800, 0x3fe1c800,
    /* MUL   R0           , R1,wwww, R0 */
    0x02001e81, 0x1c9dfe04, 0x0001c800, 0x0001c800,
  }
};

static nv_shader_t nv30_fp_composite_mask_ca = {
  .card_priv.NV30FP.num_regs = 2,
  .size = (3*4),
  .data = {
    /* TEXC0 R0           , fragment.texcoord[0], texture[0], 2D */
    0x17009f00, 0x1c9dc801, 0x0001c800, 0x3fe1c800,
    /* TEX   R1 (NE0.xyzw), fragment.texcoord[1], texture[1], 2D */
    0x1702be02, 0x1c95c801, 0x0001c800, 0x3fe1c800,
    /* MUL   R0           , R0, R1 */
    0x02001e81, 0x1c9dc800, 0x0001c804, 0x0001c800,
  }
};

// TESTING
static nv_shader_t nv30_fp = {
  .card_priv.NV30FP.num_regs = 2,
  .size = (2*4),
  .data = {
    /* TEX R0, fragment.texcoord[0], texture[0], 2D */
    0x17009e00, 0x1c9dc801, 0x0001c800, 0x3fe1c800,
    /* MOV R0, R0 */
    0x01401e81, 0x1c9dc800, 0x0001c800, 0x0001c800,
  }
};


#endif
