#define _GNU_SOURCE 1

#include <pthread.h>
#include <sched.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

#include "sys_thread.h"
#include "mem_utils.h"
#include "system_log.h"
#include "global_var.h"
#include "receive.h"
#include "decapsulate.h"
#include "encapsulate.h"
#include "timer.h"
#include "encapsulate_frame.h"
static THREAD_POOL thread_pool;

COMM_THREADS  g_stru_single_threads;


static int g_i_decap_threads_pool_initialized  = FALSE;
THREAD_POOL g_st_decap_pool;

static int g_i_encap_threads_pool_initialized  = FALSE;
THREAD_POOL g_st_encap_pool;

static int g_i_recv_threads_pool_initialized  = FALSE;
THREAD_POOL g_st_recv_pool;

static int g_i_snd_bb_threads_pool_initialized  = FALSE;
THREAD_POOL g_st_snd_bb_pool;

/*
*	Spawn a new thread, and place it in the thread pool.
*
*	The thread is started initially in the blocked state, waiting
*	for the semaphore.
*/
static THREAD_HANDLE * threadpool_spawn_thread(void *(*thread_fun)(void*), int pThreadParam)
{
	int i_ret;
	THREAD_HANDLE* handle;
	pthread_attr_t attr;
	//static uint8_t core_id = 7;

	if (thread_pool.total_threads >= thread_pool.max_threads) 
	{		
		LOG(LOG_ERR, LOG_MODULE_MAIN, LOG_ERR,
				LOG_CONTENT("Thread spawn failed. Maximum number of threads (%d) already running."), 
				thread_pool.max_threads);
		return NULL;
	}

	/*
	*	Allocate a new thread handle.
	*/
	handle = (THREAD_HANDLE *) malloc(sizeof(THREAD_HANDLE));
	memset(handle, 0, sizeof(THREAD_HANDLE));
	handle->prev = NULL;
	handle->next = NULL;
	handle->pthread_id = NO_SUCH_CHILD_PID;
	handle->nStatus = THREAD_STAT_RUNNING;
	handle->timestamp = time(NULL);
	handle->pParam = pThreadParam;

	/*
	*	Initialize the thread's attributes to detached.
	*
	*	We could call pthread_detach() later, but if the thread
	*	exits between the create & detach calls, it will need to
	*	be joined, which will never happen.
	*/
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	#if 0
	if(core_id == 11)
		{
	cpu_set_t cpu_info;
    CPU_ZERO(&cpu_info);
    CPU_SET(core_id, &cpu_info);
    core_id++;
	if(pthread_attr_setaffinity_np(&attr, sizeof(cpu_set_t), &cpu_info) != 0) {
	    exit(1);
	}
		}
	#endif

	/*
	*	Create the thread detached, so that it cleans up it's
	*	own memory when it exits.
	*
	*	Note that the function returns non-zero on error, NOT
	*	-1.  The return code is the error, and errno isn't set.
	*/
	i_ret = pthread_create(&handle->pthread_id, &attr,
						   thread_fun, handle);
	if (i_ret != 0) 
	{
		LOG(LOG_ERR, LOG_MODULE_MAIN, LOG_ERR,
				LOG_CONTENT("FATAL: Thread create failed: %d"), 
				i_ret);
		exit(1);
	}
	pthread_attr_destroy(&attr);
	thread_pool.total_threads++;
	LOG(LOG_NOTICE, LOG_MODULE_MAIN, LOG_NOTICE,
			LOG_CONTENT("spawned new thread for %p. Total threads in pool: %d"), 
			thread_fun, thread_pool.total_threads);

    //  add to thread pool
	if (thread_pool.tail) 
	{
		thread_pool.tail->next = handle;
		handle->prev = thread_pool.tail;
		thread_pool.tail = handle;
	} 
	else 
	{
		thread_pool.head = thread_pool.tail = handle;
	}

	return handle;
}


