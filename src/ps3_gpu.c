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

#define DEV_VFB		"/dev/fb0"
#define DEV_GPU_VRAM	"/dev/ps3gpu_vram"
#define DEV_GPU_FIFO	"/dev/ps3gpu_fifo"
#define DEV_GPU_CTRL	"/dev/ps3gpu_ctrl"

/* -------------------------------------------------------------------- */
/* our private data, and two functions to allocate/free this            */

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
	
// TEMP
	ErrorF("vram %d fifo %d ctrl %d\n",
	       info.vram_size, info.fifo_size, info.ctrl_size);

	pPS3->vram_size = info.vram_size;
	pPS3->fifo_size = info.fifo_size;
	pPS3->ctrl_size = info.ctrl_size;

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

// TEMP
	printf("mmap: %s len %d\n", name, len);

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
        struct fb_fix_screeninfo fix;
	int ret = 0;
	int fd;
	int val = 0;

	if ((fd = open(DEV_VFB, O_RDWR)) < 0) {
		ErrorF("open: %s", strerror(errno));
		return -1;
	}

        /* get framebuffer size */
        if ((ret = ioctl(fd, FBIOGET_FSCREENINFO, &fix)) < 0) {
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

	pPS3->iof_base = (long) pPS3->fbmem;
	pPS3->iof_size = fix.smem_len;
	pPS3->iof_offset = 0x0d000000; /* GPUIOF */
	
	pPS3->fbmem = (unsigned char *) pPS3->vram_base;
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
