/*
 * Authors:  Vivien Chappelier <vivien.chappelier@free.fr>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

/* all driver need this */
#include "xf86.h"
#include "xf86_OSproc.h"

#include <string.h>
#include <libspe2.h>
#include <pthread.h>

#include "spu/api.h"
#include "spu_thread.h"
#include "ps3_spu.h"


/* generated from spu/spu_embed by embedspu, see Makefile.am for details */
extern void spu_embed();

/* -------------------------------------------------------------------- */
/* our private data, and two functions to allocate/free this            */

struct _Ps3SpuRec_ {
	struct spu_thread *spu_thread;
	struct spu_args spu_args;
	union spu_cmd_args io;
};

Ps3SpuPtr Ps3SpuInit(void)
{
	Ps3SpuPtr fPtr;
	struct spu_thread *spu_thread;
	struct spu_args spu_args;

	fPtr = xnfcalloc(sizeof(Ps3SpuRec), 1);

	/* fill in SPU context */
	fPtr->spu_args.io = EADDR(&fPtr->io);
// TODO	fPtr->spu_args.fb_w = 128;
// TODO	fPtr->spu_args.fb_h = 128;
// TODO	fPtr->spu_args.fb_p = 128;
// TODO	fPtr->spu_args.fb_pixels = 128;

	spu_thread_create(&fPtr->spu_thread, spu_embed, &fPtr->spu_args);

	return fPtr;
}

int Ps3SpuSendCommand(Ps3SpuPtr fPtr, enum spu_command cmd,
		      void *argp, size_t len)
{
	int ret;

	memcpy(&fPtr->io, argp, len);
	
	if(spu_thread_mbox_send(fPtr->spu_thread, cmd) < 0) {
		ErrorF("Failed writing command to SPU");
		return -EIO;
	}

	spu_thread_wait();

	if (spu_thread_mbox_recv(fPtr->spu_thread, &ret) <= 0) {
		die("Failed reading return value from SPU");
		return -EIO;
	}

	memcpy(argp, &fPtr->io, len);

	return ret;
}

void Ps3SpuCleanup(Ps3SpuPtr fPtr)
{
	if (fPtr == NULL)
		return;

	spu_thread_join(fPtr->spu_thread);

	xfree(fPtr);
}
