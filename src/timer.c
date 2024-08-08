#include <stdio.h>
#include <malloc.h>
#include <error.h>
#include <sys/ioctl.h>
#include <time.h>
#include <sys/time.h>



#include "define_const.h"
#include "system_log.h"
#include "timer.h"


uint64_t g_u64_now_time = 0;
uint32_t g_u32_now_time = 0;


void *time_creator(void *pv_arg)
{
   
    (void)pv_arg;
    
    struct timeval tm_current = {0};
    

    int i_thread_id = 0;

    i_thread_id = pthread_self();

    printf("Hi, I'm thread time_creator\n");
    
   	LOG(LOG_NOTICE, LOG_MODULE_TIMEER, LOG_NOTICE,
        LOG_CONTENT("Hi, I'm thread time_creator %d"), i_thread_id);
    
    while (TRUE)
    {
        gettimeofday(&tm_current, NULL);
        g_u32_now_time = tm_current.tv_sec;
        g_u64_now_time = (uint64_t)((uint64_t)tm_current.tv_sec *USECONDS_PER_SECOND + tm_current.tv_usec);

        
		usleep(100);
    }
    
    return NULL;
}

uint64_t get_cur_time_us(void)
{
	return g_u64_now_time;
}

uint32_t get_cur_time_s(void)
{
	return g_u32_now_time;
}



