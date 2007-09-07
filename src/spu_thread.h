#ifndef __SPU_THREAD_H
#define __SPU_THREAD_H

struct spu_thread;

int spu_thread_create(struct spu_thread **_thread, void *func, void *argp);
int spu_thread_join(struct spu_thread *thread);

int spu_thread_mbox_send(struct spu_thread *thread, int data);
int spu_thread_mbox_recv(struct spu_thread *thread, int *data);

struct spu_thread *spu_thread_wait(void);
int spu_thread_init(void);
int spu_thread_cleanup(void);

#endif
