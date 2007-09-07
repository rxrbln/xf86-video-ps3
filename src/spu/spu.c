#include <stdio.h>
#include <errno.h>

#include <spu_intrinsics.h>
#include <spu_mfcio.h> 

#include "spu.h"
#include "api.h"

#ifdef DEBUG
#define dbg(fmt, ...)	printf(fmt "\n", ## __VA_ARGS__);
#else
#define dbg(fmt, ...)	do {} while(0)
#endif

static inline void dma_get(void *ls, eaddr_t ram, size_t n)
{
	mfc_get(ls, ram, n, 0, 0, 0);
	mfc_write_tag_mask(1);
	mfc_read_tag_status_all();
}

static inline void dma_put(void *ls, eaddr_t ram, size_t n)
{
	mfc_put(ls, ram, n, 0, 0, 0);
	mfc_write_tag_mask(1);
	mfc_read_tag_status_all();
}

static inline void fill_dma_list(eaddr_t base, unsigned int *dma_list,
				 int w, int h, int p)
{
	int k;

	for (k = 0; k < h; k++) {
		dma_list[2 * k + 0] = w;
		dma_list[2 * k + 1] = (unsigned int) (base + p * k);
	}
}

static inline void dma_block_get(void *ls, const eaddr_t ram,
				 size_t w, size_t h, int p)
{
	unsigned int dma_list[2*h];

	fill_dma_list(ram, dma_list, w, h, p);
	mfc_getl(ls, (unsigned int) (ram >> 32),
		 dma_list, sizeof(dma_list), 0, 0, 0);
	mfc_write_tag_mask(1);
	mfc_read_tag_status_all();
}

static inline void dma_block_put(void *ls, const eaddr_t ram,
				 size_t w, size_t h, int p)
{
	unsigned int dma_list[2*h];

	fill_dma_list(ram, dma_list, w, h, p);
	mfc_putl(ls, (unsigned int) (ram >> 32),
		 dma_list, sizeof(dma_list), 0, 0, 0);
	mfc_write_tag_mask(1);
	mfc_read_tag_status_all();
}

static int spu_cmd_test(struct spu_args *ctxt)
{
	struct spu_cmd_test_args args;
	
	/* get command params through DMA */
	dma_get(&args, ctxt->io, sizeof(args));
	
	args.output = args.input1 + args.input2;

	/* put back result through DMA */
	dma_put(&args, ctxt->io, sizeof(args));
	
	return 0;
}

static void command_loop(struct spu_args *ctxt)
{
	enum spu_command cmd;
	int ret;

	while ((cmd = spu_read_in_mbox()) != SPU_CMD_EXIT) {

		dbg("processing command %d", cmd);

		ret = -ENOSYS;

		switch(cmd) {
		case SPU_CMD_TEST:
			ret = spu_cmd_test(ctxt);
			break;
		}

		spu_write_out_intr_mbox(ret);
	}

	spu_write_out_intr_mbox(0);
}

int main(eaddr_t spuid, eaddr_t argp, eaddr_t envp)
{
	struct spu_args ctxt;

	/* get the context through DMA */
	dma_get(&ctxt, argp, sizeof(struct spu_args));

	/* loop over commands */
	command_loop(&ctxt);

	return 0;
}

