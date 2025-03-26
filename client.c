#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>

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

#include <unistd.h>

// Placeholder for gflags values
char* FLAGS_addr = "192.168.252.211:8888";
long FLAGS_port = 8888;
long FLAGS_use_nic_idx = 0;
long FLAGS_reg_mem_name = 73;
long FLAGS_reg_ack_mem_name = 146;
char* FLAGS_cq_name = "test_channel";
char* FLAGS_ack_cq_name = "ack_channel";
int FLAGS_buffer_size = 1024 * 1024 * 1024;
int FLAGS_ack_buffer_size = 1024;
int FLAGS_msg_size = 1024;
int FLAGS_max_msg_size = 4 * 1024 * 1024;
int FLAGS_msg_count = 1000;

// Placeholder for entry_num (assuming it's a constant)
const int entry_num = 128;

char* generateRandomString(size_t numBytes) {
	const char* charset= "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
	const size_t charsetSize = sizeof(charset) - 1;
	char* result = (char*)malloc(numBytes + 1); // Allocate memory for the string + null terminator

	if (result == NULL) {
		fprintf(stderr, "Failed to allocate memory for random string.\n");
		return NULL;
	}

	// Seed the random number generator if it hasn't been seeded yet in this execution
	static int seeded = 0;
	if (!seeded) {
		srand(time(NULL));
		seeded = 1;
	}

	for (size_t i = 0; i < numBytes; ++i) {
		size_t randomIndex = (double)rand() / RAND_MAX * (charsetSize - 1);
		result[i] = charset[randomIndex];
	}

	result[numBytes] = '\0'; // Null-terminate the string
	return result;
}

// Implementation for init_send_queue function (Corrected)
void init_send_queue(rdmaio_nic_t* nic, rdmaio_rc_t** qp_out, rdmaio_reg_handler_t** local_mr_out) {
    // 1. create the local QP to send
    rdmaio_qpconfig_t* qp_config = rdmaio_qpconfig_create_default();
    if (!qp_config) {
        fprintf(stderr, "Failed to create default QP config.\n");
        *qp_out = NULL;
        *local_mr_out = NULL;
        return;
    }
    rdmaio_rc_t* qp = rdmaio_rc_create(nic, qp_config, NULL); // Pass the created config
    rdmaio_qpconfig_destroy(qp_config); // Destroy the config after passing it

    if (!qp) {
        fprintf(stderr, "Failed to create RC QP.\n");
        *qp_out = NULL;
        *local_mr_out = NULL;
        return;
    }

    // 2. create the ConnectManager and connect
    rdmaio_connect_manager_t* cm = rdmaio_connect_manager_create(FLAGS_addr);
    if (!cm) {
        fprintf(stderr, "Failed to create ConnectManager.\n");
        rdmaio_rc_destroy(qp);
        *qp_out = NULL;
        *local_mr_out = NULL;
        return;
    }

    rdmaio_iocode_t wait_res = rdmaio_connect_manager_wait_ready(cm, 1.0, 4); // Timeout in seconds
    if (wait_res == RDMAIO_TIMEOUT) {
        fprintf(stderr, "Connect to the %s timeout!\n", FLAGS_addr);
        rdmaio_connect_manager_destroy(cm);
        rdmaio_rc_destroy(qp);
        *qp_out = NULL;
        *local_mr_out = NULL;
        return;
    }

    sleep(1);

    rdmaio_qpconfig_t* qp_config_connect = rdmaio_qpconfig_create_default();
    rdmaio_iocode_t qp_res = rdmaio_connect_manager_cc_rc_msg(cm, "client_qp", FLAGS_cq_name,
                                                               FLAGS_max_msg_size, qp, (uint32_t)FLAGS_use_nic_idx, qp_config_connect);
    if (qp_res != RDMAIO_OK) {
        fprintf(stderr, "Failed to connect RC QP: %d\n", qp_res);
        rdmaio_connect_manager_destroy(cm);
        rdmaio_rc_destroy(qp);
        *qp_out = NULL;
        *local_mr_out = NULL;
        return;
    }

    // 3. fetch the remote MR for usage
    rdmaio_regattr_t remote_attr;
    rdmaio_iocode_t fetch_res = rdmaio_connect_manager_fetch_remote_mr(cm, FLAGS_reg_mem_name, &remote_attr);
    if (fetch_res != RDMAIO_OK) {
        fprintf(stderr, "Failed to fetch remote MR: %d\n", fetch_res);
        rdmaio_connect_manager_destroy(cm);
        rdmaio_rc_destroy(qp);
        *qp_out = NULL;
        *local_mr_out = NULL;
        return;
    }

    // 4. register a local buffer for sending messages
    rdmaio_rmem_t* local_rmem = rmem_create(FLAGS_buffer_size);
    if (!local_rmem) {
        fprintf(stderr, "Failed to create local RMem.\n");
        rdmaio_connect_manager_destroy(cm);
        rdmaio_rc_destroy(qp);
        *qp_out = NULL;
        *local_mr_out = NULL;
        return;
    }
    rdmaio_reg_handler_t* reg_handler = rdmaio_reg_handler_create(local_rmem, nic);
    if (!reg_handler) {
        fprintf(stderr, "Failed to register local memory.\n");
        rmem_destroy(local_rmem);
        rdmaio_connect_manager_destroy(cm);
        rdmaio_rc_destroy(qp);
        *qp_out = NULL;
        *local_mr_out = NULL;
        return;
    }
    rdmaio_regattr_t local_mr_attr = rdmaio_reg_handler_get_attr(reg_handler);

    rdmaio_connect_manager_destroy(cm);

    rdmaio_rc_bind_remote_mr(qp, &remote_attr);
    rdmaio_rc_bind_local_mr(qp, &local_mr_attr);

    *qp_out = qp;
    *local_mr_out = reg_handler; // Return the handler directly
}

