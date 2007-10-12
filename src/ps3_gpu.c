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

#include "ps3_gpu.h"

#define DEV_VFB		"/dev/fb0"
#define DEV_GPU_VRAM	"/dev/ps3gpu_vram"
#define DEV_GPU_FIFO	"/dev/ps3gpu_fifo"
#define DEV_GPU_CTRL	"/dev/ps3gpu_ctrl"

/* -------------------------------------------------------------------- */
/* our private data, and two functions to allocate/free this            */

static int gpu_get_info(Ps3GpuPtr fPtr)
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

	fPtr->vram_size = info.vram_size;
	fPtr->fifo_size = info.fifo_size;
	fPtr->ctrl_size = info.ctrl_size;

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

static int enter_direct_mode(Ps3GpuPtr fPtr)
{
	int ret = 0;
	int fd;
	int val = 0;

	if ((fd = open("/dev/fb0", O_RDWR)) < 0) {
		ErrorF("open: %s", strerror(errno));
		return -1;
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

	/* keep fd open */
	fPtr->fd = fd;
	return 0;

out:
	close(fd);

	return ret;
}

static int leave_direct_mode(Ps3GpuPtr fPtr)
{
	int ret = 0;
	int fd;

	fd = fPtr->fd;

	if ((ret = ioctl(fd, PS3FB_IOCTL_OFF, 0)) < 0) {
		ErrorF("ioctl: %s", strerror(errno));
		goto out;
	}

out:
	close(fd);

	return ret;
}

static void clear_vram(Ps3GpuPtr fPtr)
{
	memset((void *) fPtr->vram_base, 0xff, fPtr->vram_size);
}

Ps3GpuPtr Ps3GpuInit(void)
{
	Ps3GpuPtr fPtr;

	fPtr = xnfcalloc(sizeof(Ps3GpuRec), 1);

	/* fill in GPU context */
	gpu_get_info(fPtr);

	if ((fPtr->vram_base = (CARD32)
	     map_resource(DEV_GPU_VRAM, fPtr->vram_size)) == 0) {
		ErrorF("failed to map vram\n");
		goto err_free;
	}

	if ((fPtr->fifo_base = (CARD32)
	     map_resource(DEV_GPU_FIFO, fPtr->fifo_size)) == 0) {
		ErrorF("failed to map fifo\n");
		goto err_unmap_vram;
	}

	if ((fPtr->ctrl_base = (CARD32)
	     map_resource(DEV_GPU_CTRL, fPtr->ctrl_size)) == 0) {
		ErrorF("failed to map ctrl\n");
		goto err_unmap_fifo;
	}

	enter_direct_mode(fPtr);

// TEMP
	clear_vram(fPtr);

	return fPtr;

err_unmap_fifo:
	unmap_resource((void *) fPtr->fifo_base, fPtr->fifo_size);
err_unmap_vram:
	unmap_resource((void *) fPtr->ctrl_base, fPtr->ctrl_size);
err_free:
	xfree(fPtr);

	return NULL;
}

// TEMP
#if 0
int Ps3GpuSendCommand(Ps3GpuPtr fPtr, enum gpu_command cmd,
		      void *argp, size_t len)
{
	int ret;

// TEMP
	ErrorF("1\n");

	memcpy(&fPtr->io, argp, len);
	// TEMP
	ErrorF("2\n");
	if(gpu_thread_mbox_send(fPtr->gpu_thread, cmd) < 0) {
		ErrorF("Failed writing command to GPU\n");
		return -EIO;
	}
// TEMP
	ErrorF("3\n");
//	gpu_thread_wait();
// TEMP
	ErrorF("4\n");
	if (gpu_thread_mbox_recv(fPtr->gpu_thread, &ret) <= 0) {
		ErrorF("Failed reading return value from GPU\n");
		return -EIO;
	}
// TEMP
	ErrorF("5\n");
	memcpy(argp, &fPtr->io, len);
// TEMP
	ErrorF("6\n");
	return ret;
}
#endif

void Ps3GpuCleanup(Ps3GpuPtr fPtr)
{
	if (fPtr == NULL)
		return;

	leave_direct_mode(fPtr);

	unmap_resource((void *) fPtr->ctrl_base, fPtr->ctrl_size);
	unmap_resource((void *) fPtr->fifo_base, fPtr->fifo_size);
	unmap_resource((void *) fPtr->vram_base, fPtr->vram_size);

	xfree(fPtr);
}
