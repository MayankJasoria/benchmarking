#pragma once

#include "../nic.hh"
#include "./config.hh"

namespace rdmaio {

namespace qp {

/*!
  This file hides low-level calls to libibverbs.
  It's basically some wrapper over libibverbs.
 */
class Impl {
public:
  static Result<std::string>
  bring_qp_to_init(ibv_qp *qp, const QPConfig &config, Arc<RNic> nic) {

    RDMA_ASSERT(qp != nullptr);
    struct ibv_qp_attr qp_attr = {};
    qp_attr.qp_state = IBV_QPS_INIT;
    qp_attr.pkey_index = 0;
    qp_attr.port_num = nic->id.port_id;

    int flags = IBV_QP_STATE | IBV_QP_PKEY_INDEX | IBV_QP_PORT;
    if (qp->qp_type == IBV_QPT_RC) {
      qp_attr.qp_access_flags = config.access_flags;
      flags |= IBV_QP_ACCESS_FLAGS;
    } else if (qp->qp_type == IBV_QPT_UD) {
      qp_attr.qkey = config.qkey;
      flags |= IBV_QP_QKEY;
    } else if (qp->qp_type == IBV_QPT_UC) {
      // TODO: UC not implemented
      RDMA_ASSERT(false) << "UC QP not implemented";
    } else {
      RDMA_ASSERT(false) << "unknown qp type";
    }

    RDMA_ASSERT(qp != nullptr);
    int rc = ibv_modify_qp(qp, &qp_attr, flags);
    if(rc != 0) {
      return Err(std::string(strerror(errno)));
    }
    return Ok(std::string(""));
  }

  static Result<std::string> bring_rc_to_rcv(ibv_qp *qp, const QPConfig &config,
                                             const QPAttr &attr, int port_id) {
    struct ibv_qp_attr qp_attr = {};
    qp_attr.qp_state = IBV_QPS_RTR;
    qp_attr.path_mtu = IBV_MTU_1024;
    qp_attr.dest_qp_num = attr.qpn;
    qp_attr.rq_psn = config.rq_psn; // should this match the sender's psn ?
    qp_attr.max_dest_rd_atomic = config.max_dest_rd_atomic;
    qp_attr.min_rnr_timer = 20;

    qp_attr.ah_attr.dlid = attr.lid;
    qp_attr.ah_attr.sl = 0;
    qp_attr.ah_attr.src_path_bits = 0;
    qp_attr.ah_attr.port_num = port_id; /* Local port id! */

    qp_attr.ah_attr.is_global = 1;
    qp_attr.ah_attr.grh.dgid.global.subnet_prefix = attr.addr.subnet_prefix;
    qp_attr.ah_attr.grh.dgid.global.interface_id = attr.addr.interface_id;
    qp_attr.ah_attr.grh.sgid_index = 0;
    qp_attr.ah_attr.grh.flow_label = 0;
    qp_attr.ah_attr.grh.hop_limit = 255;

    int flags = IBV_QP_STATE | IBV_QP_AV | IBV_QP_PATH_MTU | IBV_QP_DEST_QPN |
                IBV_QP_RQ_PSN | IBV_QP_MAX_DEST_RD_ATOMIC |
                IBV_QP_MIN_RNR_TIMER;
    auto rc = ibv_modify_qp(qp, &qp_attr, flags);
    if (rc != 0)
      return Err(std::string(strerror(errno)));
    return Ok(std::string(""));
  }

  static Result<std::string> bring_rc_to_send(ibv_qp *qp,
                                              const QPConfig &config) {
    int rc, flags;
    struct ibv_qp_attr qp_attr = {};

    qp_attr.qp_state = IBV_QPS_RTS;
    qp_attr.sq_psn = config.sq_psn;
    qp_attr.timeout = config.timeout;
    qp_attr.retry_cnt = 7;
    qp_attr.rnr_retry = 7;
    qp_attr.max_rd_atomic = config.max_rd_atomic;
    qp_attr.max_dest_rd_atomic = config.max_dest_rd_atomic;

    flags = IBV_QP_STATE | IBV_QP_SQ_PSN | IBV_QP_TIMEOUT | IBV_QP_RETRY_CNT |
            IBV_QP_RNR_RETRY | IBV_QP_MAX_QP_RD_ATOMIC;
    rc = ibv_modify_qp(qp, &qp_attr, flags);
    if (rc != 0)
      return Err(std::string(strerror(errno)));
    return Ok(std::string(""));
  }

  /*!
    create the cq using underlying verbs API
   */
  using CreateCQRes_t = Result<std::pair<ibv_cq *, std::string>>;
  static CreateCQRes_t
  create_cq(Arc<RNic> nic, int cq_sz,
            // below two variables used for creating channel
            void *ev_ctx = nullptr,
            struct ibv_comp_channel *channel = nullptr) {
    auto ccq = ibv_create_cq(nic->get_ctx(), cq_sz, ev_ctx, channel, 0);
    if (ccq == nullptr) {
      return Err(std::make_pair(ccq, std::string(strerror(errno))));
    }
    return Ok(std::make_pair(ccq, std::string("")));
  }

