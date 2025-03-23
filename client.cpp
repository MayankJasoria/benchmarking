#include <gflags/gflags.h>
#include "rlibv2/core/lib.hh"
#include "rlibv2/core/qps/rc_recv_manager.hh"
#include "rlibv2/core/qps/recv_iter.hh"
#include <iostream>
#include <fstream>
#include <string>
#include <random>
#include <chrono>

DEFINE_string(addr, "192.168.252.211:8888", "Server IP address");
DEFINE_int64(port, 8888, "Client listener (UDP) port.");
DEFINE_int64(use_nic_idx, 0, "The nic to connect QP to");
DEFINE_int64(reg_mem_name, 73, "The name to register an MR at rctrl");
DEFINE_int64(reg_ack_mem_name, 146, "The name to register the ack MR at rctrl");
DEFINE_string(cq_name, "test_channel", "The name to register an receive cq");
DEFINE_string(ack_cq_name, "ack_channel", "The name to register an acknowledgement cq");
DEFINE_int32(buffer_size, 1024*1024*1024, "Total buffer size");
DEFINE_int32(ack_buffer_size, 1024, "Buffer for ack messages");
DEFINE_int32(msg_size, 1024, "Size of each message to send");
DEFINE_int32(max_msg_size, 4*1024*1024, "Maximum memory size");
DEFINE_int32(msg_count, 1000, "Number of messages to send");

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

std::string generateRandomString(size_t numBytes) {
    const char charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    const size_t charsetSize = sizeof(charset) - 1;
    std::string result;
    result.reserve(numBytes);

    std::random_device rd;
    std::mt19937 generator(rd());
    std::uniform_int_distribution<> distribution(0, charsetSize - 1);

    for (size_t i = 0; i < numBytes; ++i) {
        result += charset[distribution(generator)];
    }

    return result;
}

/**
 * Initializes the RDMA setup for use
 * @return pair containing the local QP and the remote memory buffer
 */
pair<Arc<RC>, Arc<RegHandler>> init_send_queue(Arc<RNic> &nic) {
    // 1. create the local QP to send
    auto qp = RC::create(nic, QPConfig()).value();

    ConnectManager cm(FLAGS_addr);
    if (cm.wait_ready(100000, 4) ==
        IOCode::Timeout) {  // wait 1 second for server to ready, retry 2 times
            RDMA_LOG(WARNING) << "connect to the " << FLAGS_addr << " timeout!";
	}

    sleep(1);
    // 2. create the remote QP and connect
    auto qp_res = cm.cc_rc_msg("client_qp", FLAGS_cq_name,
    	FLAGS_max_msg_size, qp, FLAGS_reg_mem_name, QPConfig());
    RDMA_ASSERT(qp_res == IOCode::Ok) << std::get<0>(qp_res.desc);

    // 3. fetch the remote MR for usage
    auto fetch_res = cm.fetch_remote_mr(FLAGS_reg_mem_name);
    RDMA_ASSERT(fetch_res == IOCode::Ok) << std::get<0>(fetch_res.desc);
    rmem::RegAttr remote_attr = std::get<1>(fetch_res.desc);

    // 4. register a local buffer for sending messages
    auto local_mr = RegHandler::create(Arc<RMem>(new RMem(FLAGS_buffer_size)), nic).value();


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
	  Arc<RMem>(new RMem(static_cast<const rdmaio::u64>(FLAGS_ack_buffer_size)));
	auto handler = RegHandler::create(mem, nic).value();
	auto alloc = std::make_shared<SimpleAllocator>(
	  mem, handler->get_reg_attr().value().key);

	// 3. register receive cq
	manager.reg_recv_cqs.create_then_reg(FLAGS_ack_cq_name, recv_cq, alloc);
	RDMA_LOG(EMPH) << "Register ack_channel";
	ctrl.registered_mrs.reg(FLAGS_reg_ack_mem_name, handler);

	ctrl.start_daemon();
	Option<Arc<Dummy>> recv_qp_opt;
	Option<Arc<RecvEntries<entry_num>>> recv_rs_opt;
	int ctx = 0;

	do {
		recv_qp_opt = ctrl.registered_qps.query("server_qp");
		if (!recv_qp_opt.has_value()) {
			RDMA_LOG(INFO) << "Server QP not yet registered. Retrying count " << ++ctx;
			sleep(1);
		}
	} while (!recv_qp_opt.has_value());

	ctx = 0;
	do {
		recv_rs_opt = manager.reg_recv_entries.query("server_qp");
		if (!recv_rs_opt.has_value()) {
			RDMA_LOG(INFO) << "Serv Recv entries not yet registered. Retrying count " << ++ctx;
			sleep(1);
		}
	} while (!recv_rs_opt.has_value());

	auto recv_qp = ctrl.registered_qps.query("server_qp").value();
	auto recv_rs = manager.reg_recv_entries.query("server_qp").value();

	return make_pair(recv_qp, recv_rs);
}

