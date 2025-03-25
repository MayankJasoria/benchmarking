#include "../rdmaio_reg_handler_c.h"
#include "../../core/rmem/handler.hh"
#include <memory>

using namespace rdmaio;

extern "C" {

	rdmaio_reg_handler_t* rdmaio_reg_handler_create(rdmaio_rmem_t* mem_ptr, rdmaio_nic_t* nic_ptr) {
		if (mem_ptr && nic_ptr && mem_ptr->mem) {
			auto mem = std::shared_ptr<rmem::RMem>(mem_ptr->mem,(rmem::RMem*){ /* Do not delete, managed elsewhere */ });
			auto nic = static_cast<RNic*>(nic_ptr);
			auto handler_res = RegHandler::create(mem, Arc<RNic>(nic,(RNic*){ /* Do not delete, managed elsewhere */ }));
			if (handler_res.is_ok()) {
				return new rdmaio_reg_handler_t{handler_res.value().get()};
			}
		}
		return nullptr;
	}

	bool rdmaio_reg_handler_valid(rdmaio_reg_handler_t* handler_ptr) {
		if (handler_ptr && handler_ptr->handler) {
			return handler_ptr->handler->valid();
		}
		return false;
	}

	rdmaio_regattr_t rdmaio_reg_handler_get_attr(rdmaio_reg_handler_t* handler_ptr) {
		rdmaio_regattr_t attr = {};
		if (handler_ptr && handler_ptr->handler) {
			auto reg_attr_opt = handler_ptr->handler->get_reg_attr();
			if (reg_attr_opt.is_some()) {
				auto reg_attr = reg_attr_opt.value();
				attr.buf = reinterpret_cast<uint64_t>(reg_attr.buf);
				attr.sz = reg_attr.sz;
				attr.key = reg_attr.key;
				attr.lkey = reg_attr.lkey;
			}
		}
		return attr;
	}

	void rdmaio_reg_handler_destroy(rdmaio_reg_handler_t* handler_ptr) {
		if (handler_ptr) {
			delete handler_ptr;
		}
	}

} // extern "C"

struct rdmaio_reg_handler_t {
	rdmaio::RegHandler* handler;
};