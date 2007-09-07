/*
 * Authors:  Vivien Chappelier <vivien.chappelier@free.fr>
 */

#ifndef _PS3_SPU_H_
#define _PS3_SPU_H_

struct _Ps3SpuRec_;
typedef struct _Ps3SpuRec_ Ps3SpuRec, *Ps3SpuPtr;

Ps3SpuPtr Ps3SpuInit(void);
int Ps3SpuSendCommand(Ps3SpuPtr fPtr, enum spu_command cmd,
		      void *argp, size_t len);
void Ps3SpuCleanup(Ps3SpuPtr fPtr);

#endif
