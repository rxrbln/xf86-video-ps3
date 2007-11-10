#ifndef __NV40_VPINST_H__
#define __NV40_VPINST_H__

/* Vertex programs instruction set
 *
 * 128bit opcodes, split into 4 32-bit ones for ease of use.
 *
 * Non-native instructions
 *     ABS - MOV + NV40_VP_INST0_DEST_ABS
 *     POW - EX2 + MUL + LG2
 *     SUB - ADD, second source negated
 *     SWZ - MOV
 *     XPD -  
 *
 * Register access
 *     - Only one INPUT can be accessed per-instruction (move extras into TEMPs)
 *     - Only one CONST can be accessed per-instruction (move extras into TEMPs)
 *
 * Relative Addressing
 *     According to the value returned for MAX_PROGRAM_NATIVE_ADDRESS_REGISTERS_ARB
 *     there are only two address registers available.  The destination in the ARL
 *     instruction is set to TEMP <n> (The temp isn't actually written).
 *
 *     When using vanilla ARB_v_p, the proprietary driver will squish both the available
 *     ADDRESS regs into the first hardware reg in the X and Y components.
 *
 *     To use an address reg as an index into consts, the CONST_SRC is set to
 *     (const_base + offset) and INDEX_CONST is set.
 *
 *     It is similar for inputs, INPUT_SRC is set to the offset value and INDEX_INPUT
 *     is set.
 *
 *     To access the second address reg use ADDR_REG_SELECT_1. A particular component
 *     of the address regs is selected with ADDR_SWZ.
 *
 *     Only one address register can be accessed per instruction, but you may use
 *     the address reg as an index into both consts and inputs in the same instruction
 *     as long as the swizzles also match.
 *
 * Conditional execution (see NV_vertex_program{2,3} for details)
 *     All instructions appear to be able to modify one of two condition code registers.
 *     This is enabled by setting COND_UPDATE_ENABLE.  The second condition registers is
 *     updated by setting COND_REG_SELECT_1.
 *
 *     Conditional execution of an instruction is enabled by setting COND_TEST_ENABLE, and
 *     selecting the condition which will allow the test to pass with COND_{FL,LT,...}.
 *     It is possible to swizzle the values in the condition register, which allows for
 *     testing against an individual component.
 *
 * Branching
 *     The BRA/CAL instructions seem to follow a slightly different opcode layout.  The
 *     destination instruction ID (IADDR) overlaps SRC2.  Instruction ID's seem to be
 *     numbered based on the UPLOAD_FROM_ID FIFO command, and is incremented automatically
 *     on each UPLOAD_INST FIFO command.
 *
 *     Conditional branching is achieved by using the condition tests described above.
 *     There doesn't appear to be dedicated looping instructions, but this can be done
 *     using a temp reg + conditional branching.
 *
 *     Subroutines may be uploaded before the main program itself, but the first executed
 *     instruction is determined by the PROGRAM_START_ID FIFO command.
 *
 * Texture lookup
 *     TODO
 */

