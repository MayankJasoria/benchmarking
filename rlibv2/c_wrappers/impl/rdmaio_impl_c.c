// rlibv2/c_wrappers/rdmaio_impl_c.cpp
#include "../rdmaio_impl_c.h"
#include "../../core/qps/impl.hh"
#include "../../core/nic.hh"
#include "../../core/qps/config.hh"
#include <cstring>

using namespace rdmaio;
using namespace rdmaio::qp;

extern "C" {

rdmaio_iocode_t rdmaio_bring_qp_to_init(void* qp, const rdmaio_qpconfig_t* config_ptr, rdmaio_nic_t* nic_ptr, char* err_msg, size_t err_msg_size) {
    if (!qp || !config_ptr || !nic_ptr) return RDMAIO_ERR;
    auto config = *static_cast<const QPConfig*>(config_ptr);
    auto nic = static_cast<RNic*>(nic_ptr);
    auto result = Impl::bring_qp_to_init(static_cast<ibv_qp*>(qp), config, Arc<RNic>(nic,(RNic*){ /* Do not delete, managed elsewhere */ }));
    if (result.is_ok()) {
        return RDMAIO_OK;
    } else {
        if (err_msg && err_msg_size > 0) {
            strncpy(err_msg, result.error().c_str(), err_msg_size - 1);
            err_msg[err_msg_size - 1] = '\0';
        }
        return RDMAIO_ERR;
    }
}

rdmaio_iocode_t rdmaio_bring_rc_to_rcv(void* qp, const rdmaio_qpconfig_t* config_ptr, const rdmaio_qpattr_t* attr_ptr, int port_id, char* err_msg, size_t err_msg_size) {
    if (!qp || !config_ptr || !attr_ptr) return RDMAIO_ERR;
    auto config = *static_cast<const QPConfig*>(config_ptr);
    QPAttr attr = {
        .qpn = attr_ptr->qpn,
        .lid = attr_ptr->lid,
        .addr = {
            .subnet_prefix = attr_ptr->addr.subnet_prefix,
            .interface_id = attr_ptr->addr.interface_id,
            .local_id = attr_ptr->addr.local_id
        }
    };
    auto result = Impl::bring_rc_to_rcv(static_cast<ibv_qp*>(qp), config, attr, port_id);
    if (result.is_ok()) {
        return RDMAIO_OK;
    } else {
        if (err_msg && err_msg_size > 0) {
            strncpy(err_msg, result.error().c_str(), err_msg_size - 1);
            err_msg[err_msg_size - 1] = '\0';
        }
        return RDMAIO_ERR;
    }
}

rdmaio_iocode_t rdmaio_bring_rc_to_send(void* qp, const rdmaio_qpconfig_t* config_ptr, char* err_msg, size_t err_msg_size) {
    if (!qp || !config_ptr) return RDMAIO_ERR;
    auto config = *static_cast<const QPConfig*>(config_ptr);
    auto result = Impl::bring_rc_to_send(static_cast<ibv_qp*>(qp), config);
    if (result.is_ok()) {
        return RDMAIO_OK;
    } else {
        if (err_msg && err_msg_size > 0) {
            strncpy(err_msg, result.error().c_str(), err_msg_size - 1);
            err_msg[err_msg_size - 1] = '\0';
        }
        return RDMAIO_ERR;
    }
}

void* rdmaio_create_cq(rdmaio_nic_t* nic_ptr, int cq_sz, char* err_msg, size_t err_msg_size) {
    if (!nic_ptr) return nullptr;
    auto nic = static_cast<RNic*>(nic_ptr);
    auto result = Impl::create_cq(Arc<RNic>(nic,(RNic*){ /* Do not delete, managed elsewhere */ }), cq_sz);
    if (result.is_ok()) {
        return result.value().first;
    } else {
        if (err_msg && err_msg_size > 0) {
            strncpy(err_msg, result.value().second.c_str(), err_msg_size - 1);
            err_msg[err_msg_size - 1] = '\0';
        }
        return nullptr;
    }
}

void* rdmaio_create_qp(rdmaio_nic_t* nic_ptr, int qp_type, const rdmaio_qpconfig_t* config_ptr, void* send_cq, void* recv_cq, char* err_msg, size_t err_msg_size) {
    if (!nic_ptr || !config_ptr || !send_cq) return nullptr;
    auto nic = static_cast<RNic*>(nic_ptr);
    auto config = *static_cast<const QPConfig*>(config_ptr);
    auto result = Impl::create_qp(Arc<RNic>(nic,(RNic*){ /* Do not delete, managed elsewhere */ }), static_cast<ibv_qp_type>(qp_type), config, static_cast<ibv_cq*>(send_cq), static_cast<ibv_cq*>(recv_cq));
    if (result.is_ok()) {
        return result.value().first;
    } else {
        if (err_msg && err_msg_size > 0) {
            strncpy(err_msg, result.value().second.c_str(), err_msg_size - 1);
            err_msg[err_msg_size - 1] = '\0';
        }
        return nullptr;
    }
}

void* rdmaio_create_srq(rdmaio_nic_t* nic_ptr, int max_wr, int max_sge, char* err_msg, size_t err_msg_size) {
    if (!nic_ptr) return nullptr;
    auto nic = static_cast<RNic*>(nic_ptr);
    auto result = Impl::create_srq(Arc<RNic>(nic,(RNic*){ /* Do not delete, managed elsewhere */ }), max_wr, max_sge);
    if (result.is_ok()) {
        return result.value().first;
    } else {
        if (err_msg && err_msg_size > 0) {
            strncpy(err_msg, result.value().second.c_str(), err_msg_size - 1);
            err_msg[err_msg_size - 1] = '\0';
        }
        return nullptr;
    }
}

void* rdmaio_create_dct(rdmaio_nic_t* nic_ptr, void* cq, void* srq, int dc_key, char* err_msg, size_t err_msg_size) {
    if (!nic_ptr || !cq || !srq) return nullptr;
    auto nic = static_cast<RNic*>(nic_ptr);
    auto result = Impl::create_dct(Arc<RNic>(nic,(RNic*){ /* Do not delete, managed elsewhere */ }), static_cast<ibv_cq*>(cq), static_cast<ibv_srq*>(srq), dc_key);
    if (result.is_ok()) {
        return result.value().first;
    } else {
        if (err_msg && err_msg_size > 0) {
            strncpy(err_msg, result.value().second.c_str(), err_msg_size - 1);
            err_msg[err_msg_size - 1] = '\0';
        }
        return nullptr;
    }
}

void* rdmaio_create_qp_ex(rdmaio_nic_t* nic_ptr, const rdmaio_qpconfig_t* config_ptr, void* send_cq, void* recv_cq, char* err_msg, size_t err_msg_size) {
    if (!nic_ptr || !config_ptr || !send_cq) return nullptr;
    auto nic = static_cast<RNic*>(nic_ptr);
    auto config = *static_cast<const QPConfig*>(config_ptr);
    auto result = Impl::create_qp_ex(Arc<RNic>(nic,(RNic*){ /* Do not delete, managed elsewhere */ }), config, static_cast<ibv_cq*>(send_cq), static_cast<ibv_cq*>(recv_cq));
    if (result.is_ok()) {
        return result.value().first;
    } else {
        if (err_msg && err_msg_size > 0) {
            strncpy(err_msg, result.value().second.c_str(), err_msg_size - 1);
            err_msg[err_msg_size - 1] = '\0';
        }
        return nullptr;
    }
}

void rdmaio_destroy_cq(void* cq) {
    if (cq) {
        ibv_destroy_cq(static_cast<ibv_cq*>(cq));
    }
}

void rdmaio_destroy_qp(void* qp) {
    if (qp) {
        ibv_destroy_qp(static_cast<ibv_qp*>(qp));
    }
}

void rdmaio_destroy_srq(void* srq) {
    if (srq) {
        ibv_destroy_srq(static_cast<ibv_srq*>(srq));
    }
}

void rdmaio_destroy_dct(void* dct) {
    if (dct) {
        ibv_exp_dct_destroy(static_cast<ibv_exp_dct*>(dct));
    }
}

} // extern "C"