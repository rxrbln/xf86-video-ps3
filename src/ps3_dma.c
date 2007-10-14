#include "xf86.h"
#include "exa.h"

#include <errno.h>
#include "ps3_gpu.h"

#include "ps3.h"
#include "ps3_dma.h"

static void PS3DumpLockupInfo(PS3Ptr pPS3);

void PS3DmaKickoff(PS3Ptr pPS3)
{
	if(pPS3->dmaCurrent != pPS3->dmaPut) {
		pPS3->dmaPut = pPS3->dmaCurrent;
		WRITE_PUT(pPS3,  pPS3->dmaPut);
	}
}

void PS3DmaKickoffCallback(PS3Ptr pPS3)
{
	PS3DmaKickoff(pPS3);
	pPS3->DMAKickoffCallback = NULL;
}

/* There is a HW race condition with videoram command buffers.
 * You can't jump to the location of your put offset.  We write put
 * at the jump offset + SKIPS dwords with noop padding in between
 * to solve this problem
 */
#define SKIPS  8

void PS3DmaWait (ScrnInfoPtr pScrn, int size)
{
	PS3Ptr pPS3 = PS3PTR(pScrn);
	int t_start;
	int dmaGet;

	ErrorF("%s\n", __FUNCTION__);

	size++;

	t_start = GetTimeInMillis();
	while(pPS3->dmaFree < size) {
		dmaGet = READ_GET(pPS3);

		if(pPS3->dmaPut >= dmaGet) {
			pPS3->dmaFree = pPS3->dmaMax - pPS3->dmaCurrent;
			if(pPS3->dmaFree < size) {
				PS3DmaNext(pPS3, (0x20000000|pPS3->gpu->fifo_start));
				if(dmaGet <= SKIPS) {
					if(pPS3->dmaPut <= SKIPS) /* corner case - will be idle */
						WRITE_PUT(pPS3, SKIPS + 1);
					do {
						if (GetTimeInMillis() - t_start > 2000)
							PS3Sync(pScrn);
						dmaGet = READ_GET(pPS3);
					} while(dmaGet <= SKIPS);
				}
				WRITE_PUT(pPS3, SKIPS);

				pPS3->dmaCurrent = pPS3->dmaPut = SKIPS;
				pPS3->dmaFree = dmaGet - (SKIPS + 1);
			}
		} else
			pPS3->dmaFree = dmaGet - pPS3->dmaCurrent - 1;

		if (GetTimeInMillis() - t_start > 2000)
			PS3Sync(pScrn);
	}
}

static void PS3DumpLockupInfo(PS3Ptr pPS3)
{
	int i,start;
	start=READ_GET(pPS3)-20;
	if (start<0) start=0;
	xf86DrvMsg(0, X_INFO, "Fifo dump (lockup 0x%04x,0x%04x):\n",READ_GET(pPS3),pPS3->dmaPut);
	for(i=start;i<pPS3->dmaPut+10;i++)
		xf86DrvMsg(0, X_INFO, "[0x%04x] 0x%08x\n", i, pPS3->dmaBase[i]);
	xf86DrvMsg(0, X_INFO, "End of fifo dump\n");
}

static void
PS3LockedUp(ScrnInfoPtr pScrn)
{
	PS3Ptr pPS3 = PS3PTR(pScrn);

	/* avoid re-entering FatalError on shutdown */
	if (pPS3->LockedUp)
		return;
	pPS3->LockedUp = TRUE;

	PS3DumpLockupInfo(pPS3);

	FatalError("DMA queue hang: dmaPut=%x, put=%x get=%x\n",
		   pPS3->dmaPut, READ_PUT(pPS3), READ_GET(pPS3));
}

void PS3Sync(ScrnInfoPtr pScrn)
{
	PS3Ptr pPS3 = PS3PTR(pScrn);
	int t_start, timeout = 2000;

	ErrorF("%s\n", __FUNCTION__);

	if(pPS3->NoAccel)
		return;

	if(pPS3->DMAKickoffCallback)
		(*pPS3->DMAKickoffCallback)(pPS3);

	/* Wait for entire FIFO to be processed */
	t_start = GetTimeInMillis();
	while((GetTimeInMillis() - t_start) < timeout &&
			(READ_GET(pPS3) != pPS3->dmaPut));
	if ((GetTimeInMillis() - t_start) >= timeout) {
		PS3LockedUp(pScrn);
		return;
	}
}

void PS3DmaStart(PS3Ptr pPS3, CARD32 subchannel, CARD32 tag, int size)
{
	int i;
	
	PS3DEBUG("PS3DmaStart: subc=%d, cmd=%x, num=%d\n", (subchannel), (tag), (size));

        /* XXX FIXME */
        ScrnInfoPtr pScrn = xf86Screens[0];

        /* add 2 for the potential subchannel binding */
        if((pPS3)->dmaFree <= (size + 2))
                PS3DmaWait(pScrn, size + 2);

	PS3DmaNext(pPS3, ((size) << 18) | ((subchannel) << 13) | (tag));
	pPS3->dmaFree -= ((size) + 1); 
}

void PS3ResetGraphics(ScrnInfoPtr pScrn)
{
	PS3Ptr pPS3 = PS3PTR(pScrn);
	int i;

	ErrorF("%s\n", __FUNCTION__);

	pPS3->dmaPut = pPS3->dmaCurrent = READ_GET(pPS3);
	pPS3->dmaMax = (pPS3->gpu->fifo_size >> 2) - 2;
	pPS3->dmaFree = pPS3->dmaMax - pPS3->dmaCurrent;

	/* assert there's enough room for the skips */
	if(pPS3->dmaFree <= SKIPS)
		PS3DmaWait(pScrn, SKIPS); 
	for (i=0; i<SKIPS; i++) {
		PS3DmaNext(pPS3,0);
		pPS3->dmaBase[i]=0;
	}
	pPS3->dmaFree -= SKIPS;
}

Bool PS3InitDma(ScrnInfoPtr pScrn)
{
	PS3Ptr pPS3 = PS3PTR(pScrn);
	int i, ret;

// TEMP
	ErrorF("NoAccel = %d pPS3=%p pScreen=%p\n", 
	       pPS3->NoAccel, pPS3, pScrn);

	if (pPS3->NoAccel)
		return TRUE;

	ErrorF("%s:%d\n", __FUNCTION__, __LINE__);

	pPS3->dmaBase = (CARD32 *) pPS3->gpu->fifo_base;
	pPS3->FIFO = (volatile CARD32 *) pPS3->gpu->ctrl_base;

	xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		   "  Control registers : %p\n", pPS3->FIFO);
	xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		   "  DMA command buffer: %p\n", pPS3->dmaBase);
	xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		   "  DMA cmdbuf length : %d KiB\n",
		   pPS3->gpu->fifo_size / 1024);
	xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		   "  DMA base PUT      : 0x%08x\n", pPS3->gpu->fifo_start);

	pPS3->dmaPut = pPS3->dmaCurrent = READ_GET(pPS3);
	pPS3->dmaMax = (pPS3->gpu->fifo_size >> 2) - 2;
	pPS3->dmaFree = pPS3->dmaMax - pPS3->dmaCurrent;

	for (i = 0; i < SKIPS; i++) {
		pPS3->dmaBase[i] = 0;
		PS3DmaNext(pPS3, 0);
	}
	pPS3->dmaFree -= SKIPS;

	ErrorF("%s:%d\n", __FUNCTION__, __LINE__);

	return TRUE;
}

