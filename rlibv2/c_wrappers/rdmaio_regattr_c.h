#ifndef RDMAIO_REGATTR_C_H
#define RDMAIO_REGATTR_C_H

#include <stdint.h>

typedef struct rdmaio_regattr_t {
	uintptr_t addr;
	uint64_t length;
	uint32_t lkey;
	uint32_t rkey;
} rdmaio_regattr_t;

#endif // RDMAIO_REGATTR_C_H