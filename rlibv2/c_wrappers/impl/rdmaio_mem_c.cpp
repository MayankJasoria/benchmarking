// rlibv2/c_wrappers/rdmaio_mem_wrapper.cpp
#include "rlibv2/c_wrappers/rdmaio_mem_c.h"
#include "../../core/rmem/mem.hh"

using namespace rdmaio::rmem;

extern "C" {

	rdmaio_rmem_t* rmem_create(uint64_t size) {
		return new rdmaio_rmem_t{new RMem(size)};
	}

	bool rmem_valid(rdmaio_rmem_t* mem) {
		if (mem && mem->mem) {
			return mem->mem->valid();
		}
		return false;
	}

	void rmem_destroy(rdmaio_rmem_t* mem) {
		if (mem) {
			delete mem;
		}
	}

} // extern "C"

struct rdmaio_rmem_t {
	rdmaio::rmem::RMem* mem;
};