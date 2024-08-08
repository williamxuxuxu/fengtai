#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <syslog.h>
#include <dirent.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <sys/ipc.h>
#include <sys/stat.h>
#include <sys/msg.h>

#include <semaphore.h>
#include <signal.h>
#include <pthread.h>
#include <sched.h>
#include <sys/wait.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <termios.h>

#include <sys/epoll.h>
#include <sys/queue.h>
#include <stdarg.h>
#include "system_log.h"

#include "define_const.h"
#include "general_module.h"

//extern struct ipgw_manager* ipgw_mgr;
uint16_t us_level_limit = LOG_NOTICE;
char system_time[64] = {0};
void* pst_out_file;

uint32_t  ui_rotate_c = 5000;
char ac_path[MAX_PATH_LEN] = "./log";
char ac_filename[MAX_FILE_NAME_LEN] = "router.log";


static uint32_t  sg_ui_log_function_call_times = 0;
static pthread_mutex_t sg_mutex_log_print_times;

static pthread_rwlock_t sg_rwlock_log_token;

void init_sys_log(void)
{
    openlog("ut", LOG_CONS | LOG_NDELAY, LOG_LOCAL0);
}



int log_router_init_rwlock(void)
{
    int i_ret = 0;
    pthread_rwlockattr_t attr;

    i_ret = pthread_rwlockattr_init(&attr);
    if(i_ret != 0)
    {
        return -1;
    }

    i_ret = pthread_rwlockattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
    if(i_ret != 0)
    {

        return -2;
    }

    i_ret = pthread_rwlock_init(&sg_rwlock_log_token, &attr);
    if (i_ret != 0)
    {

        return -3;
    }
    
    // destory rwlock attr
    i_ret = pthread_rwlockattr_destroy(&attr);
    if (i_ret != 0)
    {

        return -4;
    }

  return 0;
}

int log_router_init_mutex(void)
{
	int i_ret = 0;
	
	i_ret = pthread_mutex_init(&sg_mutex_log_print_times, NULL);
	if (i_ret != 0)
	{
		printf("log_router_init i_ret = %d", i_ret);
		return -1;
	}
	
	return SUCCESS;
}


int log_router_init_dir(void)
{
	uint32_t ui_file_path_len = strlen(ac_path);
	char acCmd[MAX_PATH_LEN + MAX_FILE_NAME_LEN + 32] = {0};

	strcat(acCmd,"rm -rf ");
	if (ui_file_path_len > 0)
	{
		strcat(acCmd, ac_path);
		if (ac_path[ui_file_path_len-1] != '/')
		{
			strcat(acCmd, "/");
		}
	}


	strcat(acCmd, ".svn");
  
    
	system(acCmd); 
    
    return SUCCESS;
}


int router_log_init(void)
{
    int i_ret = SUCCESS;

    i_ret = log_router_init_mutex();
    if (i_ret != SUCCESS)
	{
		printf("log_router_init_mutex() failed,ret=%d", i_ret);
		return -1;
	}

    i_ret = log_router_init_rwlock();
    if (i_ret != SUCCESS)
	{
		printf("log_routerr_init_rwlock() failed,ret=%d", i_ret);
		return -1;
	}    
    
    i_ret = log_router_init_dir();
    if (i_ret != SUCCESS)
	{
		printf("log_router_init_dir() failed,ret=%d", i_ret);
		return -1;
	}

    return SUCCESS;
}




static void safe_rename(char *oldfile, char *newfile)
{
	int i_ret = 0;
	
    i_ret = rename(oldfile,newfile);
	if (i_ret < 0)
	{
		int i = 200;//  50ms*200 = 10s
		do 
		{	
			usleep(50000);	
			i_ret = rename(oldfile,newfile);
			if (i_ret > 0)
			{
				break;
			}
			i--;
		}while (i  > 0);

		if (i <= 180)
		{
			printf("\nWarnning!for rename file ,we waiting time extend 1s\n");
		}
		if (i_ret < 0)
		{
			printf("\nrename error! i_ret = %d,errno =%d next we will try!\n",i_ret, errno);
		}
	}

	FILE *old_fp = NULL;
	FILE *new_fp = NULL;
	int iloop = 300; // 300 * 10 ms = 3s
	
	do
	{
		old_fp = fopen(oldfile,"r");
		new_fp = fopen(newfile, "r");
		if (!old_fp && new_fp)
		{
			break;
		}
		usleep(10000);
		iloop--;
	}while (iloop > 0);

	if (iloop == 0)
	{
		if (old_fp || !new_fp)
		{
			printf("file system busy\n");
		}
	}
	if (old_fp != NULL)
	{
		fclose(old_fp);
	}
	if (new_fp != NULL)
	{
		fclose(new_fp);
	}	
}


