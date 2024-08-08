/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2017 Intel Corporation
 */

#ifndef _RTE_LOG_H_
#define _RTE_LOG_H_

/**
 * @file
 *
 * RTE Logs API
 *
 * This file provides a log API to RTE applications.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>


/* SDK log type */
#define RTE_LOGTYPE_EAL        0 /**< Log related to eal. */
#define RTE_LOGTYPE_MALLOC     1 /**< Log related to malloc. */
#define RTE_LOGTYPE_RING       2 /**< Log related to ring. */
#define RTE_LOGTYPE_MEMPOOL    3 /**< Log related to mempool. */
#define RTE_LOGTYPE_TIMER      4 /**< Log related to timers. */
#define RTE_LOGTYPE_PMD        5 /**< Log related to poll mode driver. */
#define RTE_LOGTYPE_HASH       6 /**< Log related to hash table. */
#define RTE_LOGTYPE_LPM        7 /**< Log related to LPM. */
#define RTE_LOGTYPE_KNI        8 /**< Log related to KNI. */
#define RTE_LOGTYPE_ACL        9 /**< Log related to ACL. */
#define RTE_LOGTYPE_POWER     10 /**< Log related to power. */
#define RTE_LOGTYPE_METER     11 /**< Log related to QoS meter. */
#define RTE_LOGTYPE_SCHED     12 /**< Log related to QoS port scheduler. */
#define RTE_LOGTYPE_PORT      13 /**< Log related to port. */
#define RTE_LOGTYPE_TABLE     14 /**< Log related to table. */
#define RTE_LOGTYPE_PIPELINE  15 /**< Log related to pipeline. */
#define RTE_LOGTYPE_MBUF      16 /**< Log related to mbuf. */
#define RTE_LOGTYPE_CRYPTODEV 17 /**< Log related to cryptodev. */
#define RTE_LOGTYPE_EFD       18 /**< Log related to EFD. */
#define RTE_LOGTYPE_EVENTDEV  19 /**< Log related to eventdev. */
#define RTE_LOGTYPE_GSO       20 /**< Log related to GSO. */

/* these log types can be used in an application */
#define RTE_LOGTYPE_USER1     24 /**< User-defined log type 1. */
#define RTE_LOGTYPE_USER2     25 /**< User-defined log type 2. */
#define RTE_LOGTYPE_USER3     26 /**< User-defined log type 3. */
#define RTE_LOGTYPE_USER4     27 /**< User-defined log type 4. */
#define RTE_LOGTYPE_USER5     28 /**< User-defined log type 5. */
#define RTE_LOGTYPE_USER6     29 /**< User-defined log type 6. */
#define RTE_LOGTYPE_USER7     30 /**< User-defined log type 7. */
#define RTE_LOGTYPE_USER8     31 /**< User-defined log type 8. */

/** First identifier for extended logs */
#define RTE_LOGTYPE_FIRST_EXT_ID 32

/* Can't use 0, as it gives compiler warnings */
#define RTE_LOG_EMERG    1U  /**< System is unusable.               */
#define RTE_LOG_ALERT    2U  /**< Action must be taken immediately. */
#define RTE_LOG_CRIT     3U  /**< Critical conditions.              */
#define RTE_LOG_ERR      4U  /**< Error conditions.                 */
#define RTE_LOG_WARNING  5U  /**< Warning conditions.               */
#define RTE_LOG_NOTICE   6U  /**< Normal but significant condition. */
#define RTE_LOG_INFO     7U  /**< Informational.                    */
#define RTE_LOG_DEBUG    8U  /**< Debug-level messages.             */

 
/*
 * Generates a log message The message will be sent in the stream
 * defined by the previous call to rte_openlog_stream().
 * No need to check level here, done by rte_vlog().
 */
static inline int
rte_log(uint32_t level, uint32_t logtype, const char *format, ...)
{
	RTE_SET_USED(level);
	RTE_SET_USED(logtype);
	
	va_list ap;
	int ret;

	va_start(ap, format);
	ret = vfprintf(stdout, format, ap);
	fflush(stdout);
	va_end(ap);
	return ret;
}


/**
 * Generates a log message.
 *
 * The RTE_LOG() is a helper that prefixes the string with the log level
 * and type, and call rte_log().
 *
 * @param l
 *   Log level. A value between EMERG (1) and DEBUG (8). The short name is
 *   expanded by the macro, so it cannot be an integer value.
 * @param t
 *   The log type, for example, EAL. The short name is expanded by the
 *   macro, so it cannot be an integer value.
 * @param ...
 *   The fmt string, as in printf(3), followed by the variable arguments
 *   required by the format.
 * @return
 *   - 0: Success.
 *   - Negative on error.
 */
#define RTE_LOG(l, t, ...)					\
	 rte_log(RTE_LOG_ ## l,					\
		 RTE_LOGTYPE_ ## t, # t ": " __VA_ARGS__)


#ifdef __cplusplus
}
#endif

#endif /* _RTE_LOG_H_ */
