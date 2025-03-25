#include "../rdmaio_rc_c.h"
#include "../../core/qps/rc.hh"
#include "../../core/rmem/handler.hh"
#include "../../core/result.hh"
#include "../../core/qps/mod.hh"
#include "../../core/qps/config.hh"

#include <optional>
#include <cstring>
#include <errno.h>
#include <new> // For std::bad_alloc

using namespace rdmaio;
using namespace rdmaio::qp;
using namespace rdmaio::rmem;

extern "C" {

// --- RC Class ---
struct rdmaio_rc_t {
  rdmaio::qp::RC* rc;
};


rdmaio_rc_t* rdmaio_rc_create(void* nic, const rdmaio_qpconfig_t* config, struct ibv_cq* recv_cq) {
    if (!nic || !config) return nullptr; // Ensure config is not NULL
    QPConfig qp_config = QPConfig();

    qp_config.set_access_flags(config->access_flags);
    qp_config.set_max_rd_ops(config->max_rd_atomic); // Assuming max_rd_atomic in C wrapper maps to this
    qp_config.set_psn(config->rq_psn); // Assuming rq_psn and sq_psn are the same for initial setup
    qp_config.set_timeout(config->timeout);
    qp_config.set_max_send(config->max_send_size);
    qp_config.set_max_recv(config->max_recv_size);
    qp_config.set_qkey(config->qkey);
    qp_config.set_dc_key(config->dc_key);
    auto rc_option = RC::create(std::static_pointer_cast<RNic>(static_cast<Dummy*>(nic)->shared_from_this()), qp_config, recv_cq);
    if (rc_option.is_some()) {
        return new rdmaio_rc_t{rc_option.value().get()};
    }
    return nullptr;
}

void rdmaio_rc_destroy(rdmaio_rc_t* rc) {
    if (rc) {
        delete rc;
    }
}

void rdmaio_rc_my_attr(const rdmaio_rc_t* rc, rdmaio_qpattr_t* out_attr) {
    if (rc && out_attr) {
        QPAttr qp_attr = rc->rc->my_attr();
        out_attr->addr = qp_attr.addr.val; // Assuming RAddress has a 'val' member
        out_attr->lid = qp_attr.lid;
        out_attr->psn = qp_attr.psn;
        out_attr->port_id = qp_attr.port_id;
        out_attr->qpn = qp_attr.qpn;
        out_attr->qkey = qp_attr.qkey;
    }
}

rdmaio_iocode_t rdmaio_rc_my_status(const rdmaio_rc_t* rc) {
    if (rc) {
        Result<> rc_status = rc->rc->my_status();
        return static_cast<rdmaio_iocode_t>(rc_status.code.c);
    }
    return static_cast<rdmaio_iocode_t>(IOCode::Err); // Indicate error
}

void rdmaio_rc_bind_remote_mr(rdmaio_rc_t* rc, const rdmaio_regattr_t* mr) {
    if (rc && mr) {
        RegAttr remote_mr;
        remote_mr.buf = mr->addr;
        remote_mr.sz = mr->length;
        remote_mr.key = mr->rkey;
        remote_mr.lkey = mr->lkey;
        rc->rc->bind_remote_mr(remote_mr);
    }
}

void rdmaio_rc_bind_local_mr(rdmaio_rc_t* rc, const rdmaio_regattr_t* mr) {
    if (rc && mr) {
        RegAttr local_mr;
        local_mr.buf = mr->addr;
        local_mr.sz = mr->length;
        local_mr.key = mr->rkey;
        local_mr.lkey = mr->lkey;
        rc->rc->bind_local_mr(mr);
    }
}

int rdmaio_rc_connect(rdmaio_rc_t* rc, const rdmaio_qpattr_t* attr, char* out_error_msg, size_t error_msg_size) {
    if (rc && attr) {
        QPAttr qp_attr;
        qp_attr.addr.val = attr->addr; // Assuming RAddress has a 'val' member
        qp_attr.lid = attr->lid;
        qp_attr.psn = attr->psn;
        qp_attr.port_id = attr->port_id;
        qp_attr.qpn = attr->qpn;
        qp_attr.qkey = attr->qkey;
        auto result = rc->rc->connect(qp_attr);
        if (result.is_ok()) {
            return 0; // Success
        } else {
            if (out_error_msg && error_msg_size > 0) {
                strncpy(out_error_msg, result.desc.c_str(), error_msg_size - 1);
                out_error_msg[error_msg_size - 1] = '\0';
            }
            return -1; // Error
        }
    }
    return -1;
}

int rdmaio_rc_send_normal(rdmaio_rc_t* rc, const rdmaio_reqdesc_t* desc, const rdmaio_reqpayload_t* payload, char* out_error_msg, size_t error_msg_size) {
    if (rc && desc && payload) {
        RC::ReqDesc req_desc;
        req_desc.op = static_cast<ibv_wr_opcode>(desc->op);
        req_desc.flags = desc->flags;
        req_desc.len = desc->len;
        req_desc.wr_id = desc->wr_id;

        RC::ReqPayload req_payload;
        req_payload.local_addr = reinterpret_cast<rmem::RMem::raw_ptr_t>(payload->local_addr);
        req_payload.remote_addr = payload->remote_addr;
        req_payload.imm_data = payload->imm_data;

        auto result = rc->rc->send_normal(req_desc, req_payload);
        if (result.is_ok()) {
            return 0; // Success
        } else {
            if (out_error_msg && error_msg_size > 0) {
                strncpy(out_error_msg, result.desc.c_str(), error_msg_size - 1);
                out_error_msg[error_msg_size - 1] = '\0';
            }
            return -1; // Error
        }
    }
    return -1;
}

int rdmaio_rc_send_normal_mr(rdmaio_rc_t* rc, const rdmaio_reqdesc_t* desc, const rdmaio_reqpayload_t* payload,
                             const rdmaio_regattr_t* local_mr, const rdmaio_regattr_t* remote_mr,
                             char* out_error_msg, size_t error_msg_size) {
    if (rc && desc && payload && local_mr && remote_mr) {
        RC::ReqDesc req_desc;
        req_desc.op = static_cast<ibv_wr_opcode>(desc->op);
        req_desc.flags = desc->flags;
        req_desc.len = desc->len;
        req_desc.wr_id = desc->wr_id;

        RC::ReqPayload req_payload;
        req_payload.local_addr = reinterpret_cast<rmem::RMem::raw_ptr_t>(payload->local_addr);
        req_payload.remote_addr = payload->remote_addr;
        req_payload.imm_data = payload->imm_data;

        RegAttr reg_local_mr;
        reg_local_mr.buf = local_mr->addr;
        reg_local_mr.sz = local_mr->length;
        reg_local_mr.key = local_mr->rkey;
        reg_local_mr.lkey = local_mr->lkey;

        RegAttr reg_remote_mr;
        reg_remote_mr.buf = remote_mr->addr;
        reg_remote_mr.sz = remote_mr->length;
        reg_remote_mr.key = remote_mr->rkey;
        reg_remote_mr.lkey = remote_mr->lkey;

        auto result = rc->rc->send_normal(req_desc, req_payload, local_mr, remote_mr);
        if (result.is_ok()) {
            return 0; // Success
        } else {
            if (out_error_msg && error_msg_size > 0) {
                strncpy(out_error_msg, result.desc.c_str(), error_msg_size - 1);
                out_error_msg[error_msg_size - 1] = '\0';
            }
            return -1; // Error
        }
    }
    return -1;
}

int rdmaio_rc_poll_rc_comp(rdmaio_rc_t* rc, uint64_t* out_user_wr, struct ibv_wc* out_wc) {
    if (rc) {
        auto result = rc->rc->poll_rc_comp();
        if (result.is_some()) {
            if (out_user_wr) *out_user_wr = result.value().first;
            if (out_wc) std::memcpy(out_wc, &result.value().second, sizeof(ibv_wc));
            return 0; // Success
        }
    }
    return -1; // No completion
}

int rdmaio_rc_wait_rc_comp(rdmaio_rc_t* rc, double timeout, uint64_t* out_user_wr, struct ibv_wc* out_wc) {
    if (rc) {
        auto result = rc->rc->wait_rc_comp(timeout);
        if (result.is_ok()) {
            if (out_user_wr) *out_user_wr = result.value().first;
            if (out_wc) std::memcpy(out_wc, &result.value().second, sizeof(ibv_wc));
            return 0; // Success
        } else if (result.is_err()) {
            return -1; // Error
        } else {
            return 1; // Timeout (assuming Timeout is a specific result)
        }
    }
    return -1;
}

int rdmaio_rc_max_send_sz(const rdmaio_rc_t* rc) {
    if (rc) {
        return rc->rc->max_send_sz();
    }
    return -1;
}

void rdmaio_rc_destroy(rdmaio_rc_t* rc_ptr) {
    if (rc_ptr) {
        delete static_cast<qp::RC*>(rc_ptr);
    }
}

} // extern "C"

// --- C Structure Definitions (Mirroring C++ structures) ---

struct rdmaio_rc_t {
    rdmaio::qp::RC* rc;
};