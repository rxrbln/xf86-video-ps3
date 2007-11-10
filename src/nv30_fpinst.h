#ifndef __NV30_FPINST_H__
#define __NV30_FPINST_H__

/*
 * Each fragment program opcode appears to be comprised of 4 32-bit values.
 *
 *     0 - Opcode, output reg/mask, ATTRIB source
 *     1 - Source 0
 *     2 - Source 1
 *     3 - Source 2
 *
 * There appears to be no special difference between result regs and temp regs.
 * 		result.color == R0.xyzw
 * 		result.depth == R1.z
 * When the fragprog contains instructions to write depth, NV30_TCL_PRIMITIVE_3D_UNK1D78=0
 * otherwise it is set to 1.
 *
 * Constants are inserted directly after the instruction that uses them.
 * 
 * It appears that it's not possible to use two input registers in one
 * instruction as the input sourcing is done in the instruction dword
 * and not the source selection dwords.  As such instructions such as:
 * 
 *         ADD result.color, fragment.color, fragment.texcoord[0];
 *
 * must be split into two MOV's and then an ADD (nvidia does this) but
 * I'm not sure why it's not just one MOV and then source the second input
 * in the ADD instruction..
 *
 * Negation of the full source is done with NV30_FP_REG_NEGATE, arbitrary
 * negation requires multiplication with a const.
 *
 * Arbitrary swizzling is supported with the exception of SWIZZLE_ZERO/SWIZZLE_ONE
 * The temp/result regs appear to be initialised to (0.0, 0.0, 0.0, 0.0) as SWIZZLE_ZERO
 * is implemented simply by not writing to the relevant components of the destination.
 *
 * Looping
 *   Loops appear to be fairly expensive on NV40 at least, the proprietary driver
 *   goes to a lot of effort to avoid using the native looping instructions.  If
 *   the total number of *executed* instructions between REP/ENDREP or LOOP/ENDLOOP
 *   is <=500, the driver will unroll the loop.  The maximum loop count is 255.
 *
 * Conditional execution
 *   TODO
 * 
 * Non-native instructions:
 *     LIT
 *     LRP - MAD+MAD
 *     SUB - ADD, negate second source
 *     RSQ - LG2 + EX2
 *     POW - LG2 + MUL + EX2
 *     SCS - COS + SIN
 *     XPD
 *     DP2 - MUL + ADD
 *     NRM
 */

//== Opcode / Destination selection ==
#define NV30_FP_OP_PROGRAM_END      (1 << 0)
#define NV30_FP_OP_OUT_REG_SHIFT    1
#define NV30_FP_OP_OUT_REG_MASK    (31 << 1)    /* uncertain */
#define NV30_FP_OP_COND_WRITE_ENABLE (1 << 8)
#define NV30_FP_OP_OUTMASK_SHIFT    9
#define NV30_FP_OP_OUTMASK_MASK    (0xF << 9)
#    define NV30_FP_OP_OUT_X        (1 << 9)
#    define NV30_FP_OP_OUT_Y        (1 << 10)
#    define NV30_FP_OP_OUT_Z        (1 << 11)
#    define NV30_FP_OP_OUT_W        (1 << 12)
/* Uncertain about these, especially the input_src values.. it's possible that
 * they can be dynamically changed.
 */
