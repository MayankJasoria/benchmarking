#ifndef RDMAIO_QPATTR_C_H
#define RDMAIO_QPATTR_C_H

#include <stdint.h>

typedef struct rdmaio_qpattr_t {
	uint64_t addr;
	uint32_t lid;
	uint64_t psn;
	uint64_t port_id;
	uint64_t qpn;
	uint64_t qkey;
} rdmaio_qpattr_t;

#endif // RDMAIO_QPATTR_C_H