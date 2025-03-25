#include "../rdmaio_lib_c.h"
#include "../../core/lib.hh"
#include "../../core/qps/rc.hh" // Include RC definition
#include "../../core/qps/config.hh" // Include QPConfig definition
#include "../../core/rmem/handler.hh" // Include RegAttr definition

using namespace rdmaio;

extern "C" {

rdmaio_connect_manager_t* rdmaio_connect_manager_create(const char* addr) {
    if (!addr) return nullptr;
    return new rdmaio_connect_manager_t{new ConnectManager(addr)};
}

rdmaio_iocode_t rdmaio_connect_manager_wait_ready(rdmaio_connect_manager_t* cm, double timeout_sec, size_t retry) {
    if (!cm || !cm->cm) return RDMAIO_ERR;
    Result<std::string> result = cm->cm->wait_ready(timeout_sec * 1000000, retry);
    return static_cast<rdmaio_iocode_t>(result.code.c);
}

rdmaio_iocode_t rdmaio_connect_manager_cc_rc_msg(rdmaio_connect_manager_t* cm,
                                                        const char* qp_name,
                                                        const char* channel_name,
                                                        size_t max_msg_size,
                                                        void* rc_ptr,
                                                        uint32_t remote_nic_id,
                                                        const rdmaio_qpconfig_t* config) {
    if (!cm || !cm->cm || !rc_ptr || !qp_name || !channel_name) return RDMAIO_ERR;

    Arc<qp::RC> rc = std::static_pointer_cast<qp::RC>(static_cast<Dummy*>(rc_ptr)->shared_from_this());
    qp::QPConfig qp_config;
    if (config) {
        qp_config.access_flags = config->access_flags;
        qp_config.max_rd_atomic = config->max_rd_atomic;
        qp_config.max_dest_rd_atomic = config->max_dest_atomic;
        qp_config.rq_psn = config->rq_psn;
        qp_config.sq_psn = config->sq_psn;
        qp_config.timeout = config->timeout;
        qp_config.max_send_size = config->max_send_size;
        qp_config.max_recv_size = config->max_recv_size;
        qp_config.qkey = config->qkey;
        qp_config.dc_key = config->dc_key;
    }

    auto result = cm->cm->cc_rc_msg(qp_name, channel_name, max_msg_size, rc, remote_nic_id, qp_config);
    return static_cast<rdmaio_iocode_t>(result.code.c);
}

rdmaio_iocode_t rdmaio_connect_manager_fetch_remote_mr(rdmaio_connect_manager_t* cm,
                                                               const char* id,
                                                               rdmaio_regattr_t* out_attr) {
    if (!cm || !cm->cm || !id || !out_attr) return RDMAIO_ERR;
    auto result = cm->cm->fetch_remote_mr(id);
    if (result == IOCode::Ok) {
        rmem::RegAttr attr = std::get(result.desc);
        out_attr->buf = attr.buf;
        out_attr->sz = attr.sz;
        out_attr->key = attr.key;
        out_attr->lkey = attr.lkey;
        return RDMAIO_OK;
    } else {
        return static_cast<rdmaio_iocode_t>(result.code.c);
    }
}

rdmaio_rc_t* rctrl_query_qp(rdmaio_rctrl_t* ctrl_ptr, const char* name) {
    if (ctrl_ptr && name) {
       auto ctrl = static_cast<RCtrl*>(ctrl_ptr);
       auto qp_option = ctrl->registered_qps.query(name);
       if (qp_option.is_some()) {
          return new rdmaio_rc_t{qp_option.value().get()};
       }
    }
    return nullptr;
}

bool rctrl_register_nic(rdmaio_rctrl_t* ctrl_ptr, const char* name, rdmaio_nic_t* nic_ptr) {
    if (ctrl_ptr && name && nic_ptr) {
       auto ctrl = static_cast<RCtrl*>(ctrl_ptr);
       auto nic = static_cast<RNic*>(nic_ptr);
       ctrl->opened_nics.reg(name, Arc<RNic>(nic,(RNic*){ /* Do not delete, managed elsewhere */ }));
       return true;
    }
    return false;
}

void rdmaio_connect_manager_destroy(rdmaio_connect_manager_t* cm) {
    if (cm) {
        delete cm;
    }
}

} // extern "C"

struct rdmaio_connect_manager_t {
    rdmaio::ConnectManager* cm;
};