#define NV30_FP_OP_INPUT_SRC_SHIFT    13
#define NV30_FP_OP_INPUT_SRC_MASK    (15 << 13)
#    define NV30_FP_OP_INPUT_SRC_POSITION    0x0
#    define NV30_FP_OP_INPUT_SRC_COL0        0x1
#    define NV30_FP_OP_INPUT_SRC_COL1        0x2
#    define NV30_FP_OP_INPUT_SRC_FOGC        0x3
#    define NV30_FP_OP_INPUT_SRC_TC0         0x4
#    define NV30_FP_OP_INPUT_SRC_TC(n)       (0x4 + n)
#define NV30_FP_OP_TEX_UNIT_SHIFT    17
#define NV30_FP_OP_TEX_UNIT_MASK    (0xF << 17) /* guess */
#define NV30_FP_OP_PRECISION_SHIFT 22
#define NV30_FP_OP_PRECISION_MASK  (3 << 22)
#   define NV30_FP_PRECISION_FP32  0
#   define NV30_FP_PRECISION_FP16  1
#   define NV30_FP_PRECISION_FX12  2
#define NV30_FP_OP_OPCODE_SHIFT    24
#define NV30_FP_OP_OPCODE_MASK        (0x3F << 24)
#    define NV30_FP_OP_OPCODE_NOP    0x00
#    define NV30_FP_OP_OPCODE_MOV    0x01
#    define NV30_FP_OP_OPCODE_MUL    0x02
#    define NV30_FP_OP_OPCODE_ADD    0x03
#    define NV30_FP_OP_OPCODE_MAD    0x04
#    define NV30_FP_OP_OPCODE_DP3    0x05
#    define NV30_FP_OP_OPCODE_DP4    0x06
#    define NV30_FP_OP_OPCODE_DST    0x07
#    define NV30_FP_OP_OPCODE_MIN    0x08
#    define NV30_FP_OP_OPCODE_MAX    0x09
#    define NV30_FP_OP_OPCODE_SLT    0x0A
#    define NV30_FP_OP_OPCODE_SGE    0x0B
#    define NV30_FP_OP_OPCODE_SLE    0x0C
#    define NV30_FP_OP_OPCODE_SGT    0x0D
#    define NV30_FP_OP_OPCODE_SNE    0x0E
#    define NV30_FP_OP_OPCODE_SEQ    0x0F
#    define NV30_FP_OP_OPCODE_FRC    0x10
#    define NV30_FP_OP_OPCODE_FLR    0x11
#    define NV30_FP_OP_OPCODE_PK4B   0x13
#    define NV30_FP_OP_OPCODE_UP4B   0x14
#    define NV30_FP_OP_OPCODE_DDX    0x15 /* can only write XY */
#    define NV30_FP_OP_OPCODE_DDY    0x16 /* can only write XY */
#    define NV30_FP_OP_OPCODE_TEX    0x17
#    define NV30_FP_OP_OPCODE_TXP    0x18
#    define NV30_FP_OP_OPCODE_TXD    0x19
#    define NV30_FP_OP_OPCODE_RCP    0x1A
#    define NV30_FP_OP_OPCODE_EX2    0x1C
#    define NV30_FP_OP_OPCODE_LG2    0x1D
#    define NV30_FP_OP_OPCODE_COS    0x22
#    define NV30_FP_OP_OPCODE_SIN    0x23
#    define NV30_FP_OP_OPCODE_PK2H   0x24
#    define NV30_FP_OP_OPCODE_UP2H   0x25
#    define NV30_FP_OP_OPCODE_PK4UB  0x27
#    define NV30_FP_OP_OPCODE_UP4UB  0x28
#    define NV30_FP_OP_OPCODE_PK2US  0x29
#    define NV30_FP_OP_OPCODE_UP2US  0x2A
#    define NV30_FP_OP_OPCODE_DP2A   0x2E
#    define NV30_FP_OP_OPCODE_TXL    0x2F
#    define NV30_FP_OP_OPCODE_TXB    0x31
#    define NV30_FP_OP_OPCODE_DIV    0x3A
/* The use of these instructions appears to be indicated by bit 31 of DWORD 2.*/
#    define NV30_FP_OP_BRA_OPCODE_BRK    0x0
#    define NV30_FP_OP_BRA_OPCODE_CAL    0x1
#    define NV30_FP_OP_BRA_OPCODE_IF     0x2
#    define NV30_FP_OP_BRA_OPCODE_LOOP   0x3
#    define NV30_FP_OP_BRA_OPCODE_REP    0x4
#    define NV30_FP_OP_BRA_OPCODE_RET    0x5
#define NV30_FP_OP_OUT_SAT            (1 << 31)