void init_recv_queue(rdmaio_rctrl_t* ctrl, void* nic, void* manager, rdmaio_rc_t** recv_qp, void** recv_rs) {
    if (!ctrl || !nic || !manager) {
        fprintf(stderr, "Error: Null arguments passed to init_recv_queue.\n");
    	if (recv_qp) {
    		*recv_qp = NULL;
    	}
    	if (recv_rs) {
    		*recv_rs = NULL;
    	}
        return;
    }

    rdmaio_recv_manager_t* recv_manager = (rdmaio_recv_manager_t*)manager;

    // 1. create receive cq
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

    // 2. prepare the message buffer with allocator
    rdmaio_rmem_t* mem = rmem_create(FLAGS_ack_buffer_size);
    if (!mem) {
        fprintf(stderr, "Failed to create RMem for receive buffer.\n");
        rdmaio_destroy_cq(recv_cq);
    	if (recv_qp) {
    		*recv_qp = NULL;
    	}
    	if (recv_rs) {
    		*recv_rs = NULL;
    	}
        return;
    }
    rdmaio_reg_handler_t* handler = rdmaio_reg_handler_create(mem, nic);
    if (!handler) {
        fprintf(stderr, "Failed to create RegHandler for receive buffer.\n");
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
    rdmaio_regattr_t reg_attr = rdmaio_reg_handler_get_attr(handler);

    // 3. create SimpleAllocator
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

    // 4. register receive cq with RecvManager
    if (!recv_manager_reg_recv_cq(recv_manager, FLAGS_ack_cq_name, recv_cq, allocator)) {
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

    // 5. register memory region with RCtrl
    if (!rdmaio_rctrl_register_mr(ctrl, FLAGS_reg_ack_mem_name, &reg_attr, nic)) {
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

    // 6. start daemon
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

    // 7. Wait for Server QP
    rdmaio_rc_t* server_qp = NULL;
    int retry_count = 0;
    while (retry_count < 5) { // Limit retries to avoid infinite loop
        server_qp = rctrl_query_qp(ctrl, "server_qp");
        if (server_qp) {
            fprintf(stderr, "Found server QP.\n");
            break;
        }
        fprintf(stderr, "Server QP not yet registered. Retrying count %d\n", ++retry_count);
        sleep(1);
    }

    if (!server_qp) {
        fprintf(stderr, "Timeout waiting for server QP.\n");
        rctrl_stop_daemon(ctrl);
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

    // 8. Wait for Server Recv Entries
    void* server_recv_rs = recv_manager_query_recv_entries(recv_manager, "server_qp");
    int recv_rs_retry_count = 0;
    while (!server_recv_rs && recv_rs_retry_count < 5) {
        fprintf(stderr, "Server Recv entries not yet registered. Retrying count %d\n", ++recv_rs_retry_count);
        sleep(1);
        server_recv_rs = recv_manager_query_recv_entries(recv_manager, "server_qp");
    }

    if (!server_recv_rs) {
        fprintf(stderr, "Timeout waiting for server recv entries.\n");
        rctrl_stop_daemon(ctrl);
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

    // 9. Set output parameters
    if (recv_qp) {
    	*recv_qp = server_qp;
	}
    if (recv_rs) {
	    *recv_rs = server_recv_rs;
    }

    // Clean up locally created resources
    simple_allocator_destroy(allocator);
    rdmaio_reg_handler_destroy(handler);
    rmem_destroy(mem);
    rdmaio_destroy_cq(recv_cq);

    fprintf(stderr, "Receive queue initialized (server QP and recv entries obtained).\n");
}

/**
 * Writes a given string to remote memory
 * @param qp RDMA Queue Pair
 * @param local_mr Local buffer registration handler
 * @param msg Test data to be written to remote memory
 * @param imm_counter_val Message number
 * @param arr Array to store time taken
 * @param recv_qp Receive RDMA Queue Pair
 * @param recv_rs Received entries
 */
void publish_messages_and_receive_ack(rdmaio_rc_t* qp, rdmaio_reg_handler_t* local_mr, const char* msg, int imm_counter_val,
    long *arr, rdmaio_rc_t* recv_qp, void* recv_rs) {
    static size_t current_offset = 0;
    static char *base_buf = NULL;
    static size_t total_buffer_size = 0;

    if (!base_buf) {
        rdmaio_regattr_t attr = rdmaio_reg_handler_get_attr(local_mr);
        base_buf = (char *)attr.addr;
        total_buffer_size = FLAGS_buffer_size;
        printf("DEBUG: Base buffer address: %p, total size: %zu\n", base_buf, total_buffer_size);
    }

    size_t msg_len_with_null = strlen(msg) + 1;

    // Check if there is enough space remaining
    if (current_offset + msg_len_with_null > total_buffer_size) {
        // Cycle back to the start
        current_offset = 0;
        printf("DEBUG: Cycling back to the start of the buffer. Current offset: %zu\n", current_offset);
    }

    char* new_buf = base_buf + current_offset;
    memset(new_buf, 0, msg_len_with_null);
    memcpy(new_buf, msg, strlen(msg));

    struct timespec start, before_wait, after_wait, after_ack;
    clock_gettime(CLOCK_MONOTONIC_RAW, &start);

    rdmaio_reqdesc_t send_desc;
    send_desc.op = IBV_WR_SEND_WITH_IMM;
    send_desc.flags = IBV_SEND_SIGNALED;
    send_desc.len = msg_len_with_null;
    send_desc.wr_id = 0;

    rdmaio_regattr_t attr = rdmaio_reg_handler_get_attr(local_mr);
    rdmaio_reqpayload_t send_payload;
    send_payload.local_addr = (uintptr_t)new_buf;
    send_payload.remote_addr = 0;
    send_payload.imm_data = imm_counter_val;

    char error_msg[256];
    int res_s = rdmaio_rc_send_normal(qp, &send_desc, &send_payload, error_msg, sizeof(error_msg));
    if (res_s != 0) {
        fprintf(stderr, "Error sending message: %s\n", error_msg);
        return;
    }

    clock_gettime(CLOCK_MONOTONIC_RAW, &before_wait);

    struct ibv_wc wc;
    int res_p = rdmaio_rc_wait_rc_comp(qp, NULL, &wc); // Wait indefinitely
    if (res_p != 0) {
        fprintf(stderr, "Error waiting for send completion: %d\n", res_p);
        return;
    }

    clock_gettime(CLOCK_MONOTONIC_RAW, &after_wait);

	bool ack = false;
	while (!ack) {
		rdmaio_recv_iter_t* iter = rdmaio_recv_iter_create(recv_qp, recv_rs);
		if (!iter) {
			fprintf(stderr, "Error creating recv iterator.\n");
			break;
		}
		while (rdmaio_recv_iter_has_msgs(iter)) {
			ack = true;
			clock_gettime(CLOCK_MONOTONIC_RAW, &after_ack);
			uint32_t imm_msg;
			uintptr_t buf_addr;
			if (rdmaio_recv_iter_cur_msg(iter, &imm_msg, &buf_addr)) {
				if (imm_msg != imm_counter_val) {
					fprintf(stderr, "Acknowledgement message count mismatch: sent %d, received %u\n", imm_counter_val, imm_msg);
				}
			} else {
				fprintf(stderr, "Error getting current message from iterator.\n");
			}
			rdmaio_recv_iter_next(iter);
			break; // Assuming only one ack per message
		}
		rdmaio_recv_iter_destroy(iter);
	}

    long before_wait_nsec = (before_wait.tv_sec - start.tv_sec) * 1000000000LL + (before_wait.tv_nsec - start.tv_nsec);
    long after_wait_nsec = (after_wait.tv_sec - start.tv_sec) * 1000000000LL + (after_wait.tv_nsec - start.tv_nsec);
    long after_ack_nsec = (after_ack.tv_sec - start.tv_sec) * 1000000000LL + (after_ack.tv_nsec - start.tv_nsec);

    arr[0] = before_wait_nsec;
    arr[1] = after_wait_nsec;
    arr[2] = after_ack_nsec;

    current_offset += msg_len_with_null;
}

void send_reset(rdmaio_rc_t* qp, rdmaio_reg_handler_t* local_mr) {
	rdmaio_regattr_t attr = rdmaio_reg_handler_get_attr(local_mr);
	char* buf = (char *) attr.addr;

	rdmaio_reqdesc_t send_desc;
	send_desc.op = IBV_WR_SEND_WITH_IMM;
	send_desc.flags = IBV_SEND_SIGNALED;
	send_desc.len = 0;
	send_desc.wr_id = 0;

	rdmaio_reqpayload_t send_payload;
	send_payload.local_addr = (uintptr_t)buf;
	send_payload.remote_addr = 0;
	send_payload.imm_data = (uint64_t) -1;

	char error_msg[256];
	int res_s = rdmaio_rc_send_normal(qp, &send_desc, &send_payload, error_msg, sizeof(error_msg));
	if (res_s != 0) {
		fprintf(stderr, "Error sending termination message: %s\n", error_msg);
		return;
	}

	struct ibv_wc wc;
	int res_p = rdmaio_rc_wait_rc_comp(qp, NULL, &wc);
	if (res_p != 0) {
		fprintf(stderr, "Error waiting for termination send completion: %d\n", res_p);
	}
}

// Placeholder for writeResultsToFile function
void writeResultsToFile(long** times, int num_msgs, int num_bytes) {
    printf("Writing results to file (not implemented in placeholder).\n");
}

void send_termination(rdmaio_rc_t* qp, rdmaio_reg_handler_t* local_mr) {
	rdmaio_regattr_t attr = rdmaio_reg_handler_get_attr(local_mr);
	char* buf = (char *) attr.addr;

	rdmaio_reqdesc_t send_desc;
	send_desc.op = IBV_WR_SEND_WITH_IMM;
	send_desc.flags = IBV_SEND_SIGNALED;
	send_desc.len = 0;
	send_desc.wr_id = 0;

	rdmaio_reqpayload_t send_payload;
	send_payload.local_addr = (uintptr_t)buf;
	send_payload.remote_addr = 0;
	send_payload.imm_data = 0;

	char error_msg[256];
	int res_s = rdmaio_rc_send_normal(qp, &send_desc, &send_payload, error_msg, sizeof(error_msg));
	if (res_s != 0) {
		fprintf(stderr, "Error sending termination message: %s\n", error_msg);
		return;
	}

	struct ibv_wc wc;
	int res_p = rdmaio_rc_wait_rc_comp(qp, NULL, &wc);
	if (res_p != 0) {
		fprintf(stderr, "Error waiting for termination send completion: %d\n", res_p);
	}
}

int main(int argc, char **argv) {
    // In C, you would typically parse command line arguments manually
    // using argc and argv. For this example, we'll use the placeholder
    // FLAGS_ variables defined above.

    rdmaio_rctrl_t* ctrl = rctrl_create(FLAGS_port, NULL); // NULL defaults to localhost
    if (!ctrl) {
        fprintf(stderr, "Failed to create RCtrl.\n");
        return 1;
    }

	rdmaio_recv_manager_t* manager = recv_manager_create(ctrl);
	if (!manager) {
		fprintf(stderr, "Failed to create recv manager.\n");
		return 1;
	}

	size_t count = 0;
	rdmaio_devidx_t* dev_idx_array = rnic_info_query_dev_names(&count);
	if (dev_idx_array == NULL || count == 0) {
		fprintf(stderr, "No devices found.\n");
		return 1;
	}

	if (FLAGS_use_nic_idx >= count) {
		fprintf(stderr, "Invalid nic index specified: %ld (found %zu devices).\n", FLAGS_use_nic_idx, count);
		rnic_info_free_dev_names(dev_idx_array);
		return 1;
	}

	rdmaio_devidx_t selected_dev_idx = dev_idx_array[FLAGS_use_nic_idx];
	uint8_t gid = 0; // Default GID value

	rdmaio_nic_t* nic = rnic_create(selected_dev_idx, gid);
	if (nic == NULL) {
		fprintf(stderr, "Failed to create RNic with index %d port %d.\n", selected_dev_idx.dev_id,
			selected_dev_idx.port_id);
		rnic_info_free_dev_names(dev_idx_array);
		recv_manager_destroy(manager);
		rctrl_destroy(ctrl);
		return 1;
	}

    char reg_mem_name_str[50];
    sprintf(reg_mem_name_str, "%ld", FLAGS_reg_mem_name);
    if (!rctrl_register_nic(ctrl, reg_mem_name_str, nic)) {
        fprintf(stderr, "Failed to register NIC with name %s.\n", reg_mem_name_str);
    	rnic_info_free_dev_names(dev_idx_array);
        rnic_destroy(nic);
        recv_manager_destroy(manager);
        rctrl_destroy(ctrl);
        return 1;
    }

    char reg_ack_mem_name_str[50];
    sprintf(reg_ack_mem_name_str, "%ld", FLAGS_reg_ack_mem_name);
    if (!rctrl_register_nic(ctrl, reg_ack_mem_name_str, nic)) {
        fprintf(stderr, "Failed to register NIC with name %s.\n", reg_ack_mem_name_str);
    	rnic_info_free_dev_names(dev_idx_array);
        rnic_destroy(nic);
    	recv_manager_destroy(manager);
        rctrl_destroy(ctrl);
        return 1;
    }

    rdmaio_rc_t* qp = NULL;
	rdmaio_reg_handler_t* local_mr = NULL;
    init_send_queue(nic, &qp, &local_mr);
    printf("rc client ready to send message to the server!\n");

    rdmaio_rc_t* recv_qp = NULL;
    void* recv_rs = NULL;
    init_recv_queue(ctrl, nic, manager, &recv_qp, &recv_rs);
    printf("rc client ready to receive acknowledgements from the server!\n");

    int num_bytes = FLAGS_msg_size;
    int message_count = 0;

    printf("Sending 1000 messages of size %d for warmup.\n", num_bytes);

    int warm_up_msgs = 1000;
    int saved_msgs_count = (warm_up_msgs < FLAGS_msg_count) ? warm_up_msgs : FLAGS_msg_count;
    char* saved_msgs[saved_msgs_count];
    for (int i = 0; i < saved_msgs_count; ++i) {
        saved_msgs[i] = generateRandomString(num_bytes);
    }

    /* warm up run here */
    for (int i = 0, idx = 0; i < warm_up_msgs; ++i, idx = (idx + 1) % saved_msgs_count) {
        long arr[3]; // we don't care about the returned values in warm up.
        publish_messages_and_receive_ack(qp, local_mr, saved_msgs[idx], ++message_count, arr, recv_qp, recv_rs);
        free(saved_msgs[idx]); // Free the allocated string
    }

    send_reset(qp, local_mr);

    /* test run */
    int num_msgs = FLAGS_msg_count;
    long* times[num_msgs];
    message_count = 0;
    printf("Sending %d messages of size %d for test.\n", num_msgs, num_bytes);
    for (int i = 0, idx = 0; i < num_msgs; ++i, idx = (idx + 1) % saved_msgs_count) {
        long *arr = (long*)malloc(sizeof(long) * 3);
        times[i] = arr;
        saved_msgs[i] = generateRandomString(num_bytes); // Generate new random string for test
        publish_messages_and_receive_ack(qp, local_mr, saved_msgs[i], ++message_count, arr, recv_qp, recv_rs);
        free(saved_msgs[i]); // Free the allocated string
    }

    printf("Number of messages: %d\n", message_count);
    writeResultsToFile(times, message_count, num_bytes);

    printf("Sending terminate signal to server\n");
    send_termination(qp, local_mr);

    printf("Terminating client\n");
    for (int i = 0; i < num_msgs; ++i) {
        free(times[i]);
    }

	for (int i = 0; i < saved_msgs_count; ++i) {
		free(saved_msgs[i]);
	}

    rnic_destroy(nic);
    recv_manager_destroy(manager);
    rctrl_destroy(ctrl);
    rdmaio_reg_handler_destroy(local_mr);
    rdmaio_destroy_qp(recv_qp); // Assuming recv_qp was allocated with malloc

    return 0;
}