static void split_log(void)
{
    char ac_buff[128] = {0};
    char ac_file_old_path[MAX_PATH_LEN+MAX_FILE_NAME_LEN]={0};
    char ac_file_new_path[MAX_PATH_LEN+MAX_FILE_NAME_LEN]={0};
    char acFile[MAX_PATH_LEN+MAX_FILE_NAME_LEN] = {0};

    struct timeval tv_now = {0};
    struct tm tm_now = {0};
    struct tm *p = NULL;

    gettimeofday (&tv_now , NULL);
    p = gmtime_r(&(tv_now.tv_sec), &tm_now);

	sprintf(ac_buff, "router_%d_%d_%d_%d_%d_%d.log",
				  (1900+p->tm_year),(1 + p->tm_mon),p->tm_mday,
				 p->tm_hour,p->tm_min,p->tm_sec);


    uint32_t n = strlen(ac_path);
    if (n > 0)
    {
    strcat(acFile, ac_path);
    if (ac_path[n-1] != '/')
    {
      strcat(acFile, "/");
    }
    }

    strcat(ac_file_old_path, acFile);
    strcat(ac_file_old_path, ac_filename);
    strcat(ac_file_new_path, acFile);
    strcat(ac_file_new_path, ac_buff);

    usleep(5000);
    fclose(pst_out_file);
    pst_out_file = NULL;

    safe_rename(ac_file_old_path, ac_file_new_path);

}


static void router_printf(int loglevel, 
					  int logModule, 
					  const char * message,
					  va_list p_valist)

