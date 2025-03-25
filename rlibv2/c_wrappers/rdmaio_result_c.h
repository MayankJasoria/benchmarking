//
// Created by mjasoria on 24/03/25.
//

#ifndef RDMAIO_RESULT_C_H
#define RDMAIO_RESULT_C_H

typedef enum {
	/*! @brief Operation completed successfully. */
	RDMAIO_OK = 0,
	/*! @brief An error occurred during the operation. */
	RDMAIO_ERR = 1,
	/*! @brief The operation timed out. */
	RDMAIO_TIMEOUT = 2,
	/*! @brief Operation completed with a near-success status (check specific context). */
	RDMAIO_NEAR_OK = 3,
	/*! @brief The resource is not in a ready state for the operation. */
	RDMAIO_NOT_READY = 4
} rdmaio_iocode_t;

#endif //RDMAIO_RESULT_C_H
