#ifndef __NV_SHADERS_H__
#define __NV_SHADERS_H__

#define NV_SHADER_MAX_PROGRAM_LENGTH 256

typedef struct nv_vshader {
	uint32_t hw_id;
	uint32_t size;
	
	uint32_t vp_in_reg;
	uint32_t vp_out_reg;
	
	uint32_t data[NV_SHADER_MAX_PROGRAM_LENGTH];
} nv_vshader_t;

typedef struct nv_pshader {
	uint32_t hw_id;
	uint32_t size;
	uint32_t num_regs;
	uint32_t data[NV_SHADER_MAX_PROGRAM_LENGTH];
} nv_pshader_t;

/*******************************************************************************
 * NV40/G70 vertex shaders
 */

static nv_vshader_t nv40_vp = {

  .vp_in_reg  = 0x00000309,
  .vp_out_reg = 0x0000c001,
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

static nv_pshader_t nv30_fp = {
.num_regs = 2,
.size = (2*4),
.data = {
/* TEX R0, fragment.texcoord[0], texture[0], 2D */
0x17009e00, 0x1c9dc801, 0x0001c800, 0x3fe1c800,
/* MOV R0, R0 */
0x01401e81, 0x1c9dc800, 0x0001c800, 0x0001c800,
}
};



#endif
