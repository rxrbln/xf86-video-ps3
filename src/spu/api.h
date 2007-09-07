#ifndef __API_H
#define __API_H

#include "spu.h"

enum spu_command {
	SPU_CMD_EXIT = 0,
	SPU_CMD_TEST = 1,
};

/* return input1 + input2 in output */
struct spu_cmd_test_args
{
	int input1; /* in  */
	int input2; /* in  */
	int output; /* out */
}  __spu_aligned;

union spu_cmd_args {
	struct spu_cmd_test_args test;
};

struct spu_args
{
	eaddr_t io; /* address for command data input/output */
	int fb_w;
	int fb_h;
	int fb_p;
	eaddr_t fb_pixels;
}  __spu_aligned;

#endif
