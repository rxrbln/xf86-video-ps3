
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "xf86.h"
#include "exa.h"
#include "xf86xv.h"
#include <errno.h>

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
 * You can't jump to the location of your put offset.
 */

void PS3DmaWait (ScrnInfoPtr pScrn, int size)
{
	PS3Ptr pPS3 = PS3PTR(pScrn);
	int t_start = GetTimeInMillis();
	int dmaGet = GetTimeInMillis();

	size += 16; /* vast reserve for JMP to front */
	while(pPS3->dmaFree < size) {
		ErrorF("%s %x\n", __FUNCTION__, dmaGet);
	        ErrorF("%s fifo wrap JMP to front\n", __FUNCTION__);

		PS3DmaNext(pPS3, (0x20000000|pPS3->fifo_start));
		pPS3->dmaFree -= 1;
		WRITE_PUT(pPS3, 0);
		pPS3->dmaCurrent = pPS3->dmaPut = 0;

		PS3Sync(pScrn); /* this does loop, does it? ReneR */

		pPS3->dmaFree = pPS3->dmaMax;
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

//	ErrorF("%s\n", __FUNCTION__);

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
	pPS3->dmaMax = (pPS3->fifo_size >> 2) - 2;
	pPS3->dmaFree = pPS3->dmaMax - pPS3->dmaCurrent;
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

	pPS3->dmaBase = (CARD32 *) pPS3->fifo_base;
	pPS3->FIFO = (volatile CARD32 *) pPS3->ctrl_base;

	xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		   "  Control registers : %p\n", pPS3->FIFO);
	xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		   "  DMA command buffer: %p\n", pPS3->dmaBase);
	xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		   "  DMA cmdbuf length : %d KiB\n",
		   pPS3->fifo_size / 1024);
	xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		   "  DMA base PUT      : 0x%08x\n", pPS3->fifo_start);

	pPS3->dmaPut = pPS3->dmaCurrent = READ_GET(pPS3);
	pPS3->dmaMax = (pPS3->fifo_size >> 2) - 2;
	pPS3->dmaFree = pPS3->dmaMax - pPS3->dmaCurrent;

	ErrorF("FIFO: GET: %x, PUT: %x\n", READ_GET(pPS3), READ_PUT(pPS3));

	for (i = 0; i < 4; i++) {
		PS3DmaNext(pPS3, 0); // test NOP
	}
	PS3DmaKickoff(pPS3); /* ReneR: dma test early */

	PS3Sync(pScrn);
	ErrorF("%s:%d\n", __FUNCTION__, __LINE__);

	return TRUE;
}
