
#include <unistd.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>


#include "stats.h"
#include "ring_queue.h" 
#include "router.h"
int allow_debug_levels[MODOULE_END];
volatile struct ut_realtime_show ut_statis_debug;

uint64_t inroute_bytes[MAX_DEVICE_NUM] = {0};
uint64_t outroute_bytes[MAX_DEVICE_NUM] = {0};
uint64_t inroute_thoughput[MAX_DEVICE_NUM] = {0};
uint64_t outroute_thoughput[MAX_DEVICE_NUM] = {0};
extern struct rte_ring* rcv_ring[MAX_DEVICE_NUM];         	 
extern struct rte_ring* encap_ring[MAX_DEVICE_NUM];      
extern struct rte_ring* snd_bb_ring[MAX_DEVICE_NUM];     

const char path[]="router_unix_sock";


int cal_fwd_throughput(uint16_t DeviceId, Pack_InOut direction, uint8_t interval)
{
    static uint64_t inroute_bytes_last[MAX_DEVICE_NUM] = {0};
    static uint64_t outroute_bytes_last[MAX_DEVICE_NUM] = {0};
	
    uint64_t tmp_bytes[2] = {0};
	uint64_t data_bytes_tmp = 0;
    switch (direction) {
        case DIRECTION_IN:
			data_bytes_tmp = inroute_bytes[DeviceId];
            if(inroute_bytes_last[DeviceId] <= data_bytes_tmp) {
                tmp_bytes[DeviceId] = data_bytes_tmp - inroute_bytes_last[DeviceId];
				inroute_bytes_last[DeviceId] =  data_bytes_tmp ;
            }

			
			if(0 != tmp_bytes[DeviceId]) {
				inroute_thoughput[DeviceId] = ((uint64_t)(8 * tmp_bytes[DeviceId]))/1000/interval; // ((float)duration / rte_get_tsc_hz()); /* Bps-->Kbps */
			}
			else
			{
                inroute_thoughput[DeviceId] = 0;
			}
			
            break;
        case DIRECTION_OUT:
			data_bytes_tmp = outroute_bytes[DeviceId];
            if(outroute_bytes_last[DeviceId] <= data_bytes_tmp) {
                tmp_bytes[DeviceId] = data_bytes_tmp - outroute_bytes_last[DeviceId];
				outroute_bytes_last[DeviceId] =  data_bytes_tmp ;
            }

			
			if(0 != tmp_bytes[DeviceId]) {
				outroute_thoughput[DeviceId] = ((uint64_t)(8 * tmp_bytes[DeviceId]))/1000/interval; // ((float)duration / rte_get_tsc_hz()); /* Bps-->Kbps */
			}
			else
			{
                outroute_thoughput[DeviceId] = 0;
			}
			
			
            break;

    }



    return 0;
}


