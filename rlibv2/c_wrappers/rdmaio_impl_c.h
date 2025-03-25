// rlibv2/c_wrappers/rdmaio_impl_c.h
#ifndef RDMAIO_IMPL_C_H
#define RDMAIO_IMPL_C_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "rdmaio_result_c.h"
#include "rdmaio_config_c.h"
#include "rdmaio_nic_c.h"
#include "rdmaio_qpattr_c.h"

/*!
 * @brief Brings a Queue Pair to the INIT state.
 * @param qp The ibv_qp object.
 * @param config Configuration parameters for the QP.
 * @param nic A pointer to the RDMA NIC.
 * @param err_msg Buffer to store an error message if the operation fails. Can be NULL.
 * @param err_msg_size Size of the error message buffer.
 * @return RDMAIO_OK on success, RDMAIO_ERR otherwise.
 */
rdmaio_iocode_t rdmaio_bring_qp_to_init(void* qp, const rdmaio_qpconfig_t* config, rdmaio_nic_t* nic, char* err_msg, size_t err_msg_size);

/*!
 * @brief Brings an RC Queue Pair to the RTR (Ready-to-Receive) state.
 * @param qp The ibv_qp object.
 * @param config Configuration parameters for the QP.
 * @param attr Attributes of the remote QP.
 * @param port_id Local port ID.
 * @param err_msg Buffer to store an error message if the operation fails. Can be NULL.
 * @param err_msg_size Size of the error message buffer.
 * @return RDMAIO_OK on success, RDMAIO_ERR otherwise.
 */
rdmaio_iocode_t rdmaio_bring_rc_to_rcv(void* qp, const rdmaio_qpconfig_t* config, const rdmaio_qpattr_t* attr, int port_id, char* err_msg, size_t err_msg_size);

/*!
 * @brief Brings an RC Queue Pair to the RTS (Ready-to-Send) state.
 * @param qp The ibv_qp object.
 * @param config Configuration parameters for the QP.
 * @param err_msg Buffer to store an error message if the operation fails. Can be NULL.
 * @param err_msg_size Size of the error message buffer.
 * @return RDMAIO_OK on success, RDMAIO_ERR otherwise.
 */
rdmaio_iocode_t rdmaio_bring_rc_to_send(void* qp, const rdmaio_qpconfig_t* config, char* err_msg, size_t err_msg_size);

/*!
 * @brief Creates a Completion Queue.
 * @param nic A pointer to the RDMA NIC.
 * @param cq_sz The size of the Completion Queue.
 * @param err_msg Buffer to store an error message if the operation fails. Can be NULL.
 * @param err_msg_size Size of the error message buffer.
 * @return A pointer to the created ibv_cq object, or NULL on failure.
 */
void* rdmaio_create_cq(rdmaio_nic_t* nic, int cq_sz, char* err_msg, size_t err_msg_size);

/*!
 * @brief Destroys a Completion Queue.
 * @param cq The ibv_cq object to destroy.
 */
void rdmaio_destroy_cq(void* cq);

/*!
 * @brief Creates a Queue Pair.
 * @param nic A pointer to the RDMA NIC.
 * @param qp_type The type of the Queue Pair (e.g., IBV_QPT_RC).
 * @param config Configuration parameters for the QP.
 * @param send_cq The Completion Queue for send operations.
 * @param recv_cq The Completion Queue for receive operations (can be the same as send_cq).
 * @param err_msg Buffer to store an error message if the operation fails. Can be NULL.
 * @param err_msg_size Size of the error message buffer.
 * @return A pointer to the created ibv_qp object, or NULL on failure.
 */
void* rdmaio_create_qp(rdmaio_nic_t* nic, int qp_type, const rdmaio_qpconfig_t* config, void* send_cq, void* recv_cq, char* err_msg, size_t err_msg_size);

/*!
 * @brief Destroys a Queue Pair.
 * @param qp The ibv_qp object to destroy.
 */
void rdmaio_destroy_qp(void* qp);

/*!
 * @brief Creates a Shared Receive Queue.
 * @param nic A pointer to the RDMA NIC.
 * @param max_wr Maximum outstanding work requests.
 * @param max_sge Maximum scatter/gather elements per work request.
 * @param err_msg Buffer to store an error message if the operation fails. Can be NULL.
 * @param err_msg_size Size of the error message buffer.
 * @return A pointer to the created ibv_srq object, or NULL on failure.
 */
void* rdmaio_create_srq(rdmaio_nic_t* nic, int max_wr, int max_sge, char* err_msg, size_t err_msg_size);

/*!
 * @brief Destroys a Shared Receive Queue.
 * @param srq The ibv_srq object to destroy.
 */
void rdmaio_destroy_srq(void* srq);

/*!
 * @brief Creates a Direct Connect Transport.
 * @param nic A pointer to the RDMA NIC.
 * @param cq The Completion Queue.
 * @param srq The Shared Receive Queue.
 * @param dc_key The Direct Connect key.
 * @param err_msg Buffer to store an error message if the operation fails. Can be NULL.
 * @param err_msg_size Size of the error message buffer.
 * @return A pointer to the created ibv_exp_dct object, or NULL on failure.
 */
void* rdmaio_create_dct(rdmaio_nic_t* nic, void* cq, void* srq, int dc_key, char* err_msg, size_t err_msg_size);

/*!
 * @brief Destroys a Direct Connect Transport.
 * @param dct The ibv_exp_dct object to destroy.
 */
void rdmaio_destroy_dct(void* dct);

/*!
 * @brief Creates an extended Queue Pair.
 * @param nic A pointer to the RDMA NIC.
 * @param config Configuration parameters for the QP.
 * @param send_cq The Completion Queue for send operations.
 * @param recv_cq The Completion Queue for receive operations (can be the same as send_cq).
 * @param err_msg Buffer to store an error message if the operation fails. Can be NULL.
 * @param err_msg_size Size of the error message buffer.
 * @return A pointer to the created ibv_qp object, or NULL on failure.
 */
void* rdmaio_create_qp_ex(rdmaio_nic_t* nic, const rdmaio_qpconfig_t* config, void* send_cq, void* recv_cq, char* err_msg, size_t err_msg_size);

#endif // RDMAIO_IMPL_C_H