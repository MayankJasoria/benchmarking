#include <gflags/gflags.h>
#include <cstddef>
#include <thread>

#include "rlibv2/core/lib.hh"
#include "rlibv2/core/qps/rc_recv_manager.hh"
#include "rlibv2/core/qps/recv_iter.hh"

DEFINE_string(addr, "192.168.252.212:8888", "Client IP address");
DEFINE_int64(port, 8888, "Server listener (UDP) port.");
DEFINE_int64(use_nic_idx, 0, "Which NIC to create QP");
DEFINE_int64(reg_mem_name, 73, "The name to register an MR at rctrl.");
DEFINE_int64(reg_ack_mem_name, 146, "The name to register the ack MR at rctrl");
DEFINE_string(cq_name, "test_channel", "The name to register an receive cq");
DEFINE_string(ack_cq_name, "ack_channel", "The name to register an acknowledgement cq");
DEFINE_int32(buffer_size, 1024*1024*1024, "Total buffer size");
DEFINE_int32(ack_buffer_size, 1024, "Buffer for ack messages");
DEFINE_int32(msg_size, 1024, "Size of each message to send");

using namespace rdmaio;
using namespace rdmaio::rmem;
using namespace rdmaio::qp;
using namespace std;

constexpr usize entry_num = 256;

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

pair<Arc<RC>, Arc<RegHandler>> init_send_queue(Arc<RNic> &nic) {
	// 1. create the local QP to send
	auto qp = RC::create(nic, QPConfig()).value();

	ConnectManager cm(FLAGS_addr);
	if (cm.wait_ready(100000, 4) ==
		IOCode::Timeout) {  // wait 1 second for server to ready, retry 2 times
		RDMA_LOG(WARNING) << "connect to the " << FLAGS_addr << " timeout!";
		}

	sleep(1);
	// 2. create the remote QP and connect (max msg size is 1 byte as we only care about imm_data)
	auto qp_res = cm.cc_rc_msg("server_qp", FLAGS_ack_cq_name,
		1, qp, FLAGS_reg_ack_mem_name, QPConfig());
	RDMA_ASSERT(qp_res == IOCode::Ok) << std::get<0>(qp_res.desc);

	// 3. fetch the remote MR for usage
	auto fetch_res = cm.fetch_remote_mr(FLAGS_reg_ack_mem_name);
	RDMA_ASSERT(fetch_res == IOCode::Ok) << std::get<0>(fetch_res.desc);
	rmem::RegAttr remote_attr = std::get<1>(fetch_res.desc);

	// 4. register a local buffer for sending messages
	auto local_mr = RegHandler::create(Arc<RMem>(new RMem(FLAGS_ack_buffer_size)), nic).value();

	qp->bind_remote_mr(remote_attr);
	qp->bind_local_mr(local_mr->get_reg_attr().value());

	return make_pair(qp, local_mr);
}

pair<shared_ptr<Dummy>, shared_ptr<RecvEntries<entry_num>>> init_recv_queue(RCtrl &ctrl, Arc<RNic> &nic, RecvManager<entry_num> &manager) {
	// 1. create receive cq
	auto recv_cq_res = ::rdmaio::qp::Impl::create_cq(nic, entry_num);
	RDMA_ASSERT(recv_cq_res == IOCode::Ok);
	auto recv_cq = std::get<0>(recv_cq_res.desc);

	// 2. prepare the message buffer with allocator
	auto mem =
	  Arc<RMem>(new RMem(static_cast<const rdmaio::u64>(FLAGS_buffer_size)));
	auto handler = RegHandler::create(mem, nic).value();
	auto alloc = std::make_shared<SimpleAllocator>(
	  mem, handler->get_reg_attr().value().key);

	// 3. register receive cq
	manager.reg_recv_cqs.create_then_reg(FLAGS_cq_name, recv_cq, alloc);
	RDMA_LOG(EMPH) << "Register test_channel";
	ctrl.registered_mrs.reg(FLAGS_reg_mem_name, handler);

	ctrl.start_daemon();
	Option<Arc<Dummy>> recv_qp_opt;
	Option<Arc<RecvEntries<entry_num>>> recv_rs_opt;
	int ctx = 0;

	do {
		recv_qp_opt = ctrl.registered_qps.query("client_qp");
		if (!recv_qp_opt.has_value()) {
			RDMA_LOG(INFO) << "Client QP not yet registered. Retrying count " << ++ctx;
			sleep(1);
		}
	} while (!recv_qp_opt.has_value());

	ctx = 0;
	do {
		recv_rs_opt = manager.reg_recv_entries.query("client_qp");
		if (!recv_rs_opt.has_value()) {
			RDMA_LOG(INFO) << "Client Recv entries not yet registered. Retrying count " << ++ctx;
			sleep(1);
		}
	} while (!recv_rs_opt.has_value());

	auto recv_qp = ctrl.registered_qps.query("client_qp").value();
	auto recv_rs = manager.reg_recv_entries.query("client_qp").value();

	return make_pair(recv_qp, recv_rs);
}

