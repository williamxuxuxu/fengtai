#include <unistd.h>

#include "ring_queue.h"
#include "receive.h"
#include "define_const.h"
#include "global_var.h"
#include "common_dpdk.h"
#include "stats.h"
#include "pdu_header.h"
#include "general_module.h"
#include "rcv_send.h"
#include "sys_thread.h"
#include "router_libcap.h"
#include "tun.h"
#include "timer.h"
#include "encapsulate.h"

extern uint64_t inroute_bytes[MAX_DEVICE_NUM];
extern App_Conf    g_st_app_conf;
struct rte_mempool * rcv_pktmbuf_pool = NULL;

void create_rcv_pktmbuf_pool(void)
{
    /* create the mbuf pool */
	rcv_pktmbuf_pool = rte_pktmbuf_pool_create("rcv_mbuf_pool", 12800U, 0, 128,8192,0);
	if(rcv_pktmbuf_pool == NULL) {	
		LOG(LOG_WARNING, LOG_MODULE_RX, 0,
                LOG_CONTENT("Create mempool failed"));
		return;
	}
}

struct rte_mbuf *get_rcv_mbuf(void)
{
    struct rte_mbuf *mbuf = NULL;
    mbuf = rte_pktmbuf_alloc(rcv_pktmbuf_pool);
	if(NULL == mbuf)
	{
                printf("get mbuf failed from rcv pool\n");
		LOG(LOG_WARNING, LOG_MODULE_RX, 0,
                LOG_CONTENT("get mbuf failed from rcv pool"));
		return NULL;
	}
    return mbuf;
}




int get_queue_id(struct rte_mbuf* mbuf)
{
    int queue_id = 0;
    MAC_FRAME_HEADER *mac_frame_header;
   	// 提取MAC header
   	if(NULL == mbuf)
   	{
        return -1;
	}
    mac_frame_header = rte_pktmbuf_mtod(mbuf, MAC_FRAME_HEADER*);
    uint16_t m_cType = ntohs(mac_frame_header->m_cType);
	//for test
	#if 0
    char *send_buffer;
	uint32_t seq_num;
	uint32_t unique_num;
	#endif
	//for test
	#if 1
	/*if( (m_cType != 0x9101) 
		&& (m_cType != 0x9102)
		&& (m_cType != 0x9103)
		&& (m_cType != 0x9104)
		&& (m_cType != 0x0800)
		&& (m_cType != 0x0906))
    {
	    ut_statis_debug.gse_type_filter_num ++;
		//write_pcap_file(3, rte_pktmbuf_mtod(mbuf, uint8_t *), mbuf->data_len);
        return -1;
    }*/
		
    #if 0
	if(m_cType == 0x9103)
	{
		
	    send_buffer = rte_pktmbuf_mtod(mbuf, char *);    
	    send_buffer = send_buffer + 10 + 28 + 24+14;
	    /*memcpy(&seq_num, send_buffer, sizeof(uint32_t));
        seq_num = ntohl(seq_num);
        debug_log(1, test_file_record_2, "%d\n", seq_num);
		send_buffer = send_buffer + 12;*/
	    memcpy(&unique_num, send_buffer, sizeof(uint16_t));
        unique_num = ntohs(unique_num);
		if(unique_num == 12851&&(mbuf->data_len>76))
		{
		    ut_statis_debug.rtn_pdu_num++;
			
		}
	}
	#endif
	#if 0

	for(uint8_t i = 0; i<MAX_DEVICE_NUM; i++)
	{
	    int ret;
	    ret = equal_mac(mac_frame_header->m_cSrcMacAddress, g_st_app_conf.device_mac[i], 6);
	    if(MAC_DIFFERENCE != ret)
	    {
	        #if 0
	        if((mac_frame_header->m_cType == 0x9103)&&(i == 1))
	        {
		        ut_statis_debug.rtn_pdu_num++;	            
	        }
			#endif
	        return i;
			

	    }

	}
	#endif


    #else
    if( mac_frame_header->m_cType != 0x9103 )
    {
        rte_pktmbuf_free(mbuf);
	    ut_statis_debug.gse_type_filter_num ++;
        return -1;
    }
	queue_id = mac_frame_header->m_cSrcMacAddress[3];
    
    #endif
	//printf("get queue_id failed\n");
	return queue_id;
}

extern uint8_t mapping_table_entry_num;