void show_statistics(void)
{

	printf("\n\n");

	for(uint8_t i = 0; i<2; i++)
	{
	    for(uint8_t j = 0; j<MAX_DEVICE_NUM; j++)
	    {
            printf("%-15lu rcv pkt[%d][%d] num\n",       ut_statis_debug.rcv_pkt_num[i][j], i, j);
	    }
	}
	printf("\n\n");


	for(uint8_t j = 0; j<MAX_DEVICE_NUM; j++)
	{
        printf("%-15lu bb_ctl_num device_id:%d\n",       ut_statis_debug.bb_ctl_num[j],j  );
	    printf("%-15lu bb_data_num device_id:%d\n",       ut_statis_debug.bb_data_num[j],j  );
	}
	printf("%-15lu isl_crc_filter_num\n",       ut_statis_debug.isl_crc_filter_num  );
	printf("%-15lu gse_type_filter_num\n",       ut_statis_debug.gse_type_filter_num    );
	printf("%-15lu gse_mac_filter_num\n",   ut_statis_debug.gse_mac_filter_num );
	printf("%-15lu device_id_filter_num\n",   ut_statis_debug.device_id_filter_num );
	printf("%-15lu ctl_msg_num\n",   ut_statis_debug.ctl_msg_num );
	printf("%-15lu mcs_mac_filter_num\n",   ut_statis_debug.mcs_mac_filter_num );

	
	printf("=================rcv ring=================\n");
	for(int i = 0; i < g_st_app_conf.rcv_ring_queue_num; i++)
	{
	    printf("%-15lu enqueue rcv pkt num %d\n",   ut_statis_debug.pkt_enqueue_rcv_ring_num[i], i);
		printf("%-15lu drop_pkt_enqueue_rcv_ring_num %d\n",   ut_statis_debug.drop_pkt_enqueue_rcv_ring_num[i], i);
		printf("%-15lu pkt_dequeue_rcv_ring_num %d\n",   ut_statis_debug.pkt_dequeue_rcv_ring_num[i], i);
	}
	
	printf("=================encap ring=================\n");
	for(int i = 0; i < g_st_app_conf.encap_ring_queue_num; i++)
	{
	    printf("%-15lu pkt_enqueue_encap_ring_num %d\n",   ut_statis_debug.pkt_enqueue_encap_ring_num[i], i);
		printf("%-15lu drop_pkt_enqueue_encap_ring_num %d\n",   ut_statis_debug.drop_pkt_enqueue_encap_ring_num[i], i);
		printf("%-15lu pkt_dequeue_encap_ring_num %d\n",   ut_statis_debug.pkt_dequeue_encap_ring_num[i], i);
	}

	for(int i = 0; i < g_st_app_conf.encap_ring_queue_num; i++)
	{
	    printf("%-15lu pkt_enqueue_snd_bb_ring_num %d\n",   ut_statis_debug.pkt_enqueue_snd_bb_ring_num[i], i);
		printf("%-15lu drop_pkt_enqueue_snd_bb_ring_num %d\n",   ut_statis_debug.drop_pkt_enqueue_snd_bb_ring_num[i], i);
		printf("%-15lu pkt_dequeue_snd_bb_ring_num %d\n",   ut_statis_debug.pkt_dequeue_snd_bb_ring_num[i], i);
	}
	

	printf("%-15lu sent_pkt_num 0\n",   ut_statis_debug.sent_pkt_num[0]);
	printf("%-15lu sent_pkt_num 1\n",   ut_statis_debug.sent_pkt_num[1]);
	printf("%-15lu resent_pkt_num\n",   ut_statis_debug.resent_pkt_num);
	printf("%-15lu fwd_pdu_num\n",   ut_statis_debug.fwd_pdu_num);
	printf("%-15lu rtn_pdu_num\n",   ut_statis_debug.rtn_pdu_num);
	printf("%-15lu force_send_num\n",   ut_statis_debug.force_send_num);
    printf("%-15lu test_pdu_num:0\n",   ut_statis_debug.test_pkt_num[0]);

	printf("%-15lu test_pdu_num:1\n",   ut_statis_debug.test_pkt_num[1]);
	printf("\n");

}


void encap_decap_statistics(void)
{
	printf("\n\n");
   
	
	for(uint8_t j = 0; j<MAX_DEVICE_NUM; j++)
	{ 
	    printf("%-15lu bb_crc_filter_num device_id:%d\n",       ut_statis_debug.bb_crc_filter_num[j],j);
	    printf("%-15lu bb_ctl_num device_id:%d\n",       ut_statis_debug.bb_ctl_num[j],j);
	    printf("%-15lu bb_data_num device_id:%d\n",       ut_statis_debug.bb_data_num[j],j);
	}
	
	printf("\n\n");
	for(uint8_t j = 0; j<MAX_DEVICE_NUM; j++)
	{
	    printf("%-15lu rcv_mpe_data_num device_id:%d\n", 	  ut_statis_debug.rcv_mpe_data_num[j],j);
	    printf("%-15lu snd_mpe_data_num device_id:%d\n",       ut_statis_debug.snd_mpe_data_num[j],j);
        printf("%-15lu decap frame num device_id:%d\n",   ut_statis_debug.decap_frame_num[j],j);
        printf("%-15lu decap gse num device_id:%d\n",   ut_statis_debug.decap_gse_num[j],j);
	}
	printf("\n\n");

	printf("%-15lu total frag num\n",       ut_statis_debug.total_frag_num  );

	printf("%-15lu finish fragment packets\n",   ut_statis_debug.fragment_count_finish );
	for(uint8_t j = 0; j<MAX_DEVICE_NUM; j++)
	{
	    printf("%-15lu finish fragment_ok packets device_id:%d\n",   ut_statis_debug.fragment_count_finish_ok[j],j );
	    printf("%-15lu unfrag pkt num device_id:%d\n",       ut_statis_debug.unfrag_pkt_num[j],j  );
		printf("%-15lu start fragment num device_id:%d\n",		ut_statis_debug.fragment_count_start[j],j  );
		printf("%-15lu end	 fragment num device_id:%d\n",		ut_statis_debug.fragment_count_end[j],j	  );

	}

	printf("%-15lu start fragment ok num\n",       ut_statis_debug.fragment_count_start_ok  );

	printf("%-15lu free conflict pkt num\n",   ut_statis_debug.free_confict_num );
	printf("%-15lu frag timeout pkt num\n",   ut_statis_debug.frag_timeout_num );
	printf("%-15lu reasemble pdu failed\n",   ut_statis_debug.reasemble_pdu_failed);
	printf("%-15lu alloc decap pktmbuf pool failed\n",   ut_statis_debug.alloc_decap_pktmbuf_pool_failed);
	
	printf("\n\n");
	printf("%-15lu encap pkt num\n",       ut_statis_debug.encap_pkt_num  );
	for(uint8_t j = 0; j<MAX_DEVICE_NUM; j++)
	{
	    printf("%-15lu encap frame num device_id:%d\n",   ut_statis_debug.encap_frame_count[j], j);
	    printf("%-15lu encap gse num device_id:%d\n",   ut_statis_debug.encap_gse_count[j], j);
		printf("%-15lu encap start fragment num device_id:%d\n",       ut_statis_debug.encap_start_frag_num[j], j  );
	    printf("%-15lu encap end fragment num device_id:%d\n",       ut_statis_debug.encap_end_frag_num[j], j  );
	    printf("%-15lu mbuf_free_normal_num device_id:%d\n",   ut_statis_debug.mbuf_free_normal_num[j], j);
	}



	printf("\n\n");
	printf("%-15lu encap pdu num\n",   ut_statis_debug.encap_pdu_num);
	for(uint8_t j = 0; j<MAX_DEVICE_NUM; j++)
	{
	    printf("%-15lu decap pdu num device_id:%d\n",   ut_statis_debug.decap_pdu_num[j], j);
	}

	printf("\n");

}


