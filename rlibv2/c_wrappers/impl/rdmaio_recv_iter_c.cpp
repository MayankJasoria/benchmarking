// rlibv2/c_wrappers/rdmaio_recv_iter_wrapper.cpp
#include "../rdmaio_recv_iter_c.h"
#include "../../core/recv_iter.hh"
#include "../../core/qps/rc.hh" // Assuming QP is likely RC in this context
#include "../../core/recv_helper.hh"
#include "../../core/result.hh" // Include for IOCode

using namespace rdmaio;
using namespace rdmaio::qp;

// Internal structure to hold the C++ RecvIter instance
struct rdmaio_recv_iter_internal_t {
    std::unique_ptr<RecvIter<RC, 128>> iter; // entry_num is now 128
};

extern "C" {

rdmaio_recv_iter_t* rdmaio_recv_iter_create(void* qp_ptr, void* recv_entries_ptr) {
    if (!qp_ptr || !recv_entries_ptr) return nullptr;

    RC* qp = static_cast<RC*>(qp_ptr); // Assuming qp is wrapped as RC
    RecvEntries* entries = static_cast<RecvEntries*>(recv_entries_ptr);

    rdmaio_recv_iter_t* iter_c = new rdmaio_recv_iter_t;
    iter_c->internal = new rdmaio_recv_iter_internal_t;
    iter_c->internal->iter.reset(new RecvIter<RC, 128>(Arc<RC>(qp,(RC*){ /* Do not delete, managed elsewhere */ }), Arc<RecvEntries>(entries,(RecvEntries*) {/* Do not delete */ })));

    return iter_c;
}

bool rdmaio_recv_iter_has_msgs(const rdmaio_recv_iter_t* iter) {
    if (!iter || !iter->internal || !iter->internal->iter) return false;
    return iter->internal->iter->has_msgs();
}

void rdmaio_recv_iter_next(rdmaio_recv_iter_t* iter) {
    if (iter && iter->internal && iter->internal->iter) {
        iter->internal->iter->next();
    }
}

bool rdmaio_recv_iter_cur_msg(const rdmaio_recv_iter_t* iter, uint32_t* out_imm_data, uintptr_t* out_buf_addr) {
    if (!iter || !iter->internal || !iter->internal->iter) return false;
    auto msg_opt = iter->internal->iter->cur_msg();
    if (msg_opt.is_some()) {
        auto msg = msg_opt.value();
        if (out_imm_data) *out_imm_data = msg.first;
        if (out_buf_addr) *out_buf_addr = reinterpret_cast<uintptr_t>(msg.second);
        return true;
    }
    return false;
}

void rdmaio_recv_iter_clear(rdmaio_recv_iter_t* iter) {
    if (iter && iter->internal && iter->internal->iter) {
        iter->internal->iter->clear();
    }
}

void rdmaio_recv_iter_destroy(rdmaio_recv_iter_t* iter) {
    if (iter) {
        delete iter->internal;
        delete iter;
    }
}

} // extern "C"

struct rdmaio_recv_iter_t {
    rdmaio_recv_iter_internal_t* internal;
};