/**
 * Writes a given string to remote memory
 * @param qp RDMA Queue Pair
 * @param buf Local buffer
 * @param msg Test data to be written to remote memory
 * @param imm_counter_val Message number
 * @return Time taken to perform the write
 */
void publish_messages_and_receive_ack(const Arc<RC> &qp, const Arc<RegHandler> &local_mr, const string &msg, u32 imm_counter_val,
	long *arr, shared_ptr<Dummy> &recv_qp, shared_ptr<RecvEntries<entry_num>> &recv_rs) {
	static size_t current_offset = 0;
	static char *base_buf = nullptr;
	static size_t total_buffer_size = 0;

	if (!base_buf) {
		base_buf = (char *)(local_mr->get_reg_attr().value().buf);
		total_buffer_size = FLAGS_buffer_size;
		RDMA_LOG(DEBUG) << "Base buffer address: " << (void*)base_buf << ", total size: " << total_buffer_size;
	}

	size_t msg_len_with_null = msg.size() + 1;

	// Check if there is enough space remaining
	if (current_offset + msg_len_with_null > total_buffer_size) {
		// Cycle back to the start
		current_offset = 0;
		RDMA_LOG(DEBUG) << "Cycling back to the start of the buffer. Current offset: " << current_offset;
	}

	char* new_buf = base_buf + current_offset;
	memset(new_buf, 0, msg.size() + 1);
	memcpy(new_buf, msg.data(), msg.size());

	auto start = std::chrono::high_resolution_clock::now();
	auto res_s = qp->send_normal(
		{.op = IBV_WR_SEND_WITH_IMM,
		 .flags = IBV_SEND_SIGNALED,
		 .len = (u32) msg.size() + 1,
		 .wr_id = 0},
		{.local_addr = reinterpret_cast<RMem::raw_ptr_t>(new_buf),
		 .remote_addr = 0,
		 .imm_data = imm_counter_val});

	RDMA_ASSERT(res_s == IOCode::Ok);
	auto before_wait = std::chrono::high_resolution_clock::now();
	auto res_p = qp->wait_rc_comp();
	RDMA_ASSERT(res_p == IOCode::Ok);
	auto after_wait = std::chrono::high_resolution_clock::now();

	chrono::time_point<chrono::system_clock, chrono::system_clock::duration> after_ack;
	// receive acknowledgement from server
	bool ack = false;
	while (!ack) {
		// for loop is actually not needed, loop will only run once!
		for (RecvIter<Dummy, entry_num> iter(recv_qp, recv_rs); iter.has_msgs(); iter.next()) {
			ack = true;
			after_ack = std::chrono::high_resolution_clock::now();
			auto imm_msg = iter.cur_msg().value();
			u32 num_msg = std::get<0>(imm_msg);
			u32 *count = &num_msg;
			RDMA_ASSERT(*count == imm_counter_val);
		}
	}

	long before_wait_nsec = chrono::duration_cast<chrono::nanoseconds>(before_wait - start).count();
	long after_wait_nsec = chrono::duration_cast<chrono::nanoseconds>(after_wait - start).count();
	long after_ack_nsec = chrono::duration_cast<chrono::nanoseconds>(after_ack - start).count();
	arr[0] = before_wait_nsec;
	arr[1] = after_wait_nsec;
	arr[2] = after_ack_nsec;
}

void send_termination(const Arc<RC> &qp, const Arc<RegHandler> &local_mr) {
	// don't care about contents
	char* buf = (char *) local_mr->get_reg_attr().value().buf;
	auto res_s = qp->send_normal(
		{.op = IBV_WR_SEND_WITH_IMM,
		 .flags = IBV_SEND_SIGNALED,
		 .len = 0,
		 .wr_id = 0},
		{.local_addr = reinterpret_cast<RMem::raw_ptr_t>(buf),
		 .remote_addr = 0,
		 .imm_data = 0}); // message size 0, counter 0 => terminate
	RDMA_ASSERT(res_s == IOCode::Ok);
	auto res_p = qp->wait_rc_comp(); // confirming that message was sent successfully
	RDMA_ASSERT(res_p == IOCode::Ok);
}