void delay_time_statistics(void)
{
	printf("\n\n");
	
	for(uint8_t j = 0; j<MAX_DEVICE_NUM; j++)
	{ 
	    printf("%-15lu min_delay_metric device_id:%d\n",       ut_statis_debug.min_delay_metric[j],j);
	    printf("%-15lu max_delay_metric device_id:%d\n",       ut_statis_debug.max_delay_metric[j],j);
	    printf("%-15lu avg_delay_metric device_id:%d\n",       ut_statis_debug.avg_delay_metric[j],j);
	}
}

void indication_statistics(void)
{
	printf("\n\n");
	

	printf("%-15lu total_indication_num\n",       ut_statis_debug.total_indication_num);
	printf("%-15lu indication_zero_num\n",       ut_statis_debug.indication_zero_num);
	printf("%-15lu free_indication_num\n",       ut_statis_debug.free_indication_num);
	printf("%-15lu used_indication_num\n",       ut_statis_debug.used_indication_num);
	printf("%-15lu total_indication_bytes\n",       ut_statis_debug.total_indication_bytes);
	printf("%-15lu total_bb_bytes\n",       ut_statis_debug.total_bb_bytes);
	
}



void ring_count_statistics(void)
{
    uint64_t rcv_ring_count;
	uint64_t encap_ring_count;
	printf("\n\n");
	
	for(uint8_t j = 0; j<g_st_app_conf.encap_ring_queue_num; j++)
	{ 
		encap_ring_count = rte_ring_count(encap_ring[j]);   
		printf("%-15lu encap_ring_count device_id:%d\n",	encap_ring_count,j);
	}

	for(uint8_t j = 0; j<g_st_app_conf.encap_ring_queue_num; j++)
	{ 
		encap_ring_count = rte_ring_count(snd_bb_ring[j]);   
		printf("%-15lu snd_bb_ring_count device_id:%d\n",	encap_ring_count,j);
	}
	
	printf("\n\n");
	for(uint8_t j = 0; j<g_st_app_conf.rcv_ring_queue_num; j++)
	{ 
	    rcv_ring_count = rte_ring_count(rcv_ring[j]);
	    printf("%-15lu rcv_ring_count device_id:%d\n",      rcv_ring_count,j);

	}
}




void debug_log(int output, FILE* file, const char *format, ...)
{
	if(output)
	{
		va_list arg_ptr;
		int count;

		va_start(arg_ptr, format);	/* 获取可变参数列表 */

		count = vfprintf(file, format, arg_ptr);
		fflush(file);

		va_end(arg_ptr);			 /* 可变参数列表结束 */
	}
}