int main(int argc, char **argv) {
	gflags::ParseCommandLineFlags(&argc, &argv, true);

	RCtrl ctrl(FLAGS_port);
	RecvManager<entry_num> manager(ctrl);
	auto nic =
	  RNic::create(RNicInfo::query_dev_names().at(FLAGS_use_nic_idx)).value();
	RDMA_ASSERT(ctrl.opened_nics.reg(FLAGS_reg_mem_name, nic));
	RDMA_ASSERT(ctrl.opened_nics.reg(FLAGS_reg_ack_mem_name, nic));

	auto [recv_qp, recv_rs] = init_recv_queue(ctrl, nic, manager);
	RDMA_LOG(INFO) << "Client Recv entries registered. Ready to receive messages!";

	// Ensure client has setup ready for server to connect
	sleep(3);

	auto [send_qp, local_mr] = init_send_queue(nic);
	RDMA_LOG(INFO) << "rc server ready to send acknowledgements to the client!";

	u64 recv_cnt = 0;
	u64 send_cnt = 0;
	u16 last_recvd_cnt = 0;
	bool first_recv = true;

	// receive all msgs.
	bool terminate = false;
	while (!terminate) {
		for (RecvIter<Dummy, entry_num> iter(recv_qp, recv_rs); iter.has_msgs(); iter.next()) {
			auto imm_msg = iter.cur_msg().value();
			u32 num_msg = std::get<0>(imm_msg);
			u32 *count = &num_msg;
			if (*count == 0) {
				// termination signal received
				terminate = true;
				break;
			}
			auto buf = static_cast<char *>(std::get<1>(imm_msg));
			const std::string msg(buf, FLAGS_msg_size);  // wrap the received msg
			recv_cnt++;

			u16 received_cnt = *count;

			if (first_recv) {
				RDMA_ASSERT(received_cnt == (u16) 1);
				last_recvd_cnt = received_cnt;
				send_cnt = received_cnt;;
				first_recv = false;
			} else if ((last_recvd_cnt == UINT16_MAX && received_cnt == (u16) 0) || last_recvd_cnt + 1 == received_cnt) {
				send_cnt++;
				last_recvd_cnt = received_cnt;
			} else {
				RDMA_ASSERT((u32) received_cnt != (u32) last_recvd_cnt + 1);
			}

			RDMA_ASSERT(recv_cnt == send_cnt);

			// send acknowledgement
			char* ack_buf = (char *) local_mr->get_reg_attr().value().buf;
			auto res_s = send_qp->send_normal(
				{.op = IBV_WR_SEND_WITH_IMM,
				 .flags = IBV_SEND_SIGNALED,
				 .len = 0,
				 .wr_id = 0},
				{.local_addr = reinterpret_cast<RMem::raw_ptr_t>(buf),
				 .remote_addr = 0,
				 .imm_data = recv_cnt}); // message size 0, counter 0 => terminate
			RDMA_ASSERT(res_s == IOCode::Ok);
			auto res_p = send_qp->wait_rc_comp(); // confirming that message was sent successfully
			RDMA_ASSERT(res_p == IOCode::Ok);
		}
		RDMA_LOG(INFO) << recv_cnt <<", " <<  send_cnt;
	}
	RDMA_LOG(INFO) << "Server shutting down";

	return 0;
}