void send_reset(const Arc<RC> &qp, const Arc<RegHandler> &local_mr) {
	// don't care about contents
	char* buf = (char *) local_mr->get_reg_attr().value().buf;
	auto res_s = qp->send_normal(
		{.op = IBV_WR_SEND_WITH_IMM,
		 .flags = IBV_SEND_SIGNALED,
		 .len = 0,
		 .wr_id = 0},
		{.local_addr = reinterpret_cast<RMem::raw_ptr_t>(buf),
		 .remote_addr = 0,
		 .imm_data = static_cast<u64>(-1)}); // message size 0, counter 0 => terminate
	RDMA_ASSERT(res_s == IOCode::Ok);
	auto res_p = qp->wait_rc_comp(); // confirming that message was sent successfully
	RDMA_ASSERT(res_p == IOCode::Ok);
}

void writeResultsToFile(const std::vector<long*>& times, int msg_size) {
	// Construct the output file name
	std::string filename = "/hdd2/rdma-libs/results/rdma_send_recv_" + std::to_string(msg_size) + ".txt";
	std::ofstream outputFile(filename);

	// Check if the file was opened successfully
	if (outputFile.is_open()) {
		// Write the header row
		outputFile << "before wait\tafter wait\trtt\n";

		// Write the data from the 'times' vector
		for (const long* arr : times) {
			outputFile << arr[0] << "\t" << arr[1] << "\t" << arr[2] << "\n";
		}

		// Close the file
		outputFile.close();
		std::cout << "Data written to: " << filename << std::endl;
	} else {
		std::cerr << "Unable to open file: " << filename << std::endl;
	}
}

int main(int argc, char **argv) {
	gflags::ParseCommandLineFlags(&argc, &argv, true);

	RCtrl ctrl(FLAGS_port);
	RecvManager<entry_num> manager(ctrl);
	auto nic =
		RNic::create(RNicInfo::query_dev_names().at(FLAGS_use_nic_idx)).value();
	RDMA_ASSERT(ctrl.opened_nics.reg(FLAGS_reg_mem_name, nic));
	RDMA_ASSERT(ctrl.opened_nics.reg(FLAGS_reg_ack_mem_name, nic));

	auto [qp, local_mr] = init_send_queue(nic);
	RDMA_LOG(INFO) << "rc client ready to send message to the server!";

	auto [recv_qp, recv_rs] = init_recv_queue(ctrl, nic, manager);
	RDMA_LOG(INFO) << "rc client ready to receive acknowledgements from the server!";

	int num_bytes = FLAGS_msg_size;
	int message_count = 0;

	RDMA_LOG(INFO) << "Sending 1000 messages of size " << num_bytes << " for warmup.";

	int saved_msgs_count = max(1000, FLAGS_msg_count); // ensure there are sufficient random messages
	string saved_msgs[saved_msgs_count];
	for (int i = 0; i < saved_msgs_count; ++i) {
		saved_msgs[i] = generateRandomString(num_bytes);
	}

	/* warm up run here */
	int warm_up_msgs = 1000;
	for (int i = 0; i < warm_up_msgs; ++i) {
		long arr[3]; // we don't care about the returned values in warm up.

		string msg = saved_msgs[i];
		publish_messages_and_receive_ack(qp, local_mr, msg, ++message_count, arr, recv_qp, recv_rs);
		// ignore these times
	}

	send_reset(qp, local_mr);

	/* test run */
	int num_msgs = FLAGS_msg_count;
	vector<long *> times(num_msgs);
	message_count = 0;
	RDMA_LOG(INFO) << "Sending " << num_msgs << " messages of size " << num_bytes << " for test.";
	for (int i = 0; i < num_msgs; ++i) {
		long *arr = new long[3];

		string msg = saved_msgs[i];
		publish_messages_and_receive_ack(qp, local_mr, msg, ++message_count, arr, recv_qp, recv_rs);
		times[i] = arr;
	}

	RDMA_LOG(INFO) << "Number of messages: " << message_count;
	writeResultsToFile(times, num_bytes);

	RDMA_LOG(INFO) << "Sending terminate signal to server";
	send_termination(qp, local_mr);

	RDMA_LOG(INFO) << "Terminating client";
	for (long* arr : times) {
		delete arr;
	}
	times.clear();

	return 0;
}