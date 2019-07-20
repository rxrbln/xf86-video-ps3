/*
 * Authors:  Vivien Chappelier <vivien.chappelier@free.fr>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

/* all driver need this */
#include "xf86.h"
#include "xf86_OSproc.h"

#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <errno.h>

#include <linux/types.h>
#include <linux/fb.h>
#include <asm/ps3fb.h>

#include "exa.h"
#include "xf86xv.h"
#include "ps3.h"
#include "ps3_gpu.h"
#include "ps3_dma.h"
#include "nouveau_class.h"
#include "nv_shaders.h"

#define DEV_VFB		"/dev/fb0"
#define DEV_GPU_VRAM	"/dev/ps3gpu_vram"
#define DEV_GPU_FIFO	"/dev/ps3gpu_fifo"
#define DEV_GPU_CTRL	"/dev/ps3gpu_ctrl"

/* -------------------------------------------------------------------- */
/* our private data, and two functions to allocate/free this            */

/* supported modes */
#define MODE_MASK          (0x1f)
#define MODE_RGB         (1 << 5)
#define MODE_FULLSCREEN  (1 << 7)
#define MODE_DITHERED   (1 << 11)

#define MODE_COUNT 14

struct fb_mode_info {
	CARD32 width;
	CARD32 height;

	CARD32 virtual_x;
	CARD32 virtual_y;
};

/*
  YUV 60Hz  1:480i  2:480p  3:720p  4:1080i  5:1080p
  YUV 50Hz  6:576i  7:576p  8:720p  9:1080i 10:1080p
  VESA     11:WXGA 12:SXGA 13:WUXGA
*/
static const struct fb_mode_info fb_modes[MODE_COUNT] = {
	{    0,    0,    0,    0 },
	{  576,  384,  720,  480 },
	{  576,  384,  720,  480 },
	{ 1124,  644, 1280,  720 },
	{ 1688,  964, 1920, 1080 },
	{ 1688,  964, 1920, 1080 },
	{  576,  460,  720,  576 },
	{  576,  460,  720,  576 },
	{ 1124,  644, 1280,  720 },
	{ 1688,  964, 1920, 1080 },
	{ 1688,  964, 1920, 1080 },
	{ 1280,  768, 1280,  768 },
	{ 1280, 1024, 1280, 1024 },
	{ 1920, 1200, 1920, 1200 },
};

static int gpu_get_info(PS3Ptr pPS3)
{
	struct ps3fb_ioctl_gpu_info info;
	int ret = -1;
	int fd;

	if ((fd = open(DEV_VFB, O_RDWR)) < 0) {
		ErrorF("open: %s", strerror(errno));
		return -1;
	}

	if ((ret = ioctl(fd, PS3FB_IOCTL_GPU_INFO, &info)) < 0) {
		ErrorF("ioctl: %s", strerror(errno));
		goto out;
	}
	
	pPS3->vram_size = info.vram_size;
	pPS3->fifo_size = info.fifo_size;
	pPS3->ctrl_size = info.ctrl_size;

	/* steal some memory for the cursor */
	pPS3->vram_size -= (32 * 1024);
	pPS3->cursor_start = pPS3->vram_size;

	/* GPU hangs if all space is used */
	pPS3->fifo_size -= 1024;

	ret = 0;
out:
	close(fd);

	return ret;
}

