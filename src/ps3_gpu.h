/*
 * Authors:  Vivien Chappelier <vivien.chappelier@free.fr>
 */

#ifndef _PS3_GPU_H_
#define _PS3_GPU_H_

struct _PS3GpuRec_ {
	int fd;
	CARD32 vram_base;
	CARD32 vram_size;
	CARD32 fifo_base;
	CARD32 fifo_size;
	CARD32 ctrl_base;
	CARD32 ctrl_size;
	CARD32 fifo_start;
};

typedef struct _PS3GpuRec_ PS3GpuRec, *PS3GpuPtr;

PS3GpuPtr PS3GpuInit(void);
// TEMP
//int PS3GpuSendCommand(PS3GpuPtr fPtr, enum gpu_command cmd,
//		      void *argp, size_t len);
void PS3GpuCleanup(PS3GpuPtr fPtr);

#endif
