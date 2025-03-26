// rlibv2/c_wrappers/rdmaio_rctrl_wrapper.cpp
#include "../rdmaio_rctrl_c.h"
#include "../../core/rctrl.hh"
#include "../../core/rmem/handler.hh"
#include "../../core/qps/rc.hh"
#include "../../core/nic.hh"
#include <string>
#include <memory>
#include <cstdio> // For fprintf
#include <optional> // Include for std::optional

using namespace rdmaio;
using namespace rdmaio::rmem;

extern "C" {

rdmaio_rctrl_t* rctrl_create(unsigned short port, const char* host) {
    std::string host_str = (host != nullptr) ? host : "localhost";
    RCtrl* rctrl_obj = new RCtrl(static_cast<usize>(port), host_str);
    rdmaio_rctrl_t* wrapper = new rdmaio_rctrl_t;
    wrapper->ctrl = rctrl_obj;
    return wrapper;
}

bool rctrl_start_daemon(rdmaio_rctrl_t* ctrl_ptr) {
    if (ctrl_ptr && ctrl_ptr->ctrl) {
        RCtrl* ctrl = static_cast<RCtrl*>(ctrl_ptr->ctrl);
        return ctrl->start_daemon();
    }
    return false;
}

void rctrl_stop_daemon(rdmaio_rctrl_t* ctrl_ptr) {
    if (ctrl_ptr && ctrl_ptr->ctrl) {
        RCtrl* ctrl = static_cast<RCtrl*>(ctrl_ptr->ctrl);
        ctrl->stop_daemon();
    }
}

bool rctrl_register_mr(rdmaio_rctrl_t* ctrl_ptr, int64_t key, const rdmaio_regattr_t* attr, rdmaio_nic_t* nic_ptr) {
    if (ctrl_ptr && ctrl_ptr->ctrl && attr && nic_ptr && nic_ptr->nic) {
       RCtrl* ctrl = static_cast<RCtrl*>(ctrl_ptr->ctrl);
       Arc<RNic>* arc_nic_ptr = static_cast<Arc<RNic>*>(nic_ptr->nic);
       Arc<RNic> arc_nic = *arc_nic_ptr;
       rmem::RegAttr reg_attr = {
          .buf = attr->addr,
          .sz = attr->length,
          .key = attr->rkey,
          .lkey = attr->lkey
       };

       // Create Arc<RMem> that points to the memory described by reg_attr
       Arc<RMem> mem(reinterpret_cast<RMem*>(attr->addr), [](RMem*){ /* No-op deleter */ });

       auto mr = RegHandler::create(mem, arc_nic);
       if (mr.has_value()) {
          ctrl->registered_mrs.reg(static_cast<u64>(key), mr.value());
          return true;
       } else {
          fprintf(stderr, "Error registering MR (check logs)\n");
          return false;
       }
    } else {
       fprintf(stderr, "Error: Invalid arguments for rctrl_register_mr.\n");
       return false;
    }
}

rdmaio_rc_t* rctrl_query_qp(rdmaio_rctrl_t* ctrl_ptr, const char* qp_name) {
    if (ctrl_ptr && ctrl_ptr->ctrl && qp_name) {
       RCtrl* ctrl = static_cast<RCtrl*>(ctrl_ptr->ctrl);
       auto qp_opt = ctrl->registered_qps.query(qp_name);
       if (qp_opt.has_value()) {
          // Wrap the raw pointer to the RC object
          rdmaio_rc_t* rc_wrapper = new rdmaio_rc_t;
          rc_wrapper->rc = qp_opt.value().get();
          return rc_wrapper;
       }
    }
    return nullptr;
}

bool rctrl_register_nic(rdmaio_rctrl_t* ctrl_ptr, uint64_t nic_id, rdmaio_nic_t* nic_ptr) {
    if (ctrl_ptr && ctrl_ptr->ctrl && nic_ptr && nic_ptr->nic) {
       RCtrl* ctrl = static_cast<RCtrl*>(ctrl_ptr->ctrl);
       Arc<RNic>* arc_nic_ptr = static_cast<Arc<RNic>*>(nic_ptr->nic);
       ctrl->opened_nics.reg(nic_id, *arc_nic_ptr);
       return true;
    }
    return false;
}

void rctrl_destroy(rdmaio_rctrl_t* ctrl_ptr) {
    if (ctrl_ptr) {
        RCtrl* ctrl = static_cast<RCtrl*>(ctrl_ptr->ctrl);
        delete ctrl;
        delete ctrl_ptr;
    }
}

} // extern "C"