/*
*	Allocate the thread pool, and seed it with an initial number
*	of threads.
*   create this function as a base one
*/
int thread_pool_init(THREAD_POOL* vpst_pool, 
						   int* vpi_flag, 
						   void* (*thread_fun)(void*), 
						   int vi_num)
{
	int	i_index = 0;
	
	if (0 >= vi_num)
	{
	    LOG(LOG_ERR, LOG_MODULE_MAIN, LOG_ERR,
		   LOG_CONTENT("vi_num=%d"), 
				vi_num);
		return ERROR;
	}

	if (NULL == vpst_pool)
	{
		LOG(LOG_ERR, LOG_MODULE_MAIN, LOG_ERR,
				LOG_CONTENT("vpst_pool=NULL"));
		return ERROR;
	}

	if (NULL == vpi_flag)
	{
		LOG(LOG_ERR, LOG_MODULE_MAIN, LOG_ERR,
			   LOG_CONTENT("vpi_flag=NULL"));
		return ERROR;
	}

	if (*vpi_flag)
	{
		LOG(LOG_ERR, LOG_MODULE_MAIN, LOG_ERR,
				LOG_CONTENT("already initialized"));
		return 0;
	}
	
	LOG(LOG_NOTICE, LOG_MODULE_MAIN, LOG_NOTICE, 
			LOG_CONTENT("Initializing the thread pool..."));

	memset((void*)&thread_pool, 0, sizeof(THREAD_POOL));
	thread_pool.head = NULL;
	thread_pool.tail = NULL;
	thread_pool.total_threads = 0;
	thread_pool.cleanup_delay = 5;
	thread_pool.max_threads = vi_num;
	
	for(i_index = 0;i_index < thread_pool.max_threads;i_index++)
	{
		if (threadpool_spawn_thread(thread_fun, i_index) == NULL) 
		{
					LOG(LOG_ERR, LOG_MODULE_MAIN, LOG_ERR,
					LOG_CONTENT("threadpool_spawn_thread()=NULL"));
		}
	}

	memset((void*)vpst_pool, 0, sizeof(THREAD_POOL));
	memcpy((void*)vpst_pool, (void*)&thread_pool, sizeof(THREAD_POOL));
	*vpi_flag = 1;

	return 0;
}




/*
*
*	This function ONLY remove the thread_handle struct from pool link and free it. To finish the thread rely on others
*/
void delete_thread(THREAD_HANDLE *handle)
{
	THREAD_HANDLE* prev = NULL;
	THREAD_HANDLE* next = NULL;

	pthread_mutex_lock(&thread_pool.mutex);
	
	prev = handle->prev;
	next = handle->next;
	if (thread_pool.total_threads < 1) 
	{
		return;
	}
	thread_pool.total_threads--;

	/*
	*	Remove the handle from the list.
	*/
	if (prev == NULL) 
	{
		thread_pool.head = next;
	} 
    else 
    {
		prev->next = next;
	}

	if (next == NULL) 
    {
		thread_pool.tail = prev;
	} 
    else 
	{
		next->prev = prev;
	}
	
		LOG(LOG_ERR, LOG_MODULE_MAIN, LOG_ERR,
			LOG_CONTENT("Deleting thread %d"), 
			handle->pthread_id);
	
	// Free the memory, now that we're sure the thread exited.
	free_p_memory((void**)&handle);

	pthread_mutex_unlock(&thread_pool.mutex);
}




#define MAX_MESSAGE_TYPE (5)




