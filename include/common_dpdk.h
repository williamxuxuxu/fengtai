#ifndef _COMMON_DPDK_H_
#define _COMMON_DPDK_H_


// #ifndef RTE_LIBEAL_USE_HPET
// #define RTE_LIBEAL_USE_HPET
// #endif


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/queue.h>
#include <netinet/in.h>
#include <setjmp.h>
#include <stdarg.h>
#include <ctype.h>
#include <errno.h>
#include <getopt.h>
#include <signal.h>
#include <stdbool.h>


#include <rte_common.h>
#include <rte_log.h>
#include <rte_malloc.h>
#include <rte_memcpy.h>
#include <rte_atomic.h>
#include <rte_prefetch.h>
#include <rte_branch_prediction.h>
#include <rte_mempool.h>
#include <rte_mbuf.h>

#include <pthread.h>


#define RTE_LOGTYPE_L2FWD RTE_LOGTYPE_USER1

#define MAX_PKT_BURST 1
#define BURST_TX_DRAIN_US 100 /* TX drain every ~100us */
#define MEMPOOL_CACHE_SIZE 256

/*
 * Configurable number of RX/TX ring descriptors
 */
#define RTE_TEST_RX_DESC_DEFAULT 1024
#define RTE_TEST_TX_DESC_DEFAULT 1024

#define MAX_RX_QUEUE_PER_LCORE 16
#define MAX_TX_QUEUE_PER_PORT 16

#define MAX_TIMER_PERIOD 86400 /* 1 day max */




// 优先级队列入队时，可操作全部优先级队列
//typedef struct
//{
//	LINKLIST* pQueue[PRIORITY_Q_NUM];							// PRIORITY_QUEUE_NUM个优先级队列
//	pthread_mutex_t* ptr_mutex_access_pri_Q[PRIORITY_Q_NUM];	// 每个队列拥有一个访问互斥量
//
//}ENQUEUE_PRIO_PARAM;


#endif