extern char g_ach_ara_dir[1024] ;
void *show_statistics_srv_thread(void *pv_arg)
{
    printf("i am show_statistics_srv_thread\n");
    int server_fd,client_fd;
    char buffer[1024]   = {0};
	strcat(buffer, g_ach_ara_dir);
    strcat(buffer, "/router_unix_sock");
    struct sockaddr_un server_addr, client_addr;
    unlink(buffer);
    server_fd = socket(AF_UNIX,SOCK_STREAM,0);
    if(server_fd == -1){
        perror("socket: ");
        exit(1);
    }
    server_addr.sun_family=AF_UNIX;
    strcpy(server_addr.sun_path,buffer);
    if(bind(server_fd,(struct sockaddr *)&server_addr,sizeof(server_addr))==-1){
        perror("bind: ");
        exit(1);
    }
    listen(server_fd,10);  //server listen most 10 requests.
    puts("server is listening: ");
	while (1)
	{
		
    int client_len=sizeof(client_addr);
    client_fd=accept(server_fd,(struct sockaddr *)&client_addr,(int *)&client_len);
    if(client_fd == -1){
        perror("accept: ");
        exit(1);
    }
    char recv_cmd[105], send[105] = "don't find corresponding command";
	char msg[20000];
	struct ut_realtime_show *statis_debug;
    statis_debug = (struct ut_realtime_show *)msg;
	
    int i;
    while(1){
        memset(recv_cmd,0,sizeof(recv_cmd));
        if(read(client_fd,recv_cmd,105)==-1){
            perror("read: ");
            break;
        }

		if(recv_cmd[0] == '\0')
		{
            break;
		}

		if( (strcmp(recv_cmd, "s\n") == 0)
			|| (strcmp(recv_cmd, "ed\n") == 0)
			|| ( strcmp(recv_cmd, "dlt\n") == 0)
			|| ( strcmp(recv_cmd, "rc\n") == 0))
		{
			memcpy(statis_debug, &ut_statis_debug, sizeof(struct ut_realtime_show));
			if(write(client_fd,msg,sizeof(struct ut_realtime_show))==-1)
			{
                perror("write: ");
                break;
            }
		}
		else if(strcmp(recv_cmd,"quit\n")==0)
	    {
            printf("the server process end.\n");
            break;
        }
        else
		{  
		    if(write(client_fd,send,strlen(send))==-1)
		    {
                perror("write: ");
                break;
		    }
        }
        /*printf("recv message from client: %s",recv_cmd);
        memset(send,0,sizeof(send));
        if(read(STDIN_FILENO,send,sizeof(send))==-1){
            perror("read: ");
            break;
        }
        if(write(client_fd,send,strlen(send))==-1){
            perror("write: ");
            break;
        }*/
    }

	printf("link end\n");
		}
	    close(server_fd);
    unlink(buffer);
}

void *show_statistics_cli_thread(void *pv_arg)
{
    printf("i am show_statistics_cli_thread\n");


    int server_fd,client_fd;
    struct sockaddr_un server_addr, client_addr;
    //unlink(path);
    server_fd = socket(AF_UNIX,SOCK_STREAM,0);
    if(server_fd == -1){
        perror("socket: ");
        exit(1);
    }
    server_addr.sun_family=AF_UNIX;
    strcpy(server_addr.sun_path,path);
    if(connect(server_fd,(struct sockaddr *)&server_addr,sizeof(server_addr)) == -1){
        perror("connect: ");
        exit(1);
    }
    char recv[105],send_cmd[105];
    int i;
	char msg[20000];
	struct ut_realtime_show *statis_debug;
    statis_debug = (struct ut_realtime_show *)msg;
    puts("the client started, please enter your message: ");
    while(1){
        memset(send_cmd,0,sizeof(send_cmd));
		memset(recv,0,strlen(recv));
        if(read(STDIN_FILENO,send_cmd,105)==-1){
            perror("read: ");
            break;
        }
        if(write(server_fd,send_cmd,strlen(send_cmd))==-1){
            perror("send: ");
            break;
        }
		if( (strcmp(send_cmd, "s\n") == 0)
			|| (strcmp(send_cmd, "ed\n") == 0)
			|| ( strcmp(send_cmd, "dlt\n") == 0)
			|| ( strcmp(send_cmd, "rc\n") == 0))
		{

		    if(read(server_fd,msg,sizeof(struct ut_realtime_show))==-1)
		    {
                perror("read: ");
                break;
            }
            statis_debug = (struct ut_realtime_show*) msg;
			if( strcmp(send_cmd, "s\n") == 0 || strcmp(send_cmd, "statistics") == 0 ) 
			{
			    memcpy(&ut_statis_debug, statis_debug, sizeof(struct ut_realtime_show));
			    show_statistics();
		    }
		    else if( strcmp(send_cmd, "ed\n") == 0)
			{
			    encap_decap_statistics();
		    }
		    else if( strcmp(send_cmd, "dlt\n") == 0)
		    {
                delay_time_statistics();
		    }
		    else if( strcmp(send_cmd, "rc\n") == 0)
		    {
                ring_count_statistics();
		    }
	    } 
		else if(strcmp(send_cmd,"quit\n")==0)
		{
            printf("the client process end.\n");
            break;
        }
		else
		{
            if(read(server_fd,recv,105)==-1)
		    {
                printf("%s\n",recv);
                break;
            }
		}
		
       

		printf("%s\n",recv);
    
       
    }
    close(server_fd);
    unlink(path);
    exit(0) ;
}