{
	if (loglevel <= us_level_limit)
	{
		char acPromptFormat[2048] = {0};
        char acFile[MAX_PATH_LEN+MAX_FILE_NAME_LEN] = {0};
		time_t timep;
		pthread_t id_thread_myself = 0;
		uint32_t ui_len = 0;
		int i_ret = 0;
        uint32_t ui_file_path_len = 0;
        #if 0
        uint8_t ucTag_simple_log_mode = 0;//  if set to 1, use simple log, for commercial env,e.g.
   
        if (us_level_limit < LOG_DEBUG)
        {
            ucTag_simple_log_mode = 1;
        }
		#endif

        //init time to split log by day below
		struct timeval tv_now;
        struct tm tm_now;
        struct tm * p_tm = NULL;
 
        gettimeofday (&tv_now , NULL);
        p_tm = gmtime_r(&(tv_now.tv_sec), &tm_now);
        char auc_time[64] = {0}, current_time[64] = {0};
        sprintf(auc_time,"%u-%02u-%02u %02u:%02u:%02u:%06lu ",
                (1900+p_tm->tm_year),
                (1+p_tm->tm_mon),
                 p_tm->tm_mday,
                 p_tm->tm_hour,
                 p_tm->tm_min,
                 p_tm->tm_sec,
                 tv_now.tv_usec);
        sprintf(current_time, "%u%02u%02u",
                (1900+p_tm->tm_year),
                (1+p_tm->tm_mon),
                 p_tm->tm_mday);
        if(0 == system_time[0]) strcpy(system_time, current_time);
        //init time to split log by day below
        
		time (&timep);
		strcat(acPromptFormat, asctime(gmtime(&timep)));
		ui_len = strlen(acPromptFormat);
		if ((ui_len > 0) && (acPromptFormat[ui_len-1] == '\n'))
		{
			acPromptFormat[ui_len-1] = '\0';
		}
		strcat(acPromptFormat, " ");


        #ifdef OS_EQUAL_LINUX
        id_thread_myself = gettid();
        #else
        id_thread_myself = pthread_self();
        #endif
        
		sprintf(acPromptFormat+strlen(acPromptFormat), 
				"thread %lu ", 
				(uint64_t)id_thread_myself);
		
		switch (loglevel)
		{
		case LOG_EMERG:
            strcat(acPromptFormat, "EMER ");
            break;
        case LOG_ALERT:
            strcat(acPromptFormat, "ALER ");
            break;
        case LOG_CRIT:
            strcat(acPromptFormat, "CRIT ");
            break;
        case LOG_ERR:
            strcat(acPromptFormat, "ERRO ");
            break;
        case LOG_WARNING:
            strcat(acPromptFormat, "WARN ");
            break;
        case LOG_NOTICE:
            strcat(acPromptFormat, "NOTICE ");
            break;
        case LOG_INFO:
            strcat(acPromptFormat, "INFO ");
            break;
        case LOG_DEBUG:
            strcat(acPromptFormat, "DEBG ");
            break;
        default:
            strcat(acPromptFormat, "Warn ");
            break;
		}

		switch (logModule)
		{
		    case LOG_MODULE_MAIN:
			    strcat(acPromptFormat, "MAIN ");
			    break;
		    case LOG_MODULE_RX:
		    	strcat(acPromptFormat, "RECV ");
		    	break;
		    case LOG_MODULE_ENCAP:
		    	strcat(acPromptFormat, "ENCAP ");
		    	break;
		    case LOG_MODULE_DECAP:
		    	strcat(acPromptFormat, "DECAP ");
		    	break;
	    	case LOG_MODULE_ROUTER:
				strcat(acPromptFormat, "ROUTER ");
				break;
			case LOG_MODULE_TIMEER:
				strcat(acPromptFormat, "TIMER ");
				break;
			default:
				strcat(acPromptFormat, "All-Modules ");
				break;
		}

		#if 0

		if (NULL == message_id)
		{
			message_id = "message_id absent";
		}
		if ((NULL == vara_file_name))
		{
			vara_file_name = "filename absent";
		}
		if (NULL == vara_func_name)
		{
			vara_func_name = "func_name absent";
		}

        if (!ucTag_simple_log_mode)
        {
        	strcat(acPromptFormat, message_id);
        	strcat(acPromptFormat, " ");
        	sprintf(acPromptFormat+strlen(acPromptFormat), 
        			ARA_LOG_PREFIX, 
        			vara_file_name, 
        			vpi_line, 
        			vara_func_name);
        }
        #endif
        strcat(acPromptFormat, message);

		//  bertliu@aicent.com changed the mutext scope for safe 20120413
        //  cheaking whether need to split log
        pthread_mutex_lock(&sg_mutex_log_print_times);

        // check log file current row number when APA startup
        if(0 == sg_ui_log_function_call_times)
        {
            ui_file_path_len = strlen(ac_path);

            if (ui_file_path_len > 0)
            {
                strcpy(acFile, ac_path);
                if (ac_path[ui_file_path_len-1] != '/')
                {
                    strcat(acFile, "/");
                }
            }
            strcat(acFile, ac_filename);
            if (strlen(acFile))
            {
                char    wc_command[MAX_PATH_LEN+MAX_FILE_NAME_LEN+8] = {0};
                FILE    *pFP = NULL;
                sprintf(wc_command, "wc -l %s | awk '{print $1}'", acFile);
                pFP = popen(wc_command, "r");
                if(pFP)
                {
                    fgets(wc_command, MAX_PATH_LEN+MAX_FILE_NAME_LEN, pFP);
                    wc_command[MAX_PATH_LEN+MAX_FILE_NAME_LEN] = 0;
                    pclose(pFP);
                    pFP = NULL;
                    sg_ui_log_function_call_times = atoi(wc_command);
                }
            }
            memset(acFile, 0, MAX_PATH_LEN+MAX_FILE_NAME_LEN);
            ui_file_path_len = 0;
        }
		
        if ((sg_ui_log_function_call_times >= ui_rotate_c)
			|| strcmp(system_time, current_time) != 0)
        {
            sg_ui_log_function_call_times =  0;
			strcpy(system_time, current_time);

            if (NULL != pst_out_file)
            {

                //  get read-token of log usage
                i_ret = pthread_rwlock_wrlock(&sg_rwlock_log_token);
            	if(i_ret != 0)
            	{ 
         
                    pthread_mutex_unlock(&sg_mutex_log_print_times);
                    return;
            	}
 
        
                split_log();

                //  release token of log usage
                i_ret = pthread_rwlock_unlock(&sg_rwlock_log_token);
            	if(i_ret != 0)
            	{ 
             

                    pthread_mutex_unlock(&sg_mutex_log_print_times);
                    return;
            	}

            }
        }

        if (NULL == pst_out_file)
        {
            ui_file_path_len = strlen(ac_path);

            if (ui_file_path_len > 0)
            {
                strcpy(acFile, ac_path);
                if (ac_path[ui_file_path_len-1] != '/')
                {
                    strcat(acFile, "/");
                }
            }

            strcat(acFile, ac_filename);
        
            if (strlen(acFile))
            {
                pst_out_file = fopen(acFile, "a+");
            }
        }

        sg_ui_log_function_call_times++;
        pthread_mutex_unlock(&sg_mutex_log_print_times);

        //  get read-token of log usage
        i_ret = pthread_rwlock_rdlock(&sg_rwlock_log_token);
    	if(i_ret != 0)
    	{ 
   

            return;
    	}
        
        if (NULL != pst_out_file)
        {
            strcat(acPromptFormat, "\n");
            vfprintf(pst_out_file, acPromptFormat, p_valist);
        }
        else
        {
            vprintf(acPromptFormat, p_valist);
            printf("\n");
        }

        if (pst_out_file)
        {
            fflush(pst_out_file);
        }


        //  release token of log usage
        i_ret = pthread_rwlock_unlock(&sg_rwlock_log_token);
    	if(i_ret != 0)
    	{ 

            return;
    	}

	}
}