/* high order bits of SRC0 */
#define NV30_FP_OP_OUT_ABS            (1 << 29)
#define NV30_FP_OP_COND_SWZ_W_SHIFT   27
#define NV30_FP_OP_COND_SWZ_W_MASK    (3 << 27)
#define NV30_FP_OP_COND_SWZ_Z_SHIFT   25
#define NV30_FP_OP_COND_SWZ_Z_MASK    (3 << 25)
#define NV30_FP_OP_COND_SWZ_Y_SHIFT   23
#define NV30_FP_OP_COND_SWZ_Y_MASK    (3 << 23)
#define NV30_FP_OP_COND_SWZ_X_SHIFT   21
#define NV30_FP_OP_COND_SWZ_X_MASK    (3 << 21)
#define NV30_FP_OP_COND_SWZ_ALL_SHIFT 21
#define NV30_FP_OP_COND_SWZ_ALL_MASK  (0xFF << 21)
#define NV30_FP_OP_COND_SHIFT         18
#define NV30_FP_OP_COND_MASK          (0x07 << 18)
#    define NV30_FP_OP_COND_FL     0
#    define NV30_FP_OP_COND_LT     1
#    define NV30_FP_OP_COND_EQ     2
#    define NV30_FP_OP_COND_LE     3
#    define NV30_FP_OP_COND_GT     4
#    define NV30_FP_OP_COND_NE     5
#    define NV30_FP_OP_COND_GE     6
#    define NV30_FP_OP_COND_TR     7

/* high order bits of SRC1 */
#define NV30_FP_OP_SRC_SCALE_SHIFT    28
#define NV30_FP_OP_SRC_SCALE_MASK     (3 << 28)

/* SRC1 LOOP */
#define NV30_FP_OP_LOOP_INCR_SHIFT    19
#define NV30_FP_OP_LOOP_INCR_MASK     (0xFF << 19)
#define NV30_FP_OP_LOOP_INDEX_SHIFT   10
#define NV30_FP_OP_LOOP_INDEX_MASK    (0xFF << 10)
#define NV30_FP_OP_LOOP_COUNT_SHIFT   2
#define NV30_FP_OP_LOOP_COUNT_MASK    (0xFF << 2) /* from MAX_PROGRAM_LOOP_COUNT_NV */

/* SRC1 IF */
#define NV30_FP_OP_ELSE_ID_SHIFT      2
#define NV30_FP_OP_ELSE_ID_MASK       (0xFF << 2) /* UNKNOWN */

/* SRC1 CAL */
#define NV30_FP_OP_IADDR_SHIFT        2
#define NV30_FP_OP_IADDR_MASK         (0xFF << 2)

/* SRC1 REP
 *   I have no idea why there are 3 count values here..  but they
 *   have always been filled with the same value in my tests so
 *   far..
 */
#define NV30_FP_OP_REP_COUNT1_SHIFT   2
#define NV30_FP_OP_REP_COUNT1_MASK    (0xFF << 2)
#define NV30_FP_OP_REP_COUNT2_SHIFT   10
#define NV30_FP_OP_REP_COUNT2_MASK    (0xFF << 10)
#define NV30_FP_OP_REP_COUNT3_SHIFT   19
#define NV30_FP_OP_REP_COUNT3_MASK    (0xFF << 19)

/* SRC2 REP/IF */
#define NV30_FP_OP_END_ID_SHIFT       2
#define NV30_FP_OP_END_ID_MASK        (0xFF << 2) /* UNKNOWN */

// SRC2 high-order
#define NV30_FP_OP_INDEX_INPUT        (1 << 30)
#define NV30_FP_OP_ADDR_INDEX_SHIFT   19
#define NV30_FP_OP_ADDR_INDEX_MASK    (0xF << 19) //UNKNOWN