int create_single_threads (void)
{
    int i_ret = 0;
	pthread_attr_t attr;

    //  prepare the attributes of a thread
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	#if 0
	i_ret = pthread_create(&(g_stru_single_threads.threadId_rcv_thread),
				   &attr,
				   receive_msg_thread,
				   NULL);
	if (i_ret != 0)
	{
	    printf("create receive_msg_thread failed!\n");
		LOG(LOG_ERR, LOG_MODULE_MAIN, LOG_ERR,
				LOG_CONTENT("receive_msg_thread thread create failed: %d"),
				i_ret);
		pthread_attr_destroy(&attr);
		return -1;
	}
	
	

	i_ret = pthread_create(&(g_stru_single_threads.threadId_tun_thread),
				   &attr,
				   receive_tun_thread,
				   NULL);
	if (i_ret != 0)
	{
	    printf("create receive_tun_thread failed!\n");
		LOG(LOG_ERR, LOG_MODULE_MAIN, LOG_ERR,
				LOG_CONTENT("receive_tun_thread thread create failed: %d"),
				i_ret);
		pthread_attr_destroy(&attr);
		return -1;
	}
	
	i_ret = pthread_create(&(g_stru_single_threads.threadId_timer_creator_thread),
				   &attr,
				   time_creator,
				   NULL);
	if (i_ret != 0)
	{
	    printf("create time_creator failed!\n");
		LOG(LOG_ERR, LOG_MODULE_MAIN, LOG_ERR,
				LOG_CONTENT("create time_creator failed: %d"),
				i_ret);
		pthread_attr_destroy(&attr);
		return -1;
	}
	#endif
    #ifdef cmdline
#if 0
	i_ret = pthread_create(&(g_stru_single_threads.threadId_show_statistics_srv_thread),
				   &attr,
				   show_statistics_srv_thread,
				   NULL);
	if (i_ret != 0)
	{
	    printf("create show_statistics_srv_thread failed!\n");
		LOG(LOG_ERR, LOG_MODULE_MAIN, LOG_ERR,
				LOG_CONTENT("create show_statistics_srv_thread failed: %d"),
				i_ret);
		pthread_attr_destroy(&attr);
		return -1;
	}
	#endif
    #else
	i_ret = pthread_create(&(g_stru_single_threads.threadId_show_statistics_cli_thread),
				   &attr,
				   show_statistics_cli_thread,
				   NULL);
	if (i_ret != 0)
	{
	    printf("create show_statistics_cli_thread failed!\n");
		LOG(LOG_ERR, LOG_MODULE_MAIN, LOG_ERR,
				LOG_CONTENT("create show_statistics_cli_thread failed: %d"),
				i_ret);
		pthread_attr_destroy(&attr);
		return -1;
	}
	#endif
	
    return 0;
}


int create_multi_threads(void)
{
    int i_ret = 0;
	#if 1
    i_ret = thread_pool_init(&g_st_decap_pool, 
                             &g_i_decap_threads_pool_initialized,
                             decapsulate_thread,
                             g_st_app_conf.decap_thread_num);
	if (SUCCESS != i_ret)
	{
	    LOG(LOG_ERR, LOG_MODULE_MAIN, LOG_ERR,
	        LOG_CONTENT("decapsulate_thread thread create failed: %d"),
				i_ret);
	   // return ERROR;
	}

	   
    i_ret = thread_pool_init(&g_st_encap_pool, 
                             &g_i_encap_threads_pool_initialized,
                             encapsulate_thread,
                             g_st_app_conf.encap_thread_num);
	if (SUCCESS != i_ret)
	{
	    LOG(LOG_ERR, LOG_MODULE_MAIN, LOG_ERR,
	        LOG_CONTENT("encapsulate_thread thread create failed: %d"),
				i_ret);
	    return ERROR;
	}
    #endif 
	i_ret = thread_pool_init(&g_st_recv_pool, 
                             &g_i_recv_threads_pool_initialized,
                             receive_msg_thread,
                             g_st_app_conf.recv_thread_num);
	if (SUCCESS != i_ret)
	{
	    LOG(LOG_ERR, LOG_MODULE_MAIN, LOG_ERR,
	        LOG_CONTENT("recv thread create failed: %d"),
				i_ret);
	    return ERROR;
	}


	i_ret = thread_pool_init(&g_st_snd_bb_pool, 
                             &g_i_snd_bb_threads_pool_initialized,
                             snd_bbframe_thread,
                             g_st_app_conf.encap_thread_num);
	if (SUCCESS != i_ret)
	{
	    LOG(LOG_ERR, LOG_MODULE_MAIN, LOG_ERR,
	        LOG_CONTENT("snd bb thread create failed: %d"),
				i_ret);
	    return ERROR;
	}

	return SUCCESS;
}

void thread_cancel(void)
{
    int res = 0;

    if (0 != g_stru_single_threads.threadId_rcv_thread)
    {
        res = pthread_cancel(g_stru_single_threads.threadId_rcv_thread);
        if (0 != res)
        {
            LOG(LOG_INFO, LOG_MODULE_MAIN, LOG_INFO,
				LOG_CONTENT("ra_redis_correlate_check_thread thread create failed: %d"),
				res);
        }
        g_stru_single_threads.threadId_rcv_thread = 0;
    }
}