#if 0
int ut_log(int priority,
           int logModule,
           int loglevel,
           const char *message,
           ...)
{
    va_list ap;
    char buffer[4196];
    int len = 0;
	//sys_related_parameters *g_sys_parameters = &ipgw_mgr->cfg.ut_param.g_sys_parameters ;


  /*  if (loglevel > g_sys_parameters->log_level_limit) {
        return 0;
    }*/


    va_start(ap, message);

    memset(buffer, 0, sizeof buffer);

	
    switch (logModule) 
	{

        case LOG_MODULE_MAIN:
            strcat(buffer, " [MAIN] ");
            break;
        case LOG_MODULE_RX:
            strcat(buffer, " [RX] ");
            break;
        case LOG_MODULE_ROUTER:
            strcat(buffer, " [ROUTER] ");
            break;
        case LOG_MODULE_ENCAP:
            strcat(buffer, " [ENCAP] ");
            break;
        case LOG_MODULE_DECAP:
            strcat(buffer, " [DECAP] ");
            break;
        case LOG_MODULE_DEFAULT:
            strcat(buffer, " [DEFAULT MODULE] ");
            break;
        default:
            strcat(buffer, " [DEFAULT MODULE] ");
            break;
    }

	len = strlen(buffer);

    switch (loglevel) 
	{

        case LOG_EMERG:
            strcat(buffer, " EMER: ");
            break;
        case LOG_ALERT:
            strcat(buffer, " ALER: ");
            break;
        case LOG_CRIT:
            strcat(buffer, " CRIT: ");
            break;
        case LOG_ERR:
            strcat(buffer, " ERROR: ");
            break;
        case LOG_WARNING:
            strcat(buffer, " WARN: ");
            break;
        case LOG_NOTICE:
            strcat(buffer, " NOTICE: ");
            break;
        case LOG_INFO:
            strcat(buffer, " INFO: ");
            break;
        case LOG_DEBUG:
            strcat(buffer, " DEBG: ");
            break;
        default:
            strcat(buffer, " WARN: ");
            break;
    }

	len = strlen(buffer);

    vsprintf(buffer + len, message, ap);
    if (strlen(buffer) >= sizeof(buffer) - 1) {
        syslog(LOG_CRIT, "UT_LOG buffer oversize");
    }


    syslog(priority, "%s", buffer);


    va_end(ap);

    return 0;
}
#endif

#if 1

int ut_log(int priority,
           int logModule,
           int loglevel,
           const char *message,
           ...)
{
    va_list ap;
    char buffer[4196];
    int len = 0;
	RTE_SET_USED(loglevel);

    va_start(ap, message);

	router_printf(priority, logModule, message, ap);
	(void)buffer;
	(void)len;



    va_end(ap);

    return 0;
}

#endif