/* ---- OPCODE BITS 127:96 / data DWORD 0 --- */
#define NV40_VP_INST_DEST_RESULT          (1 << 30)
#define NV40_VP_INST_COND_UPDATE_ENABLE   ((1 << 14)|1<<29) /* unsure about this */
#define NV40_VP_INST_INDEX_INPUT          (1 << 27) /* Use an address reg as in index into attribs */
#define NV40_VP_INST_COND_REG_SELECT_1    (1 << 25)
#define NV40_VP_INST_ADDR_REG_SELECT_1    (1 << 24)
#define NV40_VP_INST_SRC2_ABS             (1 << 23)
#define NV40_VP_INST_SRC1_ABS             (1 << 22)
#define NV40_VP_INST_SRC0_ABS             (1 << 21)
#define NV40_VP_INST_VEC_DEST_TEMP_SHIFT  15
#define NV40_VP_INST_VEC_DEST_TEMP_MASK   (0x3F << 15)
#define NV40_VP_INST_COND_TEST_ENABLE     (1 << 13) /* write masking based on condition test */
#define NV40_VP_INST_COND_SHIFT           10
#define NV40_VP_INST_COND_MASK            (0x7 << 10)
#    define NV40_VP_INST_COND_FL     0
#    define NV40_VP_INST_COND_LT     1
#    define NV40_VP_INST_COND_EQ     2
#    define NV40_VP_INST_COND_LE     3
#    define NV40_VP_INST_COND_GT     4
#    define NV40_VP_INST_COND_NE     5
#    define NV40_VP_INST_COND_GE     6
#    define NV40_VP_INST_COND_TR     7
#define NV40_VP_INST_COND_SWZ_X_SHIFT     8
#define NV40_VP_INST_COND_SWZ_X_MASK      (3 << 8)
#define NV40_VP_INST_COND_SWZ_Y_SHIFT     6
#define NV40_VP_INST_COND_SWZ_Y_MASK      (3 << 6)
#define NV40_VP_INST_COND_SWZ_Z_SHIFT     4
#define NV40_VP_INST_COND_SWZ_Z_MASK      (3 << 4)
#define NV40_VP_INST_COND_SWZ_W_SHIFT     2
#define NV40_VP_INST_COND_SWZ_W_MASK      (3 << 2)
#define NV40_VP_INST_COND_SWZ_ALL_SHIFT   2
#define NV40_VP_INST_COND_SWZ_ALL_MASK    (0xFF << 2)
#define NV40_VP_INST_ADDR_SWZ_SHIFT       0
#define NV40_VP_INST_ADDR_SWZ_MASK        (0x03 << 0)
#define NV40_VP_INST0_KNOWN ( \
		NV40_VP_INST_INDEX_INPUT | \
		NV40_VP_INST_COND_REG_SELECT_1 | \
		NV40_VP_INST_ADDR_REG_SELECT_1 | \
		NV40_VP_INST_SRC2_ABS | \
		NV40_VP_INST_SRC1_ABS | \
		NV40_VP_INST_SRC0_ABS | \
		NV40_VP_INST_VEC_DEST_TEMP_MASK | \
		NV40_VP_INST_COND_TEST_ENABLE | \
		NV40_VP_INST_COND_MASK | \
		NV40_VP_INST_COND_SWZ_ALL_MASK | \
		NV40_VP_INST_ADDR_SWZ_MASK)

