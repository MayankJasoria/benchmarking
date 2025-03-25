// rlibv2/c_wrappers/rdmaio_rctrl_wrapper.cpp
#include "../rdmaio_rctrl_c.h"
#include "../../core/rctrl.hh"
#include "../../core/rmem/handler.hh"
#include "../../core/qps/rc.hh"
#include "../../core/nic.hh"
#include <string>
#include <memory>
#include <cstdio> // For fprintf

using namespace rdmaio;

extern "C" {

rdmaio_rctrl_t* rctrl_create(unsigned short port, const char* host) {
    std::string host_str = (host != nullptr) ? host : "localhost";
    return new rdmaio_rctrl_t{new RCtrl(static_cast<usize>(port), host_str)};
}

bool rctrl_start_daemon(rdmaio_rctrl_t* ctrl_ptr) {
    if (ctrl_ptr && ctrl_ptr->ctrl) {
        return ctrl_ptr->ctrl->start_daemon();
    }
    return false;
}

void rctrl_stop_daemon(rdmaio_rctrl_t* ctrl_ptr) {
    if (ctrl_ptr && ctrl_ptr->ctrl) {
        ctrl_ptr->ctrl->stop_daemon();
    }
}

	bool rctrl_register_mr(rdmaio_rctrl_t* ctrl_ptr, int64_t key, const rdmaio_regattr_t* attr, rdmaio_nic_t* nic_ptr) {
	if (ctrl_ptr && attr && nic_ptr) {
		auto ctrl = static_cast<RCtrl*>(ctrl_ptr);
		auto nic = static_cast<RNic*>(nic_ptr);
		rmem::RegAttr reg_attr = {
			.buf = (void*)attr->buf,
			.sz = attr->sz,
			.key = attr->key,
			.lkey = attr->lkey
		};
		auto mr = RegHandler::create(Arc<RMem>(new RMem(reg_attr.buf, reg_attr.sz)), Arc<RNic>(nic,(RNic*){ /* Do not delete, managed elsewhere */ }));
		if (mr.is_ok()) {
			ctrl->registered_mrs.reg(static_cast<u64>(key), mr.value());
			return true;
		} else {
			fprintf(stderr, "Error registering MR: %s\n", mr.error().c_str());
			return false;
		}
	} else {
		fprintf(stderr, "Error: Invalid arguments for rctrl_register_mr.\n");
		return false;
	}
}

void* rctrl_query_qp(rdmaio_rctrl_t* ctrl_ptr, const char* qp_name) {
	if (ctrl_ptr && ctrl_ptr->ctrl && qp_name) {
		auto qp_opt = ctrl_ptr->ctrl->registered_qps.query(qp_name);
		if (qp_opt.is_some()) {
			return qp_opt.value().get(); // Return the raw pointer to the QP
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

void rctrl_destroy(rdmaio_rctrl_t* ctrl_ptr) {
    if (ctrl_ptr) {
        delete ctrl_ptr;
    }
}

} // extern "C"

struct rdmaio_rctrl_t {
    rdmaio::RCtrl* ctrl;
};