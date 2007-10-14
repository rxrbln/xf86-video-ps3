#ifndef __PS3_H__
#define __PS3_H__

struct _PS3Rec;
typedef struct _PS3Rec *PS3Ptr;
typedef struct _PS3Rec {
	PS3GpuPtr			gpu;
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