/* ---- OPCODE BITS 95:64 / data DWORD 1 --- */
#define NV40_VP_INST_VEC_OPCODE_SHIFT     22
#define NV40_VP_INST_VEC_OPCODE_MASK      (0x1F << 22)
#    define NV40_VP_INST_OP_NOP           0x00
#    define NV40_VP_INST_OP_MOV           0x01
#    define NV40_VP_INST_OP_MUL           0x02
#    define NV40_VP_INST_OP_ADD           0x03
#    define NV40_VP_INST_OP_MAD           0x04
#    define NV40_VP_INST_OP_DP3           0x05
#    define NV40_VP_INST_OP_DP4           0x07
#    define NV40_VP_INST_OP_DPH           0x06
#    define NV40_VP_INST_OP_DST           0x08
#    define NV40_VP_INST_OP_MIN           0x09
#    define NV40_VP_INST_OP_MAX           0x0A
#    define NV40_VP_INST_OP_SLT           0x0B
#    define NV40_VP_INST_OP_SGE           0x0C
#    define NV40_VP_INST_OP_ARL           0x0D
#    define NV40_VP_INST_OP_FRC           0x0E
#    define NV40_VP_INST_OP_FLR           0x0F
#    define NV40_VP_INST_OP_SEQ           0x10
#    define NV40_VP_INST_OP_SFL           0x11
#    define NV40_VP_INST_OP_SGT           0x12
#    define NV40_VP_INST_OP_SLE           0x13
#    define NV40_VP_INST_OP_SNE           0x14
#    define NV40_VP_INST_OP_STR           0x15
#    define NV40_VP_INST_OP_SSG           0x16
#    define NV40_VP_INST_OP_ARR           0x17
#    define NV40_VP_INST_OP_ARA           0x18
#define NV40_VP_INST_SCA_OPCODE_SHIFT     27
#define NV40_VP_INST_SCA_OPCODE_MASK      (0x1F << 27)
#    define NV40_VP_INST_OP_RCP           0x02
#    define NV40_VP_INST_OP_RCC           0x03
#    define NV40_VP_INST_OP_RSQ           0x04
#    define NV40_VP_INST_OP_EXP           0x05
#    define NV40_VP_INST_OP_LOG           0x06
#    define NV40_VP_INST_OP_LIT           0x07
#    define NV40_VP_INST_OP_BRA           0x09
#    define NV40_VP_INST_OP_CAL           0x0B
#    define NV40_VP_INST_OP_RET           0x0C
#    define NV40_VP_INST_OP_LG2           0x0D
#    define NV40_VP_INST_OP_EX2           0x0E
#    define NV40_VP_INST_OP_SIN           0x0F
#    define NV40_VP_INST_OP_COS           0x10
#    define NV40_VP_INST_OP_PUSHA         0x13
#    define NV40_VP_INST_OP_POPA          0x14
#define NV40_VP_INST_CONST_SRC_SHIFT      12
#define NV40_VP_INST_CONST_SRC_MASK       (0xFF << 12)
#define NV40_VP_INST_INPUT_SRC_SHIFT      8 
#define NV40_VP_INST_INPUT_SRC_MASK       (0x0F << 8)
#    define NV40_VP_INST_IN_POS           0      /* These seem to match the bindings specified in   */
#    define NV40_VP_INST_IN_WEIGHT        1      /* the ARB_v_p spec (2.14.3.1)                     */
#    define NV40_VP_INST_IN_NORMAL        2      
#    define NV40_VP_INST_IN_COL0          3      /* Should probably confirm them all thougth        */
#    define NV40_VP_INST_IN_COL1          4
#    define NV40_VP_INST_IN_FOGC          5
#    define NV40_VP_INST_IN_TC0           8
#    define NV40_VP_INST_IN_TC(n)         (8+n)
#define NV40_VP_INST_SRC0H_SHIFT          0
#define NV40_VP_INST_SRC0H_MASK           (0xFF << 0)
#define NV40_VP_INST1_KNOWN ( \
		NV40_VP_INST_VEC_OPCODE_MASK | \
		NV40_VP_INST_SCA_OPCODE_MASK | \
		NV40_VP_INST_CONST_SRC_MASK  | \
		NV40_VP_INST_INPUT_SRC_MASK  | \
		NV40_VP_INST_SRC0H_MASK \
		)

/* ---- OPCODE BITS 63:32 / data DWORD 2 --- */
#define NV40_VP_INST_SRC0L_SHIFT          23
#define NV40_VP_INST_SRC0L_MASK           (0x1FF << 23)
#define NV40_VP_INST_SRC1_SHIFT           6
#define NV40_VP_INST_SRC1_MASK            (0x1FFFF << 6)
#define NV40_VP_INST_SRC2H_SHIFT          0
#define NV40_VP_INST_SRC2H_MASK           (0x3F << 0)
#define NV40_VP_INST_IADDRH_SHIFT         0
#define NV40_VP_INST_IADDRH_MASK          (0x1F << 0) /* guess, need to test this */

