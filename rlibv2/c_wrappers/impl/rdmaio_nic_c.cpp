#include "../rdmaio_nic_c.h"
#include "../../core/nic.hh"
#include "../../core/rnicinfo.hh"
#include "../../core/common.hh"
#include <vector>
#include <cstring>

using namespace rdmaio;

extern "C" {

rdmaio_devidx_t* rnic_info_query_dev_names(size_t* count) {
    std::vector<DevIdx> dev_idxs = RNicInfo::query_dev_names();
    *count = dev_idxs.size();
    if (dev_idxs.empty()) {
        return nullptr;
    }
    rdmaio_devidx_t* c_dev_idxs = static_cast<rdmaio_devidx_t*>(malloc(sizeof(rdmaio_devidx_t) * dev_idxs.size()));
    if (!c_dev_idxs) {
        return nullptr;
    }
    for (size_t i = 0; i < dev_idxs.size(); ++i) {
        c_dev_idxs[i].dev_id = dev_idxs[i].dev_id;
        c_dev_idxs[i].port_id = dev_idxs[i].port_id;
    }
    return c_dev_idxs;
}

void rnic_info_free_dev_names(rdmaio_devidx_t* dev_idxs) {
    free(dev_idxs);
}

rdmaio_nic_t* rnic_create(rdmaio_devidx_t idx, uint8_t gid) {
    auto nic_option = RNic::create({.dev_id = idx.dev_id, .port_id = idx.port_id}, gid);
    if (nic_option.is_some()) {
        return new rdmaio_nic_t{nic_option.value().get()};
    }
    return nullptr;
}

bool rnic_valid(const rdmaio_nic_t* nic_ptr) {
    if (nic_ptr) {
        return static_cast<const RNic*>(nic_ptr)->valid();
    }
    return false;
}

void* rnic_get_ctx(const rdmaio_nic_t* nic_ptr) {
    if (nic_ptr) {
        return static_cast<const RNic*>(nic_ptr)->get_ctx();
    }
    return nullptr;
}

void* rnic_get_pd(const rdmaio_nic_t* nic_ptr) {
    if (nic_ptr) {
        return static_cast<const RNic*>(nic_ptr)->get_pd();
    }
    return nullptr;
}

rdmaio_iocode_t rnic_is_active(const rdmaio_nic_t* nic_ptr, char* err_msg, size_t err_msg_size) {
    if (nic_ptr) {
        auto result = static_cast<const RNic*>(nic_ptr)->is_active();
        if (result == IOCode::Ok) {
            return RDMAIO_OK;
        } else {
            if (err_msg && err_msg_size > 0) {
                strncpy(err_msg, std::get<0>(result.desc).c_str(), err_msg_size - 1);
                err_msg[err_msg_size - 1] = '\0';
            }
            return RDMAIO_ERR;
        }
    }
    return RDMAIO_ERR;
}

void rnic_destroy(rdmaio_nic_t* nic_ptr) {
    if (nic_ptr) {
        delete static_cast<RNic*>(nic_ptr);
    }
}

} // extern "C"

struct rdmaio_nic_t {
    rdmaio::RNic* nic;
};