#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include <unistd.h>
#include <getopt.h> // For parsing command-line options
#include <limits.h> // For INT_MAX and INT_MIN

#include "rlibv2/c_wrappers/rdmaio_lib_c.h"
#include "rlibv2/c_wrappers/rdmaio_rc_c.h"
#include "rlibv2/c_wrappers/rdmaio_rctrl_c.h"
#include "rlibv2/c_wrappers/rdmaio_recv_iter_c.h"
#include "rlibv2/c_wrappers/rdmaio_regattr_c.h"
#include "rlibv2/c_wrappers/rdmaio_result_c.h"
#include "rlibv2/c_wrappers/rdmaio_rc_recv_manager_c.h"
#include "rlibv2/c_wrappers/rdmaio_mem_c.h"
#include "rlibv2/c_wrappers/rdmaio_reg_handler_c.h"
#include "rlibv2/c_wrappers/rdmaio_impl_c.h"
#include "rlibv2/c_wrappers/rdmaio_qp_c.h"

// Flags (equivalent to gflags)
char* FLAGS_addr = "192.168.252.212:8888";
long FLAGS_port = 8888;
long FLAGS_use_nic_idx = 0;
long FLAGS_reg_mem_name = 73;
long FLAGS_reg_ack_mem_name = 146;
char* FLAGS_cq_name = "test_channel";
char* FLAGS_ack_cq_name = "ack_channel";
int FLAGS_buffer_size = 1024 * 1024 * 1024;
int FLAGS_ack_buffer_size = 1024;
int FLAGS_msg_size = 1024;

// Constant for entry_num
const int entry_num = 256;

void init_recv_queue(rdmaio_rctrl_t* ctrl, rdmaio_nic_t* nic, rdmaio_recv_manager_t* manager, rdmaio_qp_t** recv_qp, recv_entries_handle_t** recv_rs) {
	if (!ctrl || !nic || !manager) {
		fprintf(stderr, "Null arguments passed to init_recv_queue.\n");
		if (recv_qp) {
			*recv_qp = NULL;
		}
		if (recv_rs) {
			*recv_rs = NULL;
		}
		return;
	}

	char cq_err_msg[256];
	struct ibv_cq* recv_cq = rdmaio_create_cq(nic, entry_num, cq_err_msg, sizeof(cq_err_msg));
	if (!recv_cq) {
		fprintf(stderr, "Failed to create receive CQ: %s\n", cq_err_msg);
		if (recv_qp) {
			*recv_qp = NULL;
		}
		if (recv_rs) {
			*recv_rs = NULL;
		}
		return;
	}

	rdmaio_rmem_t* mem = rmem_create(FLAGS_buffer_size);
	rdmaio_reg_handler_t* handler = rdmaio_reg_handler_create(mem, nic);
	rdmaio_regattr_t reg_attr = rdmaio_reg_handler_get_attr(handler);

	simple_allocator_t* allocator = simple_allocator_create(mem, reg_attr.rkey);
	if (!allocator) {
		fprintf(stderr, "Failed to create SimpleAllocator.\n");
		rdmaio_reg_handler_destroy(handler);
		rmem_destroy(mem);
		rdmaio_destroy_cq(recv_cq);
		if (recv_qp) {
			*recv_qp = NULL;
		}
		if (recv_rs) {
			*recv_rs = NULL;
		}
		return;
	}

	if (!recv_manager_reg_recv_cq(manager, FLAGS_cq_name, recv_cq, allocator)) {
		fprintf(stderr, "Failed to register receive CQ with RecvManager.\n");
		simple_allocator_destroy(allocator);
		rdmaio_reg_handler_destroy(handler);
		rmem_destroy(mem);
		rdmaio_destroy_cq(recv_cq);
		if (recv_qp) {
			*recv_qp = NULL;
		}
		if (recv_rs) {
			*recv_rs = NULL;
		}
		return;
	}

	if (!rdmaio_rctrl_register_mr(ctrl, FLAGS_reg_mem_name, handler)) {
		fprintf(stderr, "Failed to register memory region with RCtrl.\n");
		simple_allocator_destroy(allocator);
		rdmaio_reg_handler_destroy(handler);
		rmem_destroy(mem);
		rdmaio_destroy_cq(recv_cq);
		if (recv_qp) {
			*recv_qp = NULL;
		}
		if (recv_rs) {
			*recv_rs = NULL;
		}
		return;
	}

	if (!rctrl_start_daemon(ctrl)) {
		fprintf(stderr, "Failed to start RCtrl daemon.\n");
		simple_allocator_destroy(allocator);
		rdmaio_reg_handler_destroy(handler);
		rmem_destroy(mem);
		rdmaio_destroy_cq(recv_cq);
		if (recv_qp) {
			*recv_qp = NULL;
		}
		if (recv_rs) {
			*recv_rs = NULL;
		}
		return;
	}

	rdmaio_qp_t* client_qp = rctrl_query_qp(ctrl, "client_qp");
	while (!client_qp) {
		fprintf(stderr, "Client QP not yet registered. Retrying...\n");
		sleep(1);
		client_qp = rctrl_query_qp(ctrl, "client_qp");
	}

	recv_entries_handle_t* client_recv_rs = recv_manager_query_recv_entries(manager, "client_qp");
	while (!client_recv_rs) {
		fprintf(stderr, "Client Recv entries not yet registered. Retrying...\n");
		sleep(1);
		client_recv_rs = recv_manager_query_recv_entries(manager, "client_qp");
	}

	if (recv_qp) {
		*recv_qp = client_qp;
	}
	if (recv_rs) {
		*recv_rs = client_recv_rs;
	}

	simple_allocator_destroy(allocator);
	rdmaio_reg_handler_destroy(handler);
	rmem_destroy(mem);
	rdmaio_destroy_cq(recv_cq);

	fprintf(stderr, "Receive queue initialized (client QP and recv entries obtained).\n");
}

