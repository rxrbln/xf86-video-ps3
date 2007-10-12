/*
 * Authors:  Vivien Chappelier <vivien.chappelier@free.fr>
 */

#ifndef _PS3_GPU_H_
#define _PS3_GPU_H_

struct _Ps3GpuRec_ {
	int fd;
	CARD32 vram_base;
	CARD32 vram_size;
	CARD32 fifo_base;
	CARD32 fifo_size;
	CARD32 ctrl_base;
	CARD32 ctrl_size;
};

typedef struct _Ps3GpuRec_ Ps3GpuRec, *Ps3GpuPtr;

Ps3GpuPtr Ps3GpuInit(void);
// TEMP
//int Ps3GpuSendCommand(Ps3GpuPtr fPtr, enum gpu_command cmd,
//		      void *argp, size_t len);
void Ps3GpuCleanup(Ps3GpuPtr fPtr);

#endif