/* ---- OPCODE BITS 31:0 / data DWORD 3 --- */
#define NV40_VP_INST_IADDRL_SHIFT         29        
#define NV40_VP_INST_IADDRL_MASK          (7 << 29)
#define NV40_VP_INST_SRC2L_SHIFT          21
#define NV40_VP_INST_SRC2L_MASK           (0x7FF << 21)
#define NV40_VP_INST_SCA_WRITEMASK_SHIFT      17
#define NV40_VP_INST_SCA_WRITEMASK_MASK       (0xF << 17)
#    define NV40_VP_INST_SCA_WRITEMASK_X      (1 << 20)
#    define NV40_VP_INST_SCA_WRITEMASK_Y      (1 << 19)
#    define NV40_VP_INST_SCA_WRITEMASK_Z      (1 << 18)
#    define NV40_VP_INST_SCA_WRITEMASK_W      (1 << 17)
#define NV40_VP_INST_VEC_WRITEMASK_SHIFT      13
#define NV40_VP_INST_VEC_WRITEMASK_MASK       (0xF << 13)
#    define NV40_VP_INST_VEC_WRITEMASK_X      (1 << 16)
#    define NV40_VP_INST_VEC_WRITEMASK_Y      (1 << 15)
#    define NV40_VP_INST_VEC_WRITEMASK_Z      (1 << 14)
#    define NV40_VP_INST_VEC_WRITEMASK_W      (1 << 13)
#define NV40_VP_INST_SCA_DEST_TEMP_SHIFT   7
#define NV40_VP_INST_SCA_DEST_TEMP_MASK    (0x3F << 7)
#define NV40_VP_INST_DEST_SHIFT           2
#define NV40_VP_INST_DEST_MASK            (31 << 2)
#    define NV40_VP_INST_DEST_POS         0
#    define NV40_VP_INST_DEST_COL0        1
#    define NV40_VP_INST_DEST_COL1        2
#    define NV40_VP_INST_DEST_BFC0        3
#    define NV40_VP_INST_DEST_BFC1        4
#    define NV40_VP_INST_DEST_FOGC        5
#    define NV40_VP_INST_DEST_PSZ         6
#    define NV40_VP_INST_DEST_TC0         7
#    define NV40_VP_INST_DEST_TC(n)       (7+n)
#    define NV40_VP_INST_DEST_TEMP        0x1F     /* see NV40_VP_INST0_* for actual register */
#define NV40_VP_INST_INDEX_CONST          (1 << 1)
#define NV40_VP_INST_PROGRAM_END          (1 << 0)
#define NV40_VP_INST3_KNOWN ( \
		NV40_VP_INST_SRC2L_MASK |\
		NV40_VP_INST_SCA_WRITEMASK_MASK |\
		NV40_VP_INST_VEC_WRITEMASK_MASK |\
		NV40_VP_INST_SCA_DEST_TEMP_MASK |\
		NV40_VP_INST_DEST_MASK |\
		NV40_VP_INST_INDEX_CONST)

/* Useful to split the source selection regs into their pieces */
#define NV40_VP_SRC0_HIGH_SHIFT 9
#define NV40_VP_SRC0_HIGH_MASK  0x0001FE00
#define NV40_VP_SRC0_LOW_MASK   0x000001FF
#define NV40_VP_SRC2_HIGH_SHIFT 11
#define NV40_VP_SRC2_HIGH_MASK  0x0001F800
#define NV40_VP_SRC2_LOW_MASK   0x000007FF

/* Source selection - these are the bits you fill NV40_VP_INST_SRCn with */
#define NV40_VP_SRC_NEGATE               (1 << 16)
#define NV40_VP_SRC_SWZ_X_SHIFT          14
#define NV40_VP_SRC_SWZ_X_MASK           (3 << 14)
#define NV40_VP_SRC_SWZ_Y_SHIFT          12
#define NV40_VP_SRC_SWZ_Y_MASK           (3 << 12)
#define NV40_VP_SRC_SWZ_Z_SHIFT          10
#define NV40_VP_SRC_SWZ_Z_MASK           (3 << 10)
#define NV40_VP_SRC_SWZ_W_SHIFT          8
#define NV40_VP_SRC_SWZ_W_MASK           (3 << 8)
#define NV40_VP_SRC_SWZ_ALL_SHIFT        8
#define NV40_VP_SRC_SWZ_ALL_MASK         (0xFF << 8)
#    define NV40_VP_SWZ_CMP_X 0
#    define NV40_VP_SWZ_CMP_Y 1
#    define NV40_VP_SWZ_CMP_Z 2
#    define NV40_VP_SWZ_CMP_W 3
#define NV40_VP_SRC_TEMP_SRC_SHIFT       2
#define NV40_VP_SRC_TEMP_SRC_MASK        (0x3F << 2)
#define NV40_VP_SRC_REG_TYPE_SHIFT       0
#define NV40_VP_SRC_REG_TYPE_MASK        (3 << 0)
#    define NV40_VP_SRC_REG_TYPE_UNK0    0
#    define NV40_VP_SRC_REG_TYPE_TEMP    1
#    define NV40_VP_SRC_REG_TYPE_INPUT   2
#    define NV40_VP_SRC_REG_TYPE_CONST   3