void init_send_queue(rdmaio_nic_t* nic, rdmaio_rc_t** qp_out, rdmaio_reg_handler_t** local_mr_out) {
	// 1. create the local QP to send acknowledgements
	rdmaio_qpconfig_t* qp_config = rdmaio_qpconfig_create_default();
	rdmaio_rc_t* qp = rdmaio_rc_create(nic, qp_config, NULL);
	rdmaio_qpconfig_destroy(qp_config);

	rdmaio_connect_manager_t* cm = rdmaio_connect_manager_create(FLAGS_addr);
	rdmaio_iocode_t wait_res = rdmaio_connect_manager_wait_ready(cm, 1.0, 4);
	if (wait_res == RDMAIO_TIMEOUT) {
		fprintf(stderr, "Connect timeout!\n");
		rdmaio_connect_manager_destroy(cm);
		rdmaio_rc_destroy(qp);
		*qp_out = NULL;
		*local_mr_out = NULL;
		return;
	}
	sleep(1);
	rdmaio_qpconfig_t* qp_config_connect = rdmaio_qpconfig_create_default();
	rdmaio_iocode_t qp_res = rdmaio_connect_manager_cc_rc_msg(cm, "server_qp", FLAGS_ack_cq_name,
																1, qp, FLAGS_reg_ack_mem_name, qp_config_connect);
	if (qp_res != RDMAIO_OK) {
		fprintf(stderr, "Failed to connect RC QP: %d\n", qp_res);
		rdmaio_connect_manager_destroy(cm);
		rdmaio_rc_destroy(qp);
		*qp_out = NULL;
		*local_mr_out = NULL;
		return;
	}
	rdmaio_regattr_t remote_attr;
	rdmaio_iocode_t fetch_res = rdmaio_connect_manager_fetch_remote_mr(cm, FLAGS_reg_ack_mem_name, &remote_attr);
	if (fetch_res != RDMAIO_OK) {
		fprintf(stderr, "Failed to fetch remote MR: %d\n", fetch_res);
		rdmaio_connect_manager_destroy(cm);
		rdmaio_rc_destroy(qp);
		*qp_out = NULL;
		*local_mr_out = NULL;
		return;
	}
	rdmaio_rmem_t* local_rmem = rmem_create(FLAGS_ack_buffer_size);
	rdmaio_reg_handler_t* reg_handler = rdmaio_reg_handler_create(local_rmem, nic);
	rdmaio_regattr_t local_mr_attr = rdmaio_reg_handler_get_attr(reg_handler);

	rdmaio_connect_manager_destroy(cm);

	rdmaio_rc_bind_remote_mr(qp, &remote_attr);
	rdmaio_rc_bind_local_mr(qp, &local_mr_attr);

	*qp_out = qp;
	*local_mr_out = reg_handler;

	rdmaio_qpconfig_destroy(qp_config_connect); // Free the allocated config
}