static void *map_resource(char const *name, int len)
{
	void *virt;
	int fd;

	if ((fd = open(name, O_RDWR)) < 0) {
		ErrorF("open: %s", strerror(errno));
		return NULL;
	}

	virt = mmap(0, len, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

	if (virt == MAP_FAILED)
		return NULL;

	close(fd);

	return virt;
}

static void unmap_resource(void *base, int len)
{
	munmap(base, len);
}

static int enter_direct_mode(PS3Ptr pPS3)
{
	struct fb_mode_info const *m;
        struct fb_fix_screeninfo fix;
        struct fb_var_screeninfo var;
	int ret = 0;
	int fd;
	int val = 0;
	int mode = 0;

	if ((fd = open(DEV_VFB, O_RDWR)) < 0) {
		ErrorF("open: %s", strerror(errno));
		return -1;
	}

        /* get video mode */
	if ((ret = ioctl(fd, PS3FB_IOCTL_GETMODE, &mode)) < 0) {
		perror("ioctl");
		goto out;
	}

	if ((mode & MODE_MASK) >= MODE_COUNT) {
		ErrorF("unsupported mode %d\n", mode);
		goto out;
	}
	m = &fb_modes[mode & MODE_MASK];

        /* get framebuffer size */
        if ((ret = ioctl(fd, FBIOGET_FSCREENINFO, &fix)) < 0) {
                perror("ioctl");
                goto out;
        }

	/* get display size */
	if ((ret == ioctl(fd, FBIOGET_VSCREENINFO, &var)) < 0) {
		perror("ioctl");
		goto out;
	}

	/* stop that incessant blitting! */
	if ((ret = ioctl(fd, PS3FB_IOCTL_ON, 0)) < 0) {
		ErrorF("ioctl: %s", strerror(errno));
		goto out;
	}

	/* wait for vsync */
	if ((ret = ioctl(fd, FBIO_WAITFORVSYNC, &val)) < 0) {
		ErrorF("ioctl: %s", strerror(errno));
		goto out;
	}

	/* setup FIFO */
	if ((ret = ioctl(fd, PS3FB_IOCTL_GPU_SETUP, &val)) < 0) {
		ErrorF("ioctl: %s", strerror(errno));
		goto out;
	}

	pPS3->fboff = ((m->virtual_y - var.yres_virtual) / 2 * m->virtual_x +
		       (m->virtual_x - var.xres_virtual) / 2) * 4;
	pPS3->fboff = (pPS3->fboff + 63) & ~63;

//	ErrorF("%dx%d mode %d off %d\n", var.xres_virtual, var.yres_virtual,
//	       mode, pPS3->fboff);

	pPS3->iof_base = (long) pPS3->fbmem;
	pPS3->iof_size = fix.smem_len;
	pPS3->iof_offset = 0x0d000000; /* GPUIOF */

	pPS3->fbmem = (unsigned char *) pPS3->vram_base;
	pPS3->fbstart = pPS3->fbmem + pPS3->fboff;
	pPS3->lineLength = m->virtual_x * 4;

	memset(pPS3->fbmem, 0, m->virtual_y * pPS3->lineLength);
	
	/* reserve mem for xv */
	pPS3->xv_size = 1920 * 1080 * 2;
	pPS3->xv_base = pPS3->iof_base + pPS3->iof_size - pPS3->xv_size;

	/* keep fd open */
	pPS3->fd = fd;
	return 0;

out:
	close(fd);

	return ret;
}

static int leave_direct_mode(PS3Ptr pPS3)
{
	int ret = 0;
	int fd;

	fd = pPS3->fd;

	if ((ret = ioctl(fd, PS3FB_IOCTL_OFF, 0)) < 0) {
		ErrorF("ioctl: %s", strerror(errno));
		goto out;
	}

out:
	close(fd);

	return ret;
}

static void clear_vram(PS3Ptr pPS3)
{
	memset((void *) pPS3->vram_base, 0xff, pPS3->vram_size);
}

int PS3GpuInit(PS3Ptr pPS3)
{
	/* fill in GPU context */
	gpu_get_info(pPS3);

	if ((pPS3->vram_base = (long)
	     map_resource(DEV_GPU_VRAM, pPS3->vram_size)) == 0) {
		ErrorF("failed to map vram\n");
		goto err_free;
	}

	if ((pPS3->fifo_base = (long)
	     map_resource(DEV_GPU_FIFO, pPS3->fifo_size)) == 0) {
		ErrorF("failed to map fifo\n");
		goto err_unmap_vram;
	}

	if ((pPS3->ctrl_base = (long)
	     map_resource(DEV_GPU_CTRL, pPS3->ctrl_size)) == 0) {
		ErrorF("failed to map ctrl\n");
		goto err_unmap_fifo;
	}

	pPS3->CURSOR = (volatile CARD32 *)((char*)pPS3->vram_base + pPS3->cursor_start);

	/* determine the start of the FIFO from GPU point of view */
	pPS3->fifo_start = ((CARD32 *) pPS3->ctrl_base)[0x10] &
		~(pPS3->fifo_size - 1);

	enter_direct_mode(pPS3);

	return 0;

err_unmap_fifo:
	unmap_resource((void *) pPS3->fifo_base, pPS3->fifo_size);
err_unmap_vram:
	unmap_resource((void *) pPS3->ctrl_base, pPS3->ctrl_size);
err_free:
	free(pPS3);

	return -1;
}

void PS3GpuCleanup(PS3Ptr pPS3)
{
	leave_direct_mode(pPS3);

	unmap_resource((void *) pPS3->ctrl_base, pPS3->ctrl_size);
	unmap_resource((void *) pPS3->fifo_base, pPS3->fifo_size);
	unmap_resource((void *) pPS3->vram_base, pPS3->vram_size);

	free(pPS3);
}