#define NV40VP_MAKE_SWIZZLE(x, y, z, w) ((x<<6)|(y<<4)|(z<<2)|(w))
#define NV40VP_SWIZZLE(x, y, z, w) NV40VP_MAKE_SWIZZLE( \
		NV40_VP_SWZ_CMP_##x, \
		NV40_VP_SWZ_CMP_##y, \
		NV40_VP_SWZ_CMP_##z, \
		NV40_VP_SWZ_CMP_##w)

/* Useful macros for defining an instruction */
#define NV40_VP_DEST_TYPE_RESULT 	1
#define NV40_VP_DEST_TYPE_TEMP   	2
#define NV40_VP_DEST_MASK(x,y,z,w)  ((x<<3)|(y<<2)|(z<<1)|w)
#define NV40VP_LOCALS(subc) uint32_t vp_inst[4]; uint32_t vp_subc = subc
#define NV40VP_ARITH_INST_SET_DEFAULTS do { \
	vp_inst[0] = 0; vp_inst[1] = 0; vp_inst[2] = 0; vp_inst[3] = 0; \
	NV40VP_INST_S0(NOP, RESULT, 0, 0, 0, 0, 0); \
	NV40VP_INST_S1(NOP, RESULT, 0, 0, 0, 0, 0); \
	NV40VP_SET_COND(0, 0); \
	NV40VP_CONDITION(TR, X, Y, Z, W, 0); \
	NV40VP_SET_SOURCE_UNUSED(0); \
	NV40VP_SET_SOURCE_UNUSED(1); \
	NV40VP_SET_SOURCE_UNUSED(2); \
} while(0)
#define NV40VP_INST_EMIT do { \
	BEGIN_RING(vp_subc, NV30_TCL_PRIMITIVE_3D_VP_UPLOAD_INST0, 4); \
	OUT_RING  (vp_inst[0]); \
	OUT_RING  (vp_inst[1]); \
	OUT_RING  (vp_inst[2]); \
	OUT_RING  (vp_inst[3]); \
} while(0)
#define NV40VP_SET_LAST_INST vp_inst[3] |= NV40_VP_INST_PROGRAM_END;