//== Register selection ==
#define NV30_FP_REG_TYPE_SHIFT		0
#define NV30_FP_REG_TYPE_MASK		(3 << 0)
#	define NV30_FP_REG_TYPE_TEMP		0
#	define NV30_FP_REG_TYPE_INPUT		1
#	define NV30_FP_REG_TYPE_CONST		2
#define NV30_FP_REG_ID_SHIFT        2            /* uncertain */
#define NV30_FP_REG_ID_MASK        (31 << 2)
#define NV30_FP_REG_UNK_0            (1 << 8)
#define NV30_FP_REG_SWZ_ALL_SHIFT    9
#define NV30_FP_REG_SWZ_ALL_MASK    (255 << 9)
#define NV30_FP_REG_SWZ_X_SHIFT    9
#define NV30_FP_REG_SWZ_X_MASK        (3 << 9)
#define NV30_FP_REG_SWZ_Y_SHIFT    11
#define NV30_FP_REG_SWZ_Y_MASK        (3 << 11)
#define NV30_FP_REG_SWZ_Z_SHIFT    13
#define NV30_FP_REG_SWZ_Z_MASK        (3 << 13)
#define NV30_FP_REG_SWZ_W_SHIFT    15
#define NV30_FP_REG_SWZ_W_MASK        (3 << 15)
#    define NV30_FP_SWIZZLE_X        0
#    define NV30_FP_SWIZZLE_Y        1
#    define NV30_FP_SWIZZLE_Z        2
#    define NV30_FP_SWIZZLE_W        3
#define NV30_FP_REG_NEGATE            (1 << 17)


#define NV30FP_MAKE_SWIZZLE(x, y, z, w) ((w<<6)|(z<<4)|(y<<2)|(x))
#define NV30FP_SWIZZLE(x, y, z, w) NV30FP_MAKE_SWIZZLE( \
		NV30_FP_SWIZZLE_##x, \
		NV30_FP_SWIZZLE_##y, \
		NV30_FP_SWIZZLE_##z, \
		NV30_FP_SWIZZLE_##w)

#define NV30_FP_DEST_TYPE_TEMP 0
#define NV30_FP_DEST_TYPE_RESULT 1
#define NV30FP_LOCALS uint32_t *fp_cur; int fp_hc = 0
#define NV30FP_SETBUF(buf) fp_cur = (uint32_t*)(buf)
#define NV30FP_ARITH_INST_SET_DEFAULTS do { \
	fp_cur[0] = 0; fp_cur[1] = 0; fp_cur[2] = 0; fp_cur[3] = 0; \
	NV30FP_ARITH(NOP, TEMP, 0, 0, 0, 0, 0); \
	NV30FP_COND (TR, X, Y, Z, W, 0); \
	NV30FP_SOURCE_UNUSED(0); \
	NV30FP_SOURCE_UNUSED(1); \
	NV30FP_SOURCE_UNUSED(2); \
} while(0)
#define NV30FP_NEXT do { \
	fprintf(stderr, "fpi[0] = 0x%08x\n", fp_cur[0]); \
	fprintf(stderr, "fpi[1] = 0x%08x\n", fp_cur[1]); \
	fprintf(stderr, "fpi[2] = 0x%08x\n", fp_cur[2]); \
	fprintf(stderr, "fpi[3] = 0x%08x\n", fp_cur[3]); \
	fp_cur+=(4+(fp_hc?4:0)); fp_hc=0; \
} while(0)
#define NV30FP_LAST_INST do { \
	fp_cur[0] |= NV30_FP_OP_PROGRAM_END; \
	NV30FP_NEXT; \
	fp_cur[0] = 0x00000001; \
	fp_cur++; \
	fp_hc = 0; \
} while(0)
#define NV30FP_DEST_MASK(x,y,z,w) ((w<<3)|(z<<2)|(y<<1)|x)

#define NV30FP_SET_ARITH(op, dt, dr, mask) fp_cur[0] = (fp_cur[0] \
		& ~(	NV30_FP_OP_OUTMASK_MASK \
			|	NV30_FP_OP_OPCODE_MASK \
			|	NV30_FP_OP_OUT_REG_MASK \
			)) \
		| (		(  (op) << NV30_FP_OP_OPCODE_SHIFT) \
			| 	((mask) << NV30_FP_OP_OUTMASK_SHIFT) \
			| 	(  (dr) << NV30_FP_OP_OUT_REG_SHIFT) \
			|	((dt == NV30_FP_DEST_TYPE_RESULT) ? (1<<7) : 0) \
			)
