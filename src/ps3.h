#ifndef __PS3_H__
#define __PS3_H__

#include "compat-api.h"

struct _PS3Rec;
typedef struct _PS3Rec *PS3Ptr;
typedef struct _PS3Rec {
	unsigned char*			fbstart;
	unsigned char*			fbmem;
	int				fboff;
	int				lineLength;
	CloseScreenProcPtr		CloseScreen;
	CreateScreenResourcesProcPtr	CreateScreenResources;
	EntityInfoPtr			pEnt;
	OptionInfoPtr			Options;
	ExaDriverPtr			EXADriverPtr;
	Bool                            NoAccel;

	int fd;
	long vram_base;
	CARD32 vram_size;
	long fifo_base;
	CARD32 fifo_size;
	CARD32 fifo_start;
	long ctrl_base;
	CARD32 ctrl_size;
	long iof_base;
	CARD32 iof_size;
	CARD32 iof_offset;
	long xv_base;
	CARD32 xv_size;
	XF86VideoAdaptorPtr             blitAdaptor;
	void				(*DMAKickoffCallback)(PS3Ptr pPS3);

	CARD32				*dmaBase;
	CARD32				dmaPut;
	CARD32				dmaCurrent;
	CARD32				dmaFree;
	CARD32				dmaMax;

	volatile CARD32			*FIFO;
	Bool				LockedUp;

} PS3Rec;

#define PS3PTR(p) ((PS3Ptr)((p)->driverPrivate))

#endif
