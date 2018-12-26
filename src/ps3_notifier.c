
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "xf86.h"
#include "exa.h"
#include "xf86xv.h"
#include "ps3.h"
#include "ps3_dma.h"

#include <stdint.h>

#define PS3_NOTIFY_TIME_0                  0x00000000
#define PS3_NOTIFY_TIME_1                  0x00000004
#define PS3_NOTIFY_RETURN_VALUE            0x00000008
#define PS3_NOTIFY_STATE                   0x0000000C
#define PS3_NOTIFY_STATE_STATUS_MASK       0xFF000000
#define PS3_NOTIFY_STATE_STATUS_SHIFT              24
#define PS3_NOTIFY_STATE_STATUS_COMPLETED        0x00
#define PS3_NOTIFY_STATE_STATUS_IN_PROCESS       0x01
#define PS3_NOTIFY_STATE_ERROR_CODE_MASK   0x0000FFFF
#define PS3_NOTIFY_STATE_ERROR_CODE_SHIFT           0

void
PS3NotifierReset(PS3Ptr pPS3)
{
	volatile CARD32 *n = pPS3->dmaNotifier;

	n[PS3_NOTIFY_TIME_0      /4] =
	n[PS3_NOTIFY_TIME_1      /4] =
	n[PS3_NOTIFY_RETURN_VALUE/4] = 0;
	n[PS3_NOTIFY_STATE       /4] =
		(PS3_NOTIFY_STATE_STATUS_IN_PROCESS <<
					PS3_NOTIFY_STATE_STATUS_SHIFT);
}

CARD32
PS3NotifierStatus(PS3Ptr pPS3)
{
	volatile CARD32 *n = pPS3->dmaNotifier;

	return (n[PS3_NOTIFY_STATE/4]) >> PS3_NOTIFY_STATE_STATUS_SHIFT;
}

CARD32
PS3NotifierErrorCode(PS3Ptr pPS3)
{
	volatile CARD32 *n = pPS3->dmaNotifier;

	return (n[PS3_NOTIFY_STATE/4]) & PS3_NOTIFY_STATE_ERROR_CODE_MASK;
}

CARD32
PS3NotifierReturnVal(PS3Ptr pPS3)
{
	volatile CARD32 *n = pPS3->dmaNotifier;

	return n[PS3_NOTIFY_RETURN_VALUE/4];
}

Bool
PS3NotifierWaitStatus(PS3Ptr pPS3,
		      unsigned int status, unsigned int timeout)
{
	volatile CARD32 *n = pPS3->dmaNotifier;
	unsigned int t_start, time = 0;
	unsigned int val;

	t_start = GetTimeInMillis();
	while (time <= timeout) {
#if 0
		ErrorF("N/%d = 0x%08x/0x%08x/0x%08x/0x%08x\n",
		       time, n[0], n[1], n[2], n[3]);
#endif
		val = n[PS3_NOTIFY_STATE/4];

		if (val & PS3_NOTIFY_STATE_ERROR_CODE_MASK) {
			ErrorF("Notifier returned error: 0x%04x\n",
			       PS3NotifierErrorCode(pPS3));
			return FALSE;
		}

		if ((val >> PS3_NOTIFY_STATE_STATUS_SHIFT)
				== status)
			return TRUE;

		if (timeout)
			time = GetTimeInMillis() - t_start;
	}

	ErrorF("Notifier timeout!\n");
	return FALSE;
}