void print_usage(const char *program_name) {
	fprintf(stderr, "Usage: %s [options]\n", program_name);
	fprintf(stderr, "Options:\n");
	fprintf(stderr, "  --addr <ip:port>      Server IP address (default: %s)\n", FLAGS_addr);
	fprintf(stderr, "  --port <int>          Server listener (UDP) port (default: %ld)\n", FLAGS_port);
	fprintf(stderr, "  --use_nic_idx <int>   Which NIC to create QP (default: %ld)\n", FLAGS_use_nic_idx);
	fprintf(stderr, "  --reg_mem_name <int>  The name to register an MR at rctrl (default: %ld)\n", FLAGS_reg_mem_name);
	fprintf(stderr, "  --reg_ack_mem_name <int> The name to register the ack MR at rctrl (default: %ld)\n", FLAGS_reg_ack_mem_name);
	fprintf(stderr, "  --cq_name <string>    The name to register an receive cq (default: %s)\n", FLAGS_cq_name);
	fprintf(stderr, "  --ack_cq_name <string> The name to register an acknowledgement cq (default: %s)\n", FLAGS_ack_cq_name);
	fprintf(stderr, "  --buffer_size <int>   Total buffer size (default: %d)\n", FLAGS_buffer_size);
	fprintf(stderr, "  --ack_buffer_size <int> Buffer for ack messages (default: %d)\n", FLAGS_ack_buffer_size);
	fprintf(stderr, "  --msg_size <int>      Size of each message to send (default: %d)\n", FLAGS_msg_size);
	fprintf(stderr, "  --help                Print this usage information\n");
}

