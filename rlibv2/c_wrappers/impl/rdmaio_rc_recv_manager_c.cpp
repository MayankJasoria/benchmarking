#include "../rdmaio_rc_recv_manager_c.h"
#include "../../core/qps/rc_recv_manager.hh"
#include "../../core/rctrl.hh"
#include "../../core/rmem/mem.hh"
#include <string>
#include <memory>

using namespace rdmaio;
using namespace rdmaio::qp;
using namespace rdmaio::rmem;

// Define a default value for the template parameter R
using DefaultRecvManager = RecvManager; // Assuming R=128 as the default

// Concrete implementation of AbsRecvAllocator
class SimpleAllocator : public AbsRecvAllocator {
    RMem::raw_ptr_t buf = nullptr;
    usize total_mem = 0;
    mr_key_t key;

public:
    virtual ~SimpleAllocator() = default;
    SimpleAllocator(const Arc<RMem>& mem, mr_key_t key)
      : buf(mem->raw_ptr), total_mem(mem->sz), key(key) {
       RDMA_LOG(4) << "simple allocator use key: " << key;
    }

    Option<std::pair<rmem::RMem::raw_ptr_t, rmem::mr_key_t>> alloc_one(
      const usize &sz) override {
       if (total_mem < sz) {
          return {};
       }
       auto ret = buf;
       buf = static_cast<char *>(buf) + sz;
       total_mem -= sz;
       return std::make_pair(ret, key);
    }

    Option<std::pair<rmem::RMem::raw_ptr_t, rmem::RegAttr>> alloc_one_for_remote(
       const usize &sz) override {
       return {};
    }
};

extern "C" {

typedef struct simple_allocator_t {
    SimpleAllocator* allocator;
} simple_allocator_t;

rdmaio_recv_manager_t* recv_manager_create(rdmaio_rctrl_t* ctrl_ptr) {
    if (ctrl_ptr && ctrl_ptr->ctrl) {
        return new rdmaio_recv_manager_t{new DefaultRecvManager(*(ctrl_ptr->ctrl))};
    }
    return nullptr;
}

simple_allocator_t* simple_allocator_create(rdmaio_rmem_t* mem_ptr, uint32_t key) {
    if (mem_ptr && mem_ptr->mem) {
        return new simple_allocator_t{new SimpleAllocator(Arc<RMem>(mem_ptr->mem,(RMem*){ /* Do not delete, managed elsewhere */ }), key)};
    }
    return nullptr;
}

void simple_allocator_destroy(simple_allocator_t* allocator_ptr) {
    if (allocator_ptr) {
        delete allocator_ptr;
    }
}

bool recv_manager_reg_recv_cq(rdmaio_recv_manager_t* manager_ptr, const char* name, void* cq, simple_allocator_t* allocator_ptr) {
    if (manager_ptr && manager_ptr->manager && name && cq && allocator_ptr && allocator_ptr->allocator) {
        auto manager = static_cast<DefaultRecvManager*>(manager_ptr->manager);
        auto allocator = static_cast<AbsRecvAllocator*>(allocator_ptr->allocator);
        auto recv_common = std::make_shared<RecvCommon>(static_cast<ibv_cq*>(cq), Arc<AbsRecvAllocator>(allocator,(AbsRecvAllocator*){ /* Do not delete, managed elsewhere */ }));
        return manager->reg_recv_cqs.reg(name, recv_common);
    }
    return false;
}

void* recv_manager_query_recv_entries(rdmaio_recv_manager_t* manager_ptr, const char* name) {
    if (manager_ptr && manager_ptr->manager && name) {
        auto manager = static_cast<DefaultRecvManager*>(manager_ptr->manager);
        auto recv_entries_opt = manager->reg_recv_entries.query(name);
        if (recv_entries_opt.is_some()) {
            return recv_entries_opt.value().get(); // Return the raw pointer
        }
    }
    return nullptr;
}

void recv_manager_destroy(rdmaio_recv_manager_t* manager_ptr) {
	if (manager_ptr && manager_ptr->manager) {
		delete manager_ptr->manager;
		delete manager_ptr;
	}
}

} // extern "C"

struct rdmaio_recv_manager_t {
    DefaultRecvManager* manager;
};