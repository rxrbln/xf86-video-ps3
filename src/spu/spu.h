#ifndef __SPU_H
#define __SPU_H

#define __spu_aligned __attribute__((aligned(128)));

typedef unsigned long long eaddr_t;

#define EADDR(ptr) ((eaddr_t) (unsigned long) (ptr))

#endif
