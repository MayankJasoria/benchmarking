// rlibv2/c_wrappers/rdmaio_rc_recv_manager_c.h
#ifndef RDMAIO_RC_RECV_MANAGER_C_H
#define RDMAIO_RC_RECV_MANAGER_C_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "rdmaio_rctrl_c.h"
#include "rdmaio_mem_c.h" // Include for rdmaio_rmem_t

/*!
 * @brief Opaque type representing the RC Receive Manager.
 */
typedef struct rdmaio_recv_manager_t rdmaio_recv_manager_t;

/*!
 * @brief Opaque type representing the SimpleAllocator.
 */
typedef struct simple_allocator_t simple_allocator_t;

/*!
 * @brief Creates an RC Receive Manager object.
 * @param ctrl A pointer to the RCtrl object.
 * @return A pointer to the newly created RecvManager object, or NULL on failure.
 */
rdmaio_recv_manager_t* recv_manager_create(rdmaio_rctrl_t* ctrl);

/*!
 * @brief Creates a SimpleAllocator object.
 * @param mem A pointer to the RMem object.
 * @param key The memory region key.
 * @return A pointer to the newly created SimpleAllocator object, or NULL on failure.
 */
simple_allocator_t* simple_allocator_create(rdmaio_rmem_t* mem, uint32_t key);

/*!
 * @brief Destroys the SimpleAllocator object.
 * @param allocator A pointer to the SimpleAllocator object to destroy.
 */
void simple_allocator_destroy(simple_allocator_t* allocator);

/*!
 * @brief Registers a receive Completion Queue with a name.
 * @param manager A pointer to the RecvManager object.
 * @param name The name to register the receive CQ under.
 * @param cq A pointer to the ibv_cq object.
 * @param allocator A pointer to the SimpleAllocator object (as AbsRecvAllocator).
 * @return True if the registration was successful, false otherwise.
 */
bool recv_manager_reg_recv_cq(rdmaio_recv_manager_t* manager, const char* name, void* cq, simple_allocator_t* allocator);

/*!
 * @brief Queries for registered receive entries.
 * @param manager A pointer to the RecvManager object.
 * @param name The name of the receive entries to query.
 * @return A pointer to the receive entries (opaque), or NULL if not found.
 */
void* recv_manager_query_recv_entries(rdmaio_recv_manager_t* manager, const char* name);

/*!
 * @brief Destroys the RC Receive Manager object.
 * @param manager A pointer to the RecvManager object to destroy.
 */
void recv_manager_destroy(rdmaio_recv_manager_t* manager);

#endif // RDMAIO_RC_RECV_MANAGER_C_H