#ifdef TAG_ROUTER
void* receive_msg_thread(void* vpv_arg)
{	
    THREAD_HANDLE* pst_handle = (THREAD_HANDLE*)vpv_arg;
    uint16_t us_serial_number = pst_handle->pParam;
    printf("Hi, I'm thread recv_thread,threadid:%d\n", us_serial_number);
	int ring_id = 0;
	int ret = false;
	uint32_t cur_time;
	uint32_t pre_time;
	uint32_t pre_time_sec;
	uint32_t check_time = g_st_app_conf.check_frag_timeout_period*30;//unit seconds
	uint32_t diff_time;
	uint32_t diff_time_sec;
    uint8_t mark_q[MAX_DEVICE_NUM] = {0};
	uint8_t mark_flag = 0;
	uint8_t mapping_table_index = 0;
	pre_time = get_cur_time_s();
	uint8_t router_startup_flag = 1;

    while(1)
    {
        struct rte_mbuf* mbuf;
		if(0 == router_startup_flag)
		{
	        for(mapping_table_index=0; mapping_table_index<mapping_table_entry_num; mapping_table_index++)
		    {
		        if(g_st_app_conf.dst_mac_flag[mapping_table_index] == 0)
		        {
		            break;
				}
		    }
			
			if(mapping_table_index == mapping_table_entry_num)
			{
	            router_startup_flag = 1;
			}
			else
			{
			    arp_request_dst(mapping_table_index);
			}
		}
		
		mbuf =  read_pcap(us_serial_number);

		if(NULL != mbuf)
		{
			ring_id = get_queue_id(mbuf);
			if(ring_id < 0)
			{
                rte_pktmbuf_free(mbuf);
				continue;
			}

			MAC_FRAME_HEADER *mac_frame_header;
	        mac_frame_header = rte_pktmbuf_mtod(mbuf, MAC_FRAME_HEADER*);
	        uint16_t m_cType = ntohs(mac_frame_header->m_cType);
			if(0 == router_startup_flag)
			{
				if(m_cType == RTE_ETHER_TYPE_ARP)
				{
	                arp_in(us_serial_number, mbuf);
					
				}
				else
				{
					rte_pktmbuf_free(mbuf);
				}
				continue;
			}
			else
			{
                if(m_cType == RTE_ETHER_TYPE_ARP)
				{
	               rte_pktmbuf_free(mbuf);
				   continue;
				}
			}

			ring_id = us_serial_number;
			ut_statis_debug.rcv_pkt_num[us_serial_number][ring_id]++;
			inroute_bytes[ring_id] += mbuf->data_len;
			
  #if 0
	        cur_time = get_cur_time_s();
	        diff_time = cur_time - pre_time;
	        if (unlikely(diff_time > check_time))
	        {
	           if(0 == pre_time_sec)
	           {
                   pre_time_sec = cur_time;
			   }

			   diff_time_sec = cur_time - pre_time_sec;
			   if(1 == mark_q[ring_id])
               {
                   Mbuf_Private_Header * mph = (Mbuf_Private_Header*)rte_mbuf_to_priv(mbuf);
                   mph->inrouter_time = get_cur_time_us();
                       #if 0
                   struct timeval tm_current = {0};
                   gettimeofday(&tm_current, NULL);
				   mph->inrouter_time  = (uint64_t)((uint64_t)tm_current.tv_sec *USECONDS_PER_SECOND + tm_current.tv_usec);
				   #endif
				   //printf("mph->inrouter_time:%lu,us_serial_number:%d",mph->inrouter_time, us_serial_number);
				   mph->delay_time_mark_flag = 1;
			       mark_flag = 1;
			       mark_q[ring_id] = 0;
				   //printf(" mark_q[%d]\n",ring_id);
			   }
			   
	           if(diff_time_sec > 2)
	           {
	              //printf("come in %ds\n", diff_time_sec);
                  mark_flag = 0;   
				  pre_time_sec = 0;
	              pre_time = cur_time;
			   }

	        }

			if(0 == mark_flag)
			{
			    mark_q[ring_id] = 1;
			}
			
			
            
            #endif
            ret = enqueue_rcv_ring(mbuf, us_serial_number);
			if(true != ret)
			{
                printf("enqueue failed\n");
				rte_pktmbuf_free(mbuf);
				continue;
			}
		
	
		}

		
    }

	return NULL;
}
#else
void* receive_msg_thread(void* vpv_arg)
{	
    THREAD_HANDLE* pst_handle = (THREAD_HANDLE*)vpv_arg;
    uint16_t us_serial_number = pst_handle->pParam;
    printf("Hi, I'm thread recv_thread,threadid:%d\n", us_serial_number);
	int ring_id = 0;
	int ret = false;
	uint16_t m_cType;
	MAC_FRAME_HEADER *mac_frame_header;


    while(1)
    {
        struct rte_mbuf* mbuf;	
		mbuf =  read_pcap(us_serial_number);
		if(NULL != mbuf)
		{
			
	        mac_frame_header = rte_pktmbuf_mtod(mbuf, MAC_FRAME_HEADER*);
	        m_cType = ntohs(mac_frame_header->m_cType);
			if(0 == us_serial_number)
			{
	            if(0x9101 != m_cType)
	            {
	                rte_pktmbuf_free(mbuf);
					continue;
				}
			}
		
			ring_id = us_serial_number;
			ut_statis_debug.rcv_pkt_num[us_serial_number][ring_id]++;
		
			

            ret = enqueue_rcv_ring(mbuf, us_serial_number);
			if(true != ret)
			{
                printf("enqueue failed\n");
				rte_pktmbuf_free(mbuf);
				continue;
			}
		
	
		}

		
    }

	return NULL;
}
#endif

void* receive_tun_thread(void* vpv_arg)
{	
    RTE_SET_USED(vpv_arg);
    printf("Hi, i am receive_tun_thread\n");

    while(1)
    {
        char buffer[MAX_TUN_BUFFER] = {0};
		int16_t pkt_len = recvFromVtnl(buffer, MAX_TUN_BUFFER);
		if(pkt_len > 0)
		{
		    snd_tun_pdu(buffer, pkt_len);
		}
		
    }

	return NULL;
}