/* Opcode / Destination reg construction */
#define NV40VP_SET_INST_S0(op, dt, dr, m) do { \
	vp_inst[1] &= ~NV40_VP_INST_VEC_OPCODE_MASK; \
	vp_inst[1] |= (op << NV40_VP_INST_VEC_OPCODE_SHIFT); \
	vp_inst[3] &= ~NV40_VP_INST_VEC_WRITEMASK_MASK; \
	vp_inst[3] |= (m << NV40_VP_INST_VEC_WRITEMASK_SHIFT); \
	switch(dt) { \
	case NV40_VP_DEST_TYPE_TEMP: \
		vp_inst[0] &= ~(NV40_VP_INST_VEC_DEST_TEMP_MASK|NV40_VP_INST_DEST_RESULT); \
		vp_inst[0] |= (dr << NV40_VP_INST_VEC_DEST_TEMP_SHIFT); \
		break; \
	case NV40_VP_DEST_TYPE_RESULT: \
		vp_inst[0] |= NV40_VP_INST_DEST_RESULT; \
		vp_inst[0] |= NV40_VP_INST_VEC_DEST_TEMP_MASK; \
		vp_inst[3] &= ~NV40_VP_INST_DEST_MASK; \
		vp_inst[3] |= (dr << NV40_VP_INST_DEST_SHIFT); \
		break; \
	default: break; \
	} \
} while(0)
#define NV40VP_SET_INST_S1(op, dt, dr, m) do { \
	vp_inst[1] &= ~NV40_VP_INST_SCA_OPCODE_MASK; \
	vp_inst[1] |= (op << NV40_VP_INST_SCA_OPCODE_SHIFT); \
	vp_inst[3] &= ~NV40_VP_INST_SCA_WRITEMASK_MASK; \
	vp_inst[3] |= (m << NV40_VP_INST_SCA_WRITEMASK_SHIFT); \
	switch(dt) { \
	case NV40_VP_DEST_TYPE_TEMP: \
		vp_inst[3] &= ~NV40_VP_INST_SCA_DEST_TEMP_MASK; \
		vp_inst[3] |= (dr << NV40_VP_INST_SCA_DEST_TEMP_SHIFT); \
		break; \
	case NV40_VP_DEST_TYPE_RESULT: \
		vp_inst[3] |= NV40_VP_INST_SCA_DEST_TEMP_MASK; \
		vp_inst[3] &= ~NV40_VP_INST_DEST_MASK; \
		vp_inst[3] |= (dr << NV40_VP_INST_DEST_SHIFT); \
		break; \
	default: break; \
	} \
} while(0)
#define NV40VP_INST_S0(op, dt, dr, mx, my, mz, mw) \
	NV40VP_SET_INST_S0(NV40_VP_INST_OP_##op, NV40_VP_DEST_TYPE_##dt, dr, NV40_VP_DEST_MASK(mx,my,mz,mw))
#define NV40VP_INST_S1(op, dt, dr, mx, my, mz, mw) \
	NV40VP_SET_INST_S1(NV40_VP_INST_OP_##op, NV40_VP_DEST_TYPE_##dt, dr, NV40_VP_DEST_MASK(mx,my,mz,mw))

/* Source register construction */
#define NV40VP_SET_SOURCE_0(src, abs) do { \
	vp_inst[0] = (vp_inst[0] & ~NV40_VP_INST_SRC0_ABS) | (abs ? NV40_VP_INST_SRC0_ABS : 0); \
	vp_inst[1] = (vp_inst[1] & ~NV40_VP_INST_SRC0H_MASK) \
		| ((((src) & NV40_VP_SRC0_HIGH_MASK) >> NV40_VP_SRC0_HIGH_SHIFT) \
				<< NV40_VP_INST_SRC0H_SHIFT); \
	vp_inst[2] = (vp_inst[2] & ~NV40_VP_INST_SRC0L_MASK) \
		| (((src) & NV40_VP_SRC0_LOW_MASK) << NV40_VP_INST_SRC0L_SHIFT); \
} while(0)
#define NV40VP_SET_SOURCE_1(src, abs) do { \
	vp_inst[0] = (vp_inst[0] & ~NV40_VP_INST_SRC1_ABS) | (abs ? NV40_VP_INST_SRC1_ABS : 0); \
	vp_inst[2] = (vp_inst[2] & ~NV40_VP_INST_SRC1_MASK) | ((src) << NV40_VP_INST_SRC1_SHIFT); \
} while(0)
#define NV40VP_SET_SOURCE_2(src, abs) do { \
	vp_inst[0] = (vp_inst[0] & ~NV40_VP_INST_SRC2_ABS) | (abs ? NV40_VP_INST_SRC2_ABS : 0); \
	vp_inst[2] = (vp_inst[2] & ~NV40_VP_INST_SRC2H_MASK) \
		| ((((src) & NV40_VP_SRC2_HIGH_MASK) >> NV40_VP_SRC2_HIGH_SHIFT) \
				<< NV40_VP_INST_SRC2H_SHIFT); \
	vp_inst[3] = (vp_inst[3] & ~NV40_VP_INST_SRC2L_MASK) \
		| (((src) & NV40_VP_SRC2_LOW_MASK) << NV40_VP_INST_SRC2L_SHIFT); \
} while(0)
#define NV40VP_SET_SOURCE_TEMP(pos, id, swz, neg, abs) NV40VP_SET_SOURCE_##pos( \
		(NV40_VP_SRC_REG_TYPE_TEMP<<NV40_VP_SRC_REG_TYPE_SHIFT) \
		| ((id) << NV40_VP_SRC_TEMP_SRC_SHIFT) \
		| ((swz) << NV40_VP_SRC_SWZ_ALL_SHIFT) \
		| ((neg) ? NV40_VP_SRC_NEGATE : 0), \
		(abs) \
		)
