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

static int gpu_get_info(PS3GpuPtr pPS3)
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

static int enter_direct_mode(PS3GpuPtr pPS3)
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
	pPS3->fd = fd;
	return 0;

out:
	close(fd);

	return ret;
}

static int leave_direct_mode(PS3GpuPtr pPS3)
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

static void clear_vram(PS3GpuPtr pPS3)
{
	memset((void *) pPS3->vram_base, 0xff, pPS3->vram_size);
}

PS3GpuPtr PS3GpuInit(void)
{
	PS3GpuPtr pPS3;

	pPS3 = xnfcalloc(sizeof(PS3GpuRec), 1);

	/* fill in GPU context */
	gpu_get_info(pPS3);

	if ((pPS3->vram_base = (CARD32)
	     map_resource(DEV_GPU_VRAM, pPS3->vram_size)) == 0) {
		ErrorF("failed to map vram\n");
		goto err_free;
	}

	if ((pPS3->fifo_base = (CARD32)
	     map_resource(DEV_GPU_FIFO, pPS3->fifo_size)) == 0) {
		ErrorF("failed to map fifo\n");
		goto err_unmap_vram;
	}

	if ((pPS3->ctrl_base = (CARD32)
	     map_resource(DEV_GPU_CTRL, pPS3->ctrl_size)) == 0) {
		ErrorF("failed to map ctrl\n");
		goto err_unmap_fifo;
	}

	/* determine the start of the FIFO from GPU point of view */
	pPS3->fifo_start = ((CARD32 *) pPS3->ctrl_base)[0x10] &
		~(pPS3->fifo_size - 1);

	enter_direct_mode(pPS3);

	return pPS3;

err_unmap_fifo:
	unmap_resource((void *) pPS3->fifo_base, pPS3->fifo_size);
err_unmap_vram:
	unmap_resource((void *) pPS3->ctrl_base, pPS3->ctrl_size);
err_free:
	xfree(pPS3);

	return NULL;
}

// TEMP
#if 0
int PS3GpuSendCommand(PS3GpuPtr pPS3, enum gpu_command cmd,
		      void *argp, size_t len)
{
	int ret;

// TEMP
	ErrorF("1\n");

	memcpy(&pPS3->io, argp, len);
	// TEMP
	ErrorF("2\n");
	if(gpu_thread_mbox_send(pPS3->gpu_thread, cmd) < 0) {
		ErrorF("Failed writing command to GPU\n");
		return -EIO;
	}
// TEMP
	ErrorF("3\n");
//	gpu_thread_wait();
// TEMP
	ErrorF("4\n");
	if (gpu_thread_mbox_recv(pPS3->gpu_thread, &ret) <= 0) {
		ErrorF("Failed reading return value from GPU\n");
		return -EIO;
	}
// TEMP
	ErrorF("5\n");
	memcpy(argp, &pPS3->io, len);
// TEMP
	ErrorF("6\n");
	return ret;
}
#endif

void PS3GpuCleanup(PS3GpuPtr pPS3)
{
	if (pPS3 == NULL)
		return;

	leave_direct_mode(pPS3);

	unmap_resource((void *) pPS3->ctrl_base, pPS3->ctrl_size);
	unmap_resource((void *) pPS3->fifo_base, pPS3->fifo_size);
	unmap_resource((void *) pPS3->vram_base, pPS3->vram_size);

	xfree(pPS3);
}