int main(int argc, char **argv) {
	int option;
	int long_index = 0;
	static struct option long_options[] = {
		{"addr", required_argument, 0, 'a'},
		{"port", required_argument, 0, 'p'},
		{"use_nic_idx", required_argument, 0, 'n'},
		{"reg_mem_name", required_argument, 0, 'm'},
		{"reg_ack_mem_name", required_argument, 0, 'k'},
		{"cq_name", required_argument, 0, 'c'},
		{"ack_cq_name", required_argument, 0, 'd'},
		{"buffer_size", required_argument, 0, 'b'},
		{"ack_buffer_size", required_argument, 0, 'e'},
		{"msg_size", required_argument, 0, 's'},
		{"help", no_argument, 0, 'h'},
		{NULL, 0, NULL, 0}
	};

	while ((option = getopt_long(argc, argv, "a:p:n:m:k:c:d:b:e:s:h", long_options, &long_index)) != -1) {
		long temp_long; // Temporary variable to hold the long value from strtol
		switch (option) {
			case 'a':
				FLAGS_addr = optarg;
				break;
			case 'p':
				FLAGS_port = strtol(optarg, NULL, 10);
				break;
			case 'n':
				FLAGS_use_nic_idx = strtol(optarg, NULL, 10);
				break;
			case 'm':
				FLAGS_reg_mem_name = strtol(optarg, NULL, 10);
				break;
			case 'k':
				FLAGS_reg_ack_mem_name = strtol(optarg, NULL, 10);
				break;
			case 'c':
				FLAGS_cq_name = optarg;
				break;
			case 'd':
				FLAGS_ack_cq_name = optarg;
				break;
		   case 'b':
			   temp_long = strtol(optarg, NULL, 10);
			   if (temp_long > INT_MAX || temp_long < INT_MIN) {
				  fprintf(stderr, "Error: --buffer_size value is out of the valid range for an integer.\n");
				  return 1;
			   }
			   FLAGS_buffer_size = (int)temp_long;
			   break;
		   case 'e':
			   temp_long = strtol(optarg, NULL, 10);
			   if (temp_long > INT_MAX || temp_long < INT_MIN) {
				  fprintf(stderr, "Error: --ack_buffer_size value is out of the valid range for an integer.\n");
				  return 1;
			   }
			   FLAGS_ack_buffer_size = (int)temp_long;
			   break;
		   case 's':
			   temp_long = strtol(optarg, NULL, 10);
			   if (temp_long > INT_MAX || temp_long < INT_MIN) {
				  fprintf(stderr, "Error: --msg_size value is out of the valid range for an integer.\n");
				  return 1;
			   }
			   FLAGS_msg_size = (int)temp_long;
			   break;
			case 'h':
				print_usage(argv[0]);
				return 0;
			case '?':
				print_usage(argv[0]);
				return 1;
			default:
				fprintf(stderr, "Unknown option\n");
				print_usage(argv[0]);
				return 1;
		}
	}

	rdmaio_rctrl_t* ctrl = rctrl_create(FLAGS_port, NULL);
	if (!ctrl) {
		fprintf(stderr, "Failed to create RCtrl.\n");
		return 1;
	}

	rdmaio_recv_manager_t* manager = recv_manager_create(ctrl);
	if (!manager) {
		fprintf(stderr, "Failed to create recv manager.\n");
		rctrl_destroy(ctrl);
		return 1;
	}

	size_t count = 0;
	rdmaio_devidx_t* dev_idx_array = rnic_info_query_dev_names(&count);
	if (!dev_idx_array || count == 0) {
		fprintf(stderr, "No devices found.\n");
		recv_manager_destroy(manager);
		rctrl_destroy(ctrl);
		return 1;
	}

	if (FLAGS_use_nic_idx >= count) {
		fprintf(stderr, "Invalid nic index specified.\n");
		rnic_info_free_dev_names(dev_idx_array);
		recv_manager_destroy(manager);
		rctrl_destroy(ctrl);
		return 1;
	}

	rdmaio_devidx_t selected_dev_idx = dev_idx_array[FLAGS_use_nic_idx];
	uint8_t gid = 0;
	rdmaio_nic_t* nic = rnic_create(selected_dev_idx, gid);
	if (!nic) {
		fprintf(stderr, "Failed to create RNic.\n");
		rnic_info_free_dev_names(dev_idx_array);
		recv_manager_destroy(manager);
		rctrl_destroy(ctrl);
		return 1;
	}

	if (!rctrl_register_nic(ctrl, (uint64_t)FLAGS_reg_mem_name, nic)) {
		fprintf(stderr, "Failed to register NIC with name %ld.\n", FLAGS_reg_mem_name);
		rnic_info_free_dev_names(dev_idx_array);
		rnic_destroy(nic);
		recv_manager_destroy(manager);
		rctrl_destroy(ctrl);
		return 1;
	}

	if (!rctrl_register_nic(ctrl, (uint64_t)FLAGS_reg_ack_mem_name, nic)) {
		fprintf(stderr, "Failed to register NIC with name %ld.\n", FLAGS_reg_ack_mem_name);
		rnic_info_free_dev_names(dev_idx_array);
		rnic_destroy(nic);
		recv_manager_destroy(manager);
		rctrl_destroy(ctrl);
		return 1;
	}

	rdmaio_qp_t* recv_qp = NULL;
	recv_entries_handle_t* recv_rs = NULL;
	init_recv_queue(ctrl, nic, manager, &recv_qp, &recv_rs);
	fprintf(stderr, "Client Recv entries registered. Ready to receive messages!\n");

	sleep(3); // Ensure client has setup ready

	rdmaio_rc_t* send_qp = NULL;
	rdmaio_reg_handler_t* local_mr = NULL;
	init_send_queue(nic, &send_qp, &local_mr);
	fprintf(stderr, "rc server ready to send acknowledgements to the client!\n");

	uint64_t recv_cnt = 0;
	uint64_t send_cnt = 0;
	uint16_t last_recvd_cnt = 0;
	bool first_recv = true;
	bool terminate = false;

	while (!terminate) {
		rdmaio_recv_iter_t* iter = rdmaio_recv_iter_create(recv_qp, recv_rs);
		if (iter) {
			while (rdmaio_recv_iter_has_msgs(iter)) {
				uint32_t imm_msg;
				uintptr_t buf_addr;
				if (rdmaio_recv_iter_cur_msg(iter, &imm_msg, &buf_addr)) {
					int received_cnt = (int) imm_msg;
					if (received_cnt == 0) {
						terminate = true;
						break;
					}
					if (received_cnt == -1) {
						recv_cnt = 0;
						send_cnt = 0;
						last_recvd_cnt = 0;
						first_recv = true;
						rdmaio_recv_iter_next(iter);
						continue;
					}
					recv_cnt++;

					if (first_recv) {
						if (received_cnt != 1) {
							fprintf(stderr, "Assertion failed: received_cnt == 1\n");
						}
						last_recvd_cnt = received_cnt;
						send_cnt = received_cnt;
						first_recv = false;
					} else {
						if (received_cnt != last_recvd_cnt + 1) {
							fprintf(stderr, "Assertion failed: received_cnt == last_recvd_cnt + 1\n");
						}
						send_cnt++;
						last_recvd_cnt = received_cnt;
					}

					if (recv_cnt != send_cnt) {
						fprintf(stderr, "Assertion failed: recv_cnt == send_cnt\n");
					}

					// send acknowledgement
					rdmaio_regattr_t local_attr = rdmaio_reg_handler_get_attr(local_mr);
					char* ack_buf = (char *)local_attr.addr;
					rdmaio_reqdesc_t send_desc;
					send_desc.op = IBV_WR_SEND_WITH_IMM;
					send_desc.flags = IBV_SEND_SIGNALED;
					send_desc.len = 0;
					send_desc.wr_id = 0;

					rdmaio_reqpayload_t send_payload;
					send_payload.local_addr = (uintptr_t)ack_buf;
					send_payload.remote_addr = 0;
					send_payload.imm_data = recv_cnt;

					char error_msg[256];
					int res_s = rdmaio_rc_send_normal(send_qp, &send_desc, &send_payload, error_msg, sizeof(error_msg));
					if (res_s != 0) {
						fprintf(stderr, "Error sending ack: %s\n", error_msg);
					}
					struct ibv_wc wc;
					int res_p = rdmaio_rc_wait_rc_comp(send_qp, NULL, &wc);
					if (res_p != 0) {
						fprintf(stderr, "Error waiting for ack send completion: %d\n", res_p);
					}
				} else {
					fprintf(stderr, "Error getting current message.\n");
				}
				rdmaio_recv_iter_next(iter);
			}
			rdmaio_recv_iter_destroy(iter);
		} else {
			fprintf(stderr, "Error creating recv iterator in main loop.\n");
			break;
		}
		fprintf(stderr, "Received count: %lu, Sent count: %lu\n", recv_cnt, send_cnt);
		if (terminate) {
			break;
		}
	}

	fprintf(stderr, "Server shutting down\n");

	rnic_info_free_dev_names(dev_idx_array);
	rnic_destroy(nic);
	rdmaio_rc_destroy(send_qp); // Use rdmaio_rc_destroy for QPs created with rdmaio_rc_create
	rdmaio_reg_handler_destroy(local_mr);
	// rdmaio_destroy_qp(recv_qp); // Receive QP obtained from RCtrl, do not destroy
	recv_manager_destroy(manager);
	rctrl_destroy(ctrl);

	return 0;
}