#define NV40VP_SET_SOURCE_CONST(pos, id, swz, neg, abs, in) do { \
	vp_inst[1] = (vp_inst[1] \
			& ~(NV40_VP_INST_CONST_SRC_MASK)) \
			| ((id) << NV40_VP_INST_CONST_SRC_SHIFT); \
	vp_inst[3] = (vp_inst[3] \
			& ~(NV40_VP_INST_INDEX_CONST)) \
			| ((in) ? NV40_VP_INST_INDEX_CONST : 0); \
	NV40VP_SET_SOURCE_##pos( \
			  (NV40_VP_SRC_REG_TYPE_CONST<<NV40_VP_SRC_REG_TYPE_SHIFT) \
			| ((swz) << NV40_VP_SRC_SWZ_ALL_SHIFT) \
			| ((neg) ? NV40_VP_SRC_NEGATE : 0), \
			(abs) \
		); \
} while(0)
#define NV40VP_SET_SOURCE_INPUT(pos, id, swz, neg, abs, in) do { \
	vp_inst[1] = (vp_inst[1] \
			& ~(	NV40_VP_INST_INPUT_SRC_MASK \
				|	NV40_VP_INST_INDEX_INPUT)) \
			| ((id) << NV40_VP_INST_INPUT_SRC_SHIFT) \
			| ((in) ? NV40_VP_INST_INDEX_INPUT : 0); \
	NV40VP_SET_SOURCE_##pos( \
			  (NV40_VP_SRC_REG_TYPE_INPUT<<NV40_VP_SRC_REG_TYPE_SHIFT) \
			| ((swz) << NV40_VP_SRC_SWZ_ALL_SHIFT) \
			| ((neg) ? NV40_VP_SRC_NEGATE : 0), \
			(abs) \
		); \
} while(0)
/* unused sources seem to be INPUT swz XYZW, don't know if this
 * actually matters or not...
 */
#define NV40VP_SET_SOURCE_UNUSED(pos) NV40VP_SET_SOURCE_##pos( \
			(NV40_VP_SRC_REG_TYPE_INPUT<<NV40_VP_SRC_REG_TYPE_SHIFT) \
			| (NV40VP_SWIZZLE(X,Y,Z,W) << NV40_VP_SRC_SWZ_ALL_SHIFT), \
			0 \
);

/* Conditional execution */
#define NV40VP_SET_COND(cr, update) vp_inst[0] = (vp_inst[0] \
		& ~(	NV40_VP_INST_COND_REG_SELECT_1 \
			|	NV40_VP_INST_COND_UPDATE_ENABLE)) \
		| ((cr) ? NV40_VP_INST_COND_REG_SELECT_1 : 0) \
		| ((update) ? NV40_VP_INST_COND_UPDATE_ENABLE : 0)
#define NV40VP_SET_CONDITION(op, swz, test) vp_inst[0] = (vp_inst[0] \
		& ~(	NV40_VP_INST_COND_MASK \
			|	NV40_VP_INST_COND_TEST_ENABLE \
			|	NV40_VP_INST_COND_SWZ_ALL_MASK)) \
		| ((op) << NV40_VP_INST_COND_SHIFT) \
		| ((swz) << NV40_VP_INST_COND_SWZ_ALL_SHIFT) \
		| ((test) ? NV40_VP_INST_COND_TEST_ENABLE : 0)
#define NV40VP_CONDITION(o, sx, sy, sz, sw, t) NV40VP_SET_CONDITION( \
		NV40_VP_INST_COND_##o, NV40VP_SWIZZLE(sx, sy, sz, sw), t)

#endif
