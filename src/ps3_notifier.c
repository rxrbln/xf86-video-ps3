#include "nv_include.h"

#define NV_NOTIFIER_SIZE                                                      32
#define NV_NOTIFY_TIME_0                                              0x00000000
#define NV_NOTIFY_TIME_1                                              0x00000004
#define NV_NOTIFY_RETURN_VALUE                                        0x00000008
#define NV_NOTIFY_STATE                                               0x0000000C
#define NV_NOTIFY_STATE_STATUS_MASK                                   0xFF000000
#define NV_NOTIFY_STATE_STATUS_SHIFT                                          24
#define NV_NOTIFY_STATE_STATUS_COMPLETED                                    0x00
#define NV_NOTIFY_STATE_STATUS_IN_PROCESS                                   0x01
#define NV_NOTIFY_STATE_ERROR_CODE_MASK                               0x0000FFFF
#define NV_NOTIFY_STATE_ERROR_CODE_SHIFT                                       0

#define NOTIFIER(__v) \
	NVPtr pNv = NVPTR(pScrn); \
	volatile uint32_t *__v = (void*)pNv->NotifierBlock + notifier->offset

struct drm_nouveau_notifierobj_alloc *
NVNotifierAlloc(ScrnInfoPtr pScrn, uint32_t handle)
{
	NVPtr pNv = NVPTR(pScrn);
	struct drm_nouveau_notifierobj_alloc *notifier;
	int ret;

	notifier = xcalloc(1, sizeof(*notifier));
	if (!notifier) {
		NVNotifierDestroy(pScrn, notifier);
		return NULL;
	}

	notifier->channel = pNv->fifo.channel;
	notifier->handle  = handle;
	notifier->count   = 1;
	ret = drmCommandWriteRead(pNv->drm_fd, DRM_NOUVEAU_NOTIFIEROBJ_ALLOC,
				  notifier, sizeof(*notifier));
	if (ret) {
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
			   "Failed to create notifier 0x%08x: %d\n",
			   handle, ret);
		NVNotifierDestroy(pScrn, notifier);
		return NULL;
	}

	return notifier;
}

void
NVNotifierDestroy(ScrnInfoPtr pScrn,
		  struct drm_nouveau_notifierobj_alloc *notifier)
{
	if (notifier) {
		/*XXX: destroy notifier object */
		xfree(notifier);
	}
}

void
NVNotifierReset(ScrnInfoPtr pScrn,
		struct drm_nouveau_notifierobj_alloc *notifier)
{
	NOTIFIER(n);

	n[NV_NOTIFY_TIME_0      /4] =
	n[NV_NOTIFY_TIME_1      /4] =
	n[NV_NOTIFY_RETURN_VALUE/4] = 0;
	n[NV_NOTIFY_STATE       /4] = (NV_NOTIFY_STATE_STATUS_IN_PROCESS <<
				       NV_NOTIFY_STATE_STATUS_SHIFT);
}

uint32_t
NVNotifierStatus(ScrnInfoPtr pScrn,
		 struct drm_nouveau_notifierobj_alloc *notifier)
{
	NOTIFIER(n);

	return n[NV_NOTIFY_STATE/4] >> NV_NOTIFY_STATE_STATUS_SHIFT;
}

uint32_t
NVNotifierErrorCode(ScrnInfoPtr pScrn,
		    struct drm_nouveau_notifierobj_alloc *notifier)
{
	NOTIFIER(n);

	return n[NV_NOTIFY_STATE/4] & NV_NOTIFY_STATE_ERROR_CODE_MASK;
}

uint32_t
NVNotifierReturnVal(ScrnInfoPtr pScrn,
		    struct drm_nouveau_notifierobj_alloc *notifier)
{
	NOTIFIER(n);

	return n[NV_NOTIFY_RETURN_VALUE/4];
}

Bool
NVNotifierWaitStatus(ScrnInfoPtr pScrn,
		     struct drm_nouveau_notifierobj_alloc *notifier,
		     unsigned int status, unsigned int timeout)
{
	NOTIFIER(n);
	unsigned int t_start, time = 0;

	t_start = GetTimeInMillis();
	while (time <= timeout) {
#if 0
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
			   "N(0x%08x)/%d = 0x%08x/0x%08x/0x%08x/0x%08x\n",
			   notifier->handle, time, n[0], n[1], n[2], n[3]);
#endif
		if (n[NV_NOTIFY_STATE/4] & NV_NOTIFY_STATE_ERROR_CODE_MASK) {
			xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
				   "Notifier returned error: 0x%04x\n",
				   NVNotifierErrorCode(pScrn, notifier));
			return FALSE;
		}

		if ((n[NV_NOTIFY_STATE/4] >> NV_NOTIFY_STATE_STATUS_SHIFT)
				== status)
			return TRUE;

		if (timeout)
			time = GetTimeInMillis() - t_start;
	}

	xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		   "Notifier (0x%08x) timeout!\n", notifier->handle);
	return FALSE;
}