  using CreateQPRes_t = Result<std::pair<ibv_qp *, std::string>>;
  static CreateQPRes_t create_qp(Arc<RNic> nic, ibv_qp_type type,
                                 const QPConfig &config,
                                 ibv_cq *cq, // send cq
                                 ibv_cq *recv_cq = nullptr) {

    if (cq == nullptr) {
      return Err(std::make_pair<ibv_qp *, std::string>(nullptr,
                                                       "CQ passed in as null"));
    }

    if (recv_cq == nullptr) {
      recv_cq = cq;
    }

    struct ibv_qp_init_attr qp_init_attr = {};

    qp_init_attr.send_cq = cq;
    qp_init_attr.recv_cq = recv_cq;
    qp_init_attr.qp_type = type;
    qp_init_attr.sq_sig_all = 0;

    qp_init_attr.cap.max_send_wr = config.max_send_sz();
    qp_init_attr.cap.max_recv_wr = config.max_recv_sz();
    qp_init_attr.cap.max_send_sge = 1;
    qp_init_attr.cap.max_recv_sge = 1;
    qp_init_attr.cap.max_inline_data = kMaxInlinSz;

    auto qp = ibv_create_qp(nic->get_pd(), &qp_init_attr);
    if (qp == nullptr) {
      return Err(std::make_pair(qp, std::string(strerror(errno))));
    }
    return Ok(std::make_pair(qp, std::string("")));
  }


  using CreateSrqRes_t = Result<std::pair<ibv_srq *, std::string>>;
  static CreateSrqRes_t create_srq(Arc<RNic> nic, int max_wr, int max_sge) {
    struct ibv_srq_init_attr attr;
    memset((void *)&attr, 0, sizeof(attr));
    attr.attr.max_wr = max_wr;
    attr.attr.max_sge = max_sge;
    auto srq = ibv_create_srq(nic->get_pd(), &attr);

    if (srq == nullptr) {
      return Err(std::make_pair(srq, std::string(strerror(errno))));
    }
    return Ok(std::make_pair(srq, std::string("")));
  }

  using CreateDCTRes_t = Result<std::pair<ibv_exp_dct *, std::string>>;
  static CreateDCTRes_t create_dct(Arc<RNic> nic,
                                   ibv_cq *cq, ibv_srq *srq,
                                   int dc_key)
  {
    if (cq == nullptr) {
      return Err(std::make_pair<ibv_exp_dct *, std::string>(nullptr,
                                                       "CQ passed in as null"));
    }

    if (srq == nullptr) {
      return Err(std::make_pair<ibv_exp_dct *, std::string>(nullptr,
                                                       "SRQ passed in as null"));
    }

    struct ibv_exp_dct_init_attr dctattr;
    memset((void *)&dctattr, 0, sizeof(dctattr));
    dctattr.pd = nic->get_pd();
    dctattr.cq = cq;
    dctattr.srq = srq;
    dctattr.dc_key = dc_key;
    dctattr.port = 1;
    dctattr.access_flags = IBV_ACCESS_REMOTE_WRITE | IBV_ACCESS_REMOTE_READ;
    dctattr.min_rnr_timer = 2;
    dctattr.tclass = 0;
    dctattr.flow_label = 0;
    dctattr.mtu = IBV_MTU_1024;
    dctattr.pkey_index = 0;
    dctattr.hop_limit = 1;
    dctattr.create_flags = 0;
    dctattr.inline_size = 60;
    auto dct = ibv_exp_create_dct(nic->get_ctx(), &dctattr);
    if (dct == nullptr) {
      return Err(std::make_pair(dct, std::string(strerror(errno))));
    }
    return Ok(std::make_pair(dct, std::string("")));
  }

  using CreateQPEXRes_t = Result<std::pair<ibv_qp *, std::string>>;
  static CreateQPEXRes_t create_qp_ex(Arc<RNic> nic, const QPConfig &config,
                                 ibv_cq *cq, // send cq
                                 ibv_cq *recv_cq = nullptr) {
    if (cq == nullptr) {
      return Err(std::make_pair<ibv_qp *, std::string>(nullptr,
                                              "CQ passed in as null"));
    }

    if (recv_cq == nullptr) {
      recv_cq = cq;
    }
    struct ibv_qp_init_attr_ex qp_init_attr = {};
    qp_init_attr.send_cq = cq;
    qp_init_attr.recv_cq = recv_cq;
    qp_init_attr.cap.max_send_wr = config.max_send_sz();
    qp_init_attr.cap.max_send_sge = 1;
    qp_init_attr.cap.max_inline_data = kMaxInlinSz;
    qp_init_attr.qp_type = IBV_EXP_QPT_DC_INI;
    qp_init_attr.pd = nic->get_pd();
    qp_init_attr.comp_mask = IBV_QP_INIT_ATTR_PD;

    auto qp = ibv_create_qp_ex(nic->get_ctx(), &qp_init_attr);
    if (qp == nullptr) {
      return Err(std::make_pair(qp, std::string(strerror(errno))));
    }
    return Ok(std::make_pair(qp, std::string("")));
  }
};

} // namespace qp

} // namespace rdmaio