#define NV30FP_ARITH(op, dt, dr, mx, my, mz, mw) \
	NV30FP_SET_ARITH(NV30_FP_OP_OPCODE_##op, NV30_FP_DEST_TYPE_##dt, dr, NV30FP_DEST_MASK(mx,my,mz,mw))
#define NV30FP_TEX(op, dt, dr, mx, my, mz, mw, unit) do { \
	NV30FP_SET_ARITH(NV30_FP_OP_OPCODE_##op, NV30_FP_DEST_TYPE_##dt, dr, NV30FP_DEST_MASK(mx,my,mz,mw)); \
	fp_cur[0] = (fp_cur[0] & ~(NV30_FP_OP_TEX_UNIT_MASK)) | (unit << NV30_FP_OP_TEX_UNIT_SHIFT); \
} while(0)
#define NV30FP_SET_SOURCE(pos, id, type, swz, neg, abs) do { \
	fp_cur[(pos)+1] = ((fp_cur[(pos)+1] \
			& ~(	NV30_FP_REG_ID_MASK \
				|	NV30_FP_REG_SWZ_ALL_MASK \
				|	NV30_FP_REG_TYPE_MASK \
			   )) \
			| (		((type) << NV30_FP_REG_TYPE_SHIFT) \
				| 	((id)   << NV30_FP_REG_ID_SHIFT) \
				| 	((swz)  << NV30_FP_REG_SWZ_ALL_SHIFT) \
				| 	(neg ? NV30_FP_REG_NEGATE : 0) \
				) \
			); \
} while(0)
#define NV30FP_SOURCE_CONST(pos, sx, sy, sz, sw, neg, abs, valx, valy, valz, valw) do { \
	float v; \
	NV30FP_SET_SOURCE((pos), 0, NV30_FP_REG_TYPE_CONST, NV30FP_SWIZZLE(sx,sy,sz,sw), (neg), (abs)); \
	v = (valx); fp_cur[4] = *(uint32_t*)&v; \
	v = (valy); fp_cur[5] = *(uint32_t*)&v; \
	v = (valz); fp_cur[6] = *(uint32_t*)&v; \
	v = (valw); fp_cur[7] = *(uint32_t*)&v; \
	fp_hc = 1; \
} while(0)
#define NV30FP_SOURCE_INPUT(pos, id, sx, sy, sz, sw, neg, abs, in) do { \
	NV30FP_SET_SOURCE((pos), (id), NV30_FP_REG_TYPE_INPUT, NV30FP_SWIZZLE(sx,sy,sz,sw), (neg), (abs)); \
	fp_cur[0] = (fp_cur[0] & ~(NV30_FP_OP_INPUT_SRC_MASK)) | ((id) << NV30_FP_OP_INPUT_SRC_SHIFT); \
	fp_cur[3] = (fp_cur[3] & ~(NV30_FP_OP_INDEX_INPUT)) | ((in) ? NV30_FP_OP_INDEX_INPUT : 0); \
} while(0)
#define NV30FP_SOURCE_TEMP(pos, id, sx, sy, sz, sw, neg, abs) \
	NV30FP_SET_SOURCE((pos), (id), NV30_FP_REG_TYPE_TEMP, NV30FP_SWIZZLE(sx,sy,sz,sw), (neg), (abs))
#define NV30FP_SOURCE_UNUSED(pos) \
	NV30FP_SOURCE_INPUT((pos), 0, X, Y, Z, W, 0, 0, 0)

#define NV30FP_SET_COND(cond, swz, wr) do { \
	fp_cur[0] = (fp_cur[0] & ~(NV30_FP_OP_COND_WRITE_ENABLE)) | ((wr) ? NV30_FP_OP_COND_WRITE_ENABLE : 0); \
	fp_cur[1] = (fp_cur[1] \
			& ~(	NV30_FP_OP_COND_SWZ_ALL_MASK \
				|	NV30_FP_OP_COND_MASK \
			   )) \
			| (		(swz) << NV30_FP_OP_COND_SWZ_ALL_SHIFT \
				|	(cond) << NV30_FP_OP_COND_SHIFT \
			  ); \
} while(0)
#define NV30FP_COND(cond, sx, sy, sz, sw, wr) \
	NV30FP_SET_COND(NV30_FP_OP_COND_##cond, NV30FP_SWIZZLE(sx,sy,sz,sw), (wr))

#endif
