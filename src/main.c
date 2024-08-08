#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <signal.h>


#include "global_var.h"
#include "decapsulate.h"
#include "encapsulate.h"
#include "sys_thread.h"
#include "stats.h"
#include "general_module.h"
#include "ring_queue.h"
#include "router.h"
#include "tun.h"
#include "rcv_send.h"
#include "receive.h"
#include "router_libcap.h"
#include "show_version.h"
#define ELEM_CNT     8192U                   // must be power of 2
#define RING_CAP     (ELEM_CNT*2)


FILE* test_file_record_1;
FILE* test_file_record_2;
volatile int     force_quit;
uint8_t little_endian;



extern ROUTERHEADER router_header[MAX_DEVICE_NUM];

static void signal_handler(int signum)
{
	if (signum == SIGINT || signum == SIGTERM) 
	{
		printf("\n\nSignal %d received, preparing to exit...\n", signum);
		show_statistics();
		force_quit = true;
	}
}

static
void show_console(void)
{

	char input[128] = { 0 };
	while(!force_quit) {
		printf("[Choice]: ");
		if(scanf("%s", input) < 0) {
			printf("Error input\n");
		       	continue;
		}
		// show statistics 
		else if( strcmp(input, "s") == 0 || strcmp(input, "statistics") == 0 ) {
			show_statistics();
		}
		else if( strcmp(input, "ed") == 0) {
			encap_decap_statistics();
		}
		else if( strcmp(input, "dlt") == 0)
		{
            delay_time_statistics();
		}
		else if( strcmp(input, "rc") == 0)
		{
            ring_count_statistics();
		}
		else if( strcmp(input, "id") == 0)
		{
            indication_statistics();
		}

		
		// quit app	
		else if ( strcmp(input, "q") == 0 || strcmp(input, "quit") == 0 ) {
			show_statistics();
				   show_pcap_state();
		    force_quit = true;
		}
    }
}


static void trim_file_name(char* vpch_path)
{
    if (vpch_path)
    {
        char* pch_worker = vpch_path;
        char* pch_note = vpch_path;

        for (; *pch_worker != '\0'; pch_worker++)
        {
            if (*pch_worker == '/')
            {
                pch_note = pch_worker;
            }
        }

        if (vpch_path == pch_note)
        {
            //  root dir
            return;
        }

        *pch_note = '\0';
    }
}

char g_ach_ara_dir[1024] = {0};
#define ARA_CFG_DIR   		"../etc/"
#define ARA_CFG_FILE  		"router.conf"

static void ara_get_router_dir(char* vpch_src)
{
    //char ac_my_cmd[640] = {0};
    //char ac_start_path[512] = {0};
    
    memset((void*)g_ach_ara_dir, 0x00, sizeof(g_ach_ara_dir));
    
    if (vpch_src)
    {
        strcpy(g_ach_ara_dir, vpch_src);
        trim_file_name(g_ach_ara_dir);//  trim '/pmu'
        if (0 != strcmp(g_ach_ara_dir, "."))
        {
            trim_file_name(g_ach_ara_dir); //  trim '/sbin'
        }
        else
        {
            memset((void*)g_ach_ara_dir, 0x00, sizeof(g_ach_ara_dir));
            strcpy(g_ach_ara_dir, "..");
        }
    }
    else
    {
        strcpy(g_ach_ara_dir, ARA_CFG_DIR);
        trim_file_name(g_ach_ara_dir);//  trim '/'
        trim_file_name(g_ach_ara_dir);//  trim '/etc'
    }
}

#ifdef cmdline

int main(int argc, char* argv[])
{
    char buffer[1024]   = {0};
	RTE_SET_USED(argc);
	RTE_SET_USED(argv);
	test_file_record_1 = fopen("./record_result_1.txt","w");
    test_file_record_2 = fopen("./record_result_2.txt","w");
	force_quit = false;
	
    ara_get_router_dir(argv[0]);
	router_log_init();
	init_sys_log();
	read_command_line_params(argc, argv);
  
	printf("ut release %s\n",g_ac_version);

	LOG(LOG_NOTICE, LOG_MODULE_MAIN, 0,
                LOG_CONTENT("ut release %s"),g_ac_version);
	
	create_rcv_pktmbuf_pool();
	signal(SIGINT, signal_handler);
	signal(SIGTERM, signal_handler);

	little_endian = is_little_endian();

	strcat(buffer, g_ach_ara_dir);
    strcat(buffer, "/etc/");

	read_nic_config("base.conf",buffer);
	read_mapping_tbl_config("mapping_tbl.conf",buffer);
	read_router_tbl_config("router.conf",buffer);
	init_encap_mem();
	init_decap_mem();
	//init_send_ring();
	//init_rcv_ring();
	//initVtnl();
	
    
    create_ring();
	init_router_header(DIRECTION_RTN);
	init_rcv_snd();
	init_libpcap();
    create_single_threads();
	create_multi_threads();
	
    show_console();
    

	return 0;
}
#else
int main(int argc, char* argv[])
{
	RTE_SET_USED(argc);
	RTE_SET_USED(argv);
  
	printf("ut release %s\n",g_ac_version);
	

    create_single_threads();


	while (true)
	{
		sleep(1000);
	}

	return 0;
}
#endif

                
