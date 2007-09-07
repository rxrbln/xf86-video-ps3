#include <libspe2.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include "spu/spu.h"
#include "spu_thread.h"

spe_event_handler_ptr_t event_handler;

struct spu_thread {
	pthread_t pthread;
	spe_context_ptr_t     spe;
	spe_program_handle_t *program;
	void *argp;
};

static void *spu_thread_routine(void *arg)
{
	struct spu_thread *spu_thread = (struct spu_thread *) arg;

	unsigned int          runflags    = SPE_RUN_USER_REGS;
	unsigned int          entry       = SPE_DEFAULT_ENTRY;
	void*                 argp        = spu_thread->argp;
	void*                 envp        = NULL;

	spe_context_ptr_t      spe        = spu_thread->spe;
	spe_program_handle_t* program     = spu_thread->program;
	spe_stop_info_t       stop_info;

	struct {
		eaddr_t spe;
		unsigned long long dummy0;
		eaddr_t argp;
		unsigned long long dummy1;
		eaddr_t envp;
		unsigned long long dummy2;
	} __attribute__((packed)) user_regs;

	memset(&user_regs, 0, sizeof(user_regs));
	user_regs.spe = EADDR(spe);
	user_regs.argp = EADDR(argp);
	user_regs.envp = EADDR(envp);

	spe_program_load(spe, program);
	spe_context_run(spe, &entry, runflags, &user_regs, 0, &stop_info);
	spe_image_close(program);
	spe_context_destroy(spe);

	return (0);
}

static int spu_thread_register(struct spu_thread *thread)
{
	spe_event_unit_t event;

	event.events = SPE_EVENT_OUT_INTR_MBOX;
	event.spe = thread->spe;
	event.data.ptr = thread;

	if (spe_event_handler_register(event_handler, &event) < 0)
		return -1;

	return 0;
}

static int spu_thread_unregister(struct spu_thread *thread)
{
#if 0 // causes segfault
	spe_event_unit_t event;

	event.events = SPE_EVENT_OUT_INTR_MBOX;
	event.spe = thread->spe;

	if (spe_event_handler_deregister(event_handler, &event) < 0)
		return -1;
#endif

	return 0;
}

int spu_thread_create(struct spu_thread **_thread, void *code, void *argp)
{
	struct spu_thread *thread;
	int ret;

	thread = (struct spu_thread *) malloc(sizeof(struct spu_thread));
	if (thread == NULL)
		return -1;
	thread->argp = argp;
	thread->spe = spe_context_create(SPE_EVENTS_ENABLE, 0);
	thread->program = (spe_program_handle_t *) code;

	spu_thread_register(thread);

	ret = pthread_create(&thread->pthread, NULL,
			     spu_thread_routine, thread);
	if (ret < 0)
		return ret;

	*_thread = thread;
	return ret;
}

int spu_thread_join(struct spu_thread *thread)
{
	int ret = pthread_join(thread->pthread, NULL);
	spu_thread_unregister(thread);
	free(thread);

	return ret;
}

int spu_thread_mbox_send(struct spu_thread *thread, int data)
{
	return spe_in_mbox_write(thread->spe, (unsigned int *) &data,
				 1, SPE_MBOX_ANY_NONBLOCKING);
}

int spu_thread_mbox_recv(struct spu_thread *thread, int *data)
{
	return spe_out_intr_mbox_read(thread->spe, (unsigned int *) data,
				      1, SPE_MBOX_ANY_NONBLOCKING);
}

struct spu_thread *spu_thread_wait(void)
{
	struct spu_thread *thread;
	spe_event_unit_t event;
	sigset_t set, oldset;

	sigfillset(&set);
	sigprocmask(SIG_SETMASK, &set, &oldset);
	if (spe_event_wait(event_handler, &event, 1, -1) <= 0) {
		sigprocmask(SIG_SETMASK, &oldset, NULL);
		return NULL;
	}
	sigprocmask(SIG_SETMASK, &oldset, NULL);

	thread = (struct spu_thread *) event.data.ptr;

	return thread;
}

int spu_thread_init(void)
{
	event_handler = spe_event_handler_create();

	return 0;
}

int spu_thread_cleanup(void)
{
	return spe_event_handler_destroy(event_handler);
}
