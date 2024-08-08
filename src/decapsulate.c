#include <stdio.h>
#include <malloc.h>
#include <error.h>
#include <sys/ioctl.h>

#include "decapsulate.h"
#include "define_struct.h"
#include "define_const.h"
#include "general_module.h"
#include "pdu_header.h"
#include "list_template.h"
#include "sys_thread.h"
#include "global_var.h"
#include "stats.h"
#include "ether.h"
#include "system_log.h"
#include "router.h"
#include "ring_queue.h"
#include "common_dpdk.h"
#include "timer.h"
#include "encapsulate_frame.h"

#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wunused-value"
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"



// 测试：读文件获取codeblock
uint8_t  test_code_block[7500];			// 用文件模拟测试时使用，保存一个code block的内容
uint32_t test_total_read_size = 0;		// 已经从BBFrame读出的总长度
uint8_t  test_read_end = 0;
uint64_t test_old_N = 0;
uint64_t test_N =0;

FRAG_INFO g_frag_info[MAX_DEVICE_NUM][MAX_FRAG_ID][FRAG_BUFFER_NUM];
uint64_t  frag_timeout_threshold_us;
LINKLIST* frag_buffer_index_list[MAX_DEVICE_NUM][MAX_FRAG_ID];
LINKNODE* frag_buffer_index_node[MAX_DEVICE_NUM][MAX_FRAG_ID][FRAG_BUFFER_NUM];


PREVIOUS_MAC_INFO previous_mac_info;

volatile uint64_t indication_total_num        = 0;

rte_atomic32_t available_fpga_buff;


extern int mac_updating;

extern volatile int force_quit;
uint32_t pkt_count = 0;


struct rte_mempool * decap_pktmbuf_pool = NULL;



void init_frag_info(FRAG_INFO frag_info[][MAX_FRAG_ID][FRAG_BUFFER_NUM])
{
	int i,j,k,l;

	for(i=0; i<MAX_DEVICE_NUM; i++)
	{

	    for(j=0; j<MAX_FRAG_ID; j++)
	    {
	    	for(k=0; k<FRAG_BUFFER_NUM; ++k)
	    	{
		    	frag_info[i][j][k].max_frag_seq = 0;
		    	frag_info[i][j][k].pdu_recv_len = 0;
		    	frag_info[i][j][k].frag_recv_num = 0;

		    	for(l=0; l<FRAG_MAX_NUM; l++)
		    	{
		     		frag_info[i][j][k].frag_len[l] = 0;
		    		frag_info[i][j][k].data_addr[l] = (uint8_t*)malloc(FRAG_MAX_LENGTH);
		    	}

		    	memset(&(frag_info[i][j][k].pdu_node), 0, sizeof(DE_PDU_NODE));
		    	frag_info[i][j][k].recv_end = 0;
		    	frag_info[i][j][k].crc = 0;
		    }
	    }
	}
}

void reset_frag_info(FRAG_INFO* ptr_frag_info)
{
	if(!ptr_frag_info)
		return;

	ptr_frag_info->max_frag_seq = 0;
	ptr_frag_info->pdu_recv_len = 0;
	ptr_frag_info->frag_recv_num = 0;

	memset(ptr_frag_info->frag_len, 0, sizeof(uint16_t)*FRAG_MAX_NUM);

	ptr_frag_info->recv_end = 0;
	ptr_frag_info->t0_us = 0;
	ptr_frag_info->t1_us = 0;
	ptr_frag_info->timeout_flag = 0;
// 	if(NULL != ptr_frag_info->pdu_node.ptr_pdu)
// 	{
// 		rte_pktmbuf_free((struct rte_mbuf*)(ptr_frag_info->pdu_node.ptr_pdu));
		ptr_frag_info->pdu_node.ptr_pdu = NULL;
// 	}
	ptr_frag_info->pdu_node.pdu_length = 0;
    ptr_frag_info->time_stamp = 0;
	ptr_frag_info->crc = 0;
	// ptr_frag_info->frame_seq_num = 0;
}

static void init_queue(LINKLIST** pQueue)
{
	Free_List(*pQueue, 0);	// 防止pQueue是一个已用的内存，先将其代表的队列释放
	*pQueue = Init_List();	// 动态生成一个 LINKLIST 结构
}


void init_decap_mem(void)
{
	// 生成多项式 x32 + x26 + x23 + x22 + x16 + x12 + x11 + x10 + x8 + x7 + x5 + x4 + x2 + x + 1
	unsigned int poly_32 = 0x04C11DB7; 	// 1 0000 0100 1100 0001 0001 1101 1011 0111 
	// 生成多项式 X8+X5+X4+1
	unsigned char poly_8 = 0x31; 		// 1 1101 0101
	crc8_init_table(poly_8);
	crc32_init_table(poly_32);

	little_endian = is_little_endian();
	printf("CPU is %s\n", little_endian ? "little endian" : "big endian");

	int i, j, k;
	int8_t* frag_buffer_index_value;


    for(i = 0; i<MAX_DEVICE_NUM; i++)
    {
	    for(j=0; j<MAX_FRAG_ID; ++j)
	    {
		    init_queue(&frag_buffer_index_list[i][j]);

		    for(k=0; k<FRAG_BUFFER_NUM; ++k)
		    {
		    	frag_buffer_index_value = (int8_t*)malloc(sizeof(int8_t));
		    	if(frag_buffer_index_value == NULL)
		    	{
			    	printf("No Enough Memory to malloc frag_buffer_index_value! \n");
		    		free(frag_buffer_index_value);
		     		return;
		    	}
		    	*frag_buffer_index_value = k;

		    	frag_buffer_index_node[i][j][k] = (LINKNODE*)malloc(sizeof(LINKNODE));
			    if(frag_buffer_index_node[i][j][k] == NULL)
			    {
			    	printf("No Enough Memory to malloc frag_buffer_index_node! \n");
			    	free(frag_buffer_index_node[j][k]);
			    	return;
		    	}

		    	frag_buffer_index_node[i][j][k]->data_bytes = sizeof(int8_t);
		    	frag_buffer_index_node[i][j][k]->data = (void*)frag_buffer_index_value;
		    	Push_List(frag_buffer_index_list[i][j], frag_buffer_index_node[i][j][k]);
  		    }
    	}
    }
	init_frag_info(g_frag_info);

	/* create the mbuf pool */
	decap_pktmbuf_pool = rte_pktmbuf_pool_create("de_mbuf_pool", 20480U, 0, 128, DECAP_MBUF_SIZE,0);

}

#if 0
static struct rte_mbuf *get_decap_mbuf(void)
{
    struct rte_mbuf *mbuf = NULL;
    mbuf = rte_pktmbuf_alloc(decap_pktmbuf_pool);
	if(NULL == mbuf)
	{
	    LOG(LOG_ERR, LOG_MODULE_RX, LOG_ERR,
	        LOG_CONTENT("get mbuf failed from decap pool\n"));
	}
    return mbuf;
}
#endif



void fragment_timeout(FRAG_INFO frag_info[][MAX_FRAG_ID][FRAG_BUFFER_NUM])
{

	frag_timeout_threshold_us = 5;

	int8_t  index = -1;
	int8_t* ptr_index;
	LINKNODE* pNode;
	uint64_t t_us = get_current_time();
	static uint32_t test_timeout_count = 0;
    for(int i=0; i<MAX_DEVICE_NUM; i++)
    {
	    for(int k=0; k<MAX_FRAG_ID; k++)
	    {
		    ptr_index = (int8_t*)Get_Header_Data(frag_buffer_index_list[i][k], &pNode);
		    if(ptr_index)
		    {
		    	index = *ptr_index;
		    	if(frag_info[i][k][index].t0_us > 0)
		    	{
			    	frag_info[i][k][index].timeout_flag = ((t_us - frag_info[i][k][index].t0_us) >= frag_timeout_threshold_us) ? 1 : 0;
			    }
			    else
			    {
			    	frag_info[i][k][index].timeout_flag = 0;
			    }

			    if(frag_info[i][k][index].timeout_flag)
			    {
			    	ut_statis_debug.frag_timeout_num++;
			    	Remove_Node_point(frag_buffer_index_list[i][k], pNode);
			    	Push_List(frag_buffer_index_list[i][k], pNode);
				
			    	
				    if(frag_info[i][k][index].pdu_node.ptr_pdu)
				    {
					    rte_pktmbuf_free((struct rte_mbuf*)(frag_info[i][k][index].pdu_node.ptr_pdu));

				    }
				    reset_frag_info(&(frag_info[i][k][index]));
				
				    frag_info[i][k][index].timeout_flag = 0;
				
			    }
		    }
	    }
	}
}



L2L1_HEADER* pL2L1Header = 0;	// 调试用


#ifdef TAG_ROUTE
void dispatch_decap_pkt(struct rte_mbuf* mbuf, uint16_t queue_id)
{
    int ret = 0;
	FRAME_PARAM frame_para;

	ret = process_frame_header(mbuf, &frame_para, queue_id);

	if(ret == -1)
	{
	    printf("process_frame_header\n");
        return;
	}
	else if(ret == 2)
	{
        return;
	}

	switch (frame_para.pack_type)
	{
		case ISL_PACK:
		case ENUM_FWD_Tx_FWD_Data:
		case ENUM_FWD_Rx_FWD_Data:
			decapsulate_gse(mbuf, &frame_para, queue_id);
			break;

		case MULTI_FWD_PDU_PACK:
		case MULTI_RTN_PDU_PACK:
			 decapsulate_pdu(mbuf, &frame_para,queue_id);
			 break;
	    case FWD_TUNNEL_PDU:
		case RTN_TUNNEL_PDU:
			 rte_pktmbuf_adj(mbuf, sizeof(L2L1_MSG_HEADER));
			 router_func(mbuf);
			 break;
        /*case RTN_TUNNEL_PDU:
	    	 {
	    	     uint16_t router_table_id;
				 ROUTERHEADER *router_header_temp;
				 uint16_t protocol_type;				 	
				 uint64_t inrouter_time = 0;
				 Mbuf_Private_Header * mph = (Mbuf_Private_Header*)rte_mbuf_to_priv(mbuf);
				 protocol_type = mph->protocol_type;
	             if(1 == mph->delay_time_mark_flag)
	             {
                     inrouter_time = mph->inrouter_time;
					 mph->delay_time_mark_flag = 0;
	             }
	    	     rte_pktmbuf_adj(mbuf, sizeof(L2L1_MSG_HEADER));
	             router_header_temp = rte_pktmbuf_mtod(mbuf, ROUTERHEADER *);
                 uint16_t router_header_segment_size = sizeof(uint16_t)*(router_header_temp->LAST_ENTRY + 1);
	             uint16_t router_header_size = ROUTER_HEADER_LEN + sizeof(uint16_t)*(router_header_temp->LAST_ENTRY + 1);
				 struct rte_mbuf* decap_mbuf = rte_pktmbuf_alloc(decap_pktmbuf_pool);
                 if(NULL == decap_mbuf)
                 {
			         printf("decap_pktmbuf_pool:run ot of memory\n");
                 }

				 		
	           	 if(0 != inrouter_time)
		         {
			         mph = (Mbuf_Private_Header*)rte_mbuf_to_priv(decap_mbuf);
			         mph->delay_time_mark_flag = 1; 
			         mph->inrouter_time = inrouter_time;
		         }
				 
                 router_table_id = decide_router_table(mbuf);
				 router_header[router_table_id].S = router_header_temp->S;
				 router_header[router_table_id].T = router_header_temp->T;
				 rte_pktmbuf_adj(mbuf, router_header_size);
				 uint16_t msg_len = frame_para.frame_size - router_header_size;
				 char *send_buffer = (char *)rte_pktmbuf_append(decap_mbuf, g_st_app_conf.router_header_size[router_table_id] + msg_len);	
				 rte_memcpy(send_buffer, &router_header[router_table_id], g_st_app_conf.router_header_size[router_table_id]);
				 rte_memcpy(send_buffer + g_st_app_conf.router_header_size[router_table_id], rte_pktmbuf_mtod(mbuf, uint8_t *), msg_len);
				 mph = (Mbuf_Private_Header*)rte_mbuf_to_priv(decap_mbuf);
	             mph->protocol_type = protocol_type;
			     router_func(decap_mbuf);
				 rte_pktmbuf_free(mbuf);
        	 }
			 break;*/


	    default:
			break;
	}
}

#else
void dispatch_decap_pkt(struct rte_mbuf* mbuf, uint16_t queue_id)
{
    int ret = 0;
	FRAME_PARAM frame_para;

	ret = process_frame_header(mbuf, &frame_para, queue_id);

	if(ret == -1)
	{
	    printf("process_frame_header\n");
        return;
	}
	else if(ret == 2)
	{
        return;
	}

	switch (frame_para.pack_type)
	{
		case ISL_PACK:
		case ENUM_FWD_Tx_FWD_Data:
		case ENUM_FWD_Rx_FWD_Data:
			decapsulate_gse(mbuf, &frame_para, queue_id);
			break;

		case MULTI_FWD_PDU_PACK:
		case MULTI_RTN_PDU_PACK:
			 decapsulate_pdu(mbuf, &frame_para,queue_id);
			 break;
	    case FWD_TUNNEL_PDU:
		case RTN_TUNNEL_PDU:
			 if(false == enqueue_encap_ring(mbuf, queue_id))
	         {
		         rte_pktmbuf_free(mbuf);
		         LOG(LOG_ERR, LOG_MODULE_ROUTER, LOG_ERR,
	                     LOG_CONTENT("enqueue_encap_ring failed"));
          	 }
			 break;
	    default:
			break;
	}
}

#endif

void * decapsulate_thread (void * pv_arg)
{ 

    THREAD_HANDLE* pst_handle = (THREAD_HANDLE*)pv_arg;
    uint16_t us_serial_number = pst_handle->pParam;
    struct rte_mbuf *pkts_burst[MAX_PKT_BURST];
	int nb_rx, j;
	
	uint32_t cur_time = 0;
	uint32_t pre_time = 0;
	uint32_t check_time = g_st_app_conf.check_frag_timeout_period;//unit seconds
	uint32_t diff_time = 0;
	
    if(0 == pre_time)
    {
	    pre_time = get_cur_time_s();
    }
	
	uint8_t loop_count = 0;
	printf("Hi, I'm thread decapsulate_thread,threadid:%d\n", us_serial_number);
	LOG(LOG_NOTICE, LOG_MODULE_DECAP, LOG_NOTICE,
	    LOG_CONTENT("Hi, I'm thread decapsulate_thread,threadid:%d"), us_serial_number);

    do
    {
       if(1 == g_st_app_conf.decap_thread_num)// round robin
       {
           for(int i=0; i<g_st_app_conf.rcv_ring_queue_num; i++)
           {
               nb_rx = dequeue_rcv_ring(pkts_burst, MAX_PKT_BURST, i);
		       if(nb_rx != 0)
		       {
		           loop_count = 0;
                   for(j=0; j<nb_rx; j++) 
			       {
                       dispatch_decap_pkt(pkts_burst[j], i);
                   }
		       }
			   #if 1
		       else
		       {
				    loop_count++;
					if(10 == loop_count)
					{
                        usleep(10);
						loop_count = 0;
					}
			   }
		       #endif
		   }
	   }
	   else if(g_st_app_conf.decap_thread_num > 1)//one on one
	   {
           nb_rx = dequeue_rcv_ring(pkts_burst, MAX_PKT_BURST, us_serial_number);
		   if(nb_rx != 0) /* fwd control message */
		   {
		       
               for(j=0; j<nb_rx; j++) 
			   {
                   dispatch_decap_pkt(pkts_burst[j], us_serial_number);
               }
		   }
	   }
	   

	   cur_time = get_cur_time_s();
	   diff_time = cur_time - pre_time;
	   if (unlikely(diff_time > check_time))
	   {
	       pre_time = cur_time;
		   fragment_timeout(g_frag_info);
	   }
	   
    }while (TRUE);


	LOG(LOG_ERR, LOG_MODULE_DECAP, LOG_ERR,
	    LOG_CONTENT("decapsulate_thread is over"));

    return NULL;
}


void decapsulate_pdu(struct rte_mbuf* pdu_mbuf, 	FRAME_PARAM *frame_para, uint16_t queue_id)
{
    L2L1_MSG_HEADER *l2l1_msg_header;
	uint16_t pdu_total_len = 0;
	uint16_t *data_len;
	uint16_t data_len_tmp;
	uint8_t *ptmp;
	uint16_t pdu_mbuf_len = pdu_mbuf->data_len;
    char *decap_buffer;
    uint16_t m_cType; 
    MAC_FRAME_HEADER *mac_frame_header;

    uint16_t router_table_id;
	ROUTERHEADER *router_header_temp;
	uint16_t protocol_type;
	uint64_t inrouter_time = 0;
	Mbuf_Private_Header * mph = (Mbuf_Private_Header*)rte_mbuf_to_priv(pdu_mbuf);
	protocol_type = mph->protocol_type;
	if(1 == mph->delay_time_mark_flag)
	{
        inrouter_time = mph->inrouter_time;
		mph->delay_time_mark_flag = 0;
	}
	
   	// 提取MAC header
    mac_frame_header = rte_pktmbuf_mtod(pdu_mbuf, MAC_FRAME_HEADER*);
    m_cType = ntohs(mac_frame_header->m_cType);

	//for test
    char *send_buffer;
	uint32_t seq_num;
	uint32_t unique_num;

	
    if(NULL == pdu_mbuf)
    {
        printf("pdu_mbuf is NULL \n");
    }

	rte_pktmbuf_adj(pdu_mbuf, sizeof(L2L1_MSG_HEADER));
   	ut_statis_debug.rcv_mpe_data_num[queue_id]++;
	while(pdu_total_len < frame_para->frame_size)
	{
	    data_len = rte_pktmbuf_mtod(pdu_mbuf, uint16_t *);
        rte_pktmbuf_adj(pdu_mbuf, 2);
		data_len_tmp = ntohs(*data_len);
		pdu_total_len += data_len_tmp + 2;
		if(data_len_tmp > DECAP_MBUF_SIZE)
		{
            rte_pktmbuf_free(pdu_mbuf);
			printf("pdu pkt len exceed mbuf size\n");
			return;
		}

        struct rte_mbuf* decap_mbuf = rte_pktmbuf_alloc(decap_pktmbuf_pool);
        if(NULL == decap_mbuf)
        {
            rte_pktmbuf_free(pdu_mbuf);
			printf("decap_pktmbuf_pool:run ot of memory\n");
		    return;
        }

		
		if(0 != inrouter_time)
		{
			mph = (Mbuf_Private_Header*)rte_mbuf_to_priv(decap_mbuf);
			mph->delay_time_mark_flag = 1; 
			mph->inrouter_time = inrouter_time;
			inrouter_time = 0;
		}

		router_header_temp = rte_pktmbuf_mtod(pdu_mbuf, ROUTERHEADER *);
		uint16_t router_header_segment_size = sizeof(uint16_t)*(router_header_temp->LAST_ENTRY + 1);
		uint16_t router_header_size = ROUTER_HEADER_LEN + sizeof(uint16_t)*(router_header_temp->LAST_ENTRY + 1);
		uint16_t msg_len;
		if(DIRECTION_RTN == router_header_temp->D)
		{
		    router_table_id = decide_router_table(pdu_mbuf);
		    router_header[router_table_id].S = router_header_temp->S;
		    router_header[router_table_id].T = router_header_temp->T;
		    rte_pktmbuf_adj(pdu_mbuf, router_header_size);
		    msg_len = data_len_tmp - router_header_size;
		    char *send_buffer = (char *)rte_pktmbuf_append(decap_mbuf, g_st_app_conf.router_header_size[router_table_id] + msg_len);   
		    rte_memcpy(send_buffer, &router_header[router_table_id], g_st_app_conf.router_header_size[router_table_id]);
		    rte_memcpy(send_buffer + g_st_app_conf.router_header_size[router_table_id], rte_pktmbuf_mtod(pdu_mbuf, uint8_t *), msg_len);
			ut_statis_debug.rtn_pdu_num++;
		}
		else
		{
            char *send_buffer = (char *)rte_pktmbuf_append(decap_mbuf, data_len_tmp);
			rte_memcpy(send_buffer, rte_pktmbuf_mtod(pdu_mbuf, uint8_t *), data_len_tmp);
			rte_pktmbuf_adj(pdu_mbuf, router_header_size);
			msg_len = data_len_tmp - router_header_size;
			ut_statis_debug.fwd_pdu_num++;
		}
		mph = (Mbuf_Private_Header*)rte_mbuf_to_priv(decap_mbuf);
		mph->protocol_type = protocol_type;
	    rte_pktmbuf_adj(pdu_mbuf, msg_len);
		router_func(decap_mbuf);
        
	    //for test
	
        #if 0
	    if(m_cType == 0x9103)
	    {
	        send_buffer = rte_pktmbuf_mtod(decap_mbuf, char *);    
	        send_buffer = send_buffer + 10 + 44;
	        /*memcpy(&seq_num, send_buffer, sizeof(uint32_t));
            seq_num = ntohl(seq_num);
            debug_log(1, test_file_record_2, "%d\n", seq_num);
	    	send_buffer = send_buffer + 12;*/
	        memcpy(&unique_num, send_buffer, sizeof(uint16_t));
            unique_num = ntohs(unique_num);
	    	if(unique_num == 12851&&(decap_mbuf->data_len>76))
	    	{
		        ut_statis_debug.rtn_pdu_num++;
			
		    }
	    }
	    #endif
		//for test
		ut_statis_debug.decap_pdu_num[queue_id]++;

    }


	if(pdu_total_len != frame_para->frame_size)
	{
        printf("decap pdu len error\n");
	}
	rte_pktmbuf_free(pdu_mbuf);
}

void decode_delay_pdu(struct rte_mbuf* pdu_mbuf)
{
    static uint8_t i = 0;
    if(NULL == pdu_mbuf)
    {
        printf("pdu_mbuf is NULL \n");
    }
	
	rte_pktmbuf_adj(pdu_mbuf, sizeof(L2L1_MSG_HEADER));


	if(false == enqueue_encap_ring(pdu_mbuf, i))
	{
		rte_pktmbuf_free(pdu_mbuf);
		printf("enqueue_encap_ring failed\n");
	}


	i++;
	if(i == g_st_app_conf.encap_ring_queue_num)
	{
        i = 0;
	}
}


static int prepare_l2l1_pdu(struct rte_mbuf* mbuf)
{

	char *send_buffer;
    L2L1_MSG_HEADER inner_msg_header;
	int router_table_id;
	ROUTERHEADER *router_header_tmp;
	uint16_t msg_len;
	uint8_t mapping_id;
    uint16_t pdu_len;
	uint8_t  pdu_buffer_tmp[4096];    
    if(NULL == mbuf)
	{
		LOG(LOG_ERR, LOG_MODULE_ENCAP, LOG_ERR,
                LOG_CONTENT("mbuf is null"));
		return -1;
	}
	else
	{
        pdu_len = mbuf->data_len - sizeof(struct rte_ether_hdr);
		memcpy(pdu_buffer_tmp, rte_pktmbuf_mtod_offset(mbuf, uint8_t *, sizeof(struct rte_ether_hdr)), pdu_len);
	}


//	if(NULL != l2l1_pdu_mbuf)
	{ 
		
	    router_table_id = decide_router_table_by_dst_id();
	        	if(-1 == router_table_id)
						{
							            return -1;
								    		}
		uint16_t next_hop_id = 0;
		if(router_header[router_table_id].CUR_ENTRY == router_header[router_table_id].LAST_ENTRY)
		{
            next_hop_id = ntohs(router_header[router_table_id].segment[router_header[router_table_id].CUR_ENTRY]);
		}
		else
		{
            next_hop_id = ntohs(router_header[router_table_id].segment[router_header[router_table_id].CUR_ENTRY + 1]);
		}

		mapping_id = get_mapping_index(g_st_app_conf.local_id, router_header[router_table_id].LAST_ENTRY, next_hop_id);
        uint16_t msg_len = g_st_app_conf.router_header_size[router_table_id] + pdu_len;
		encode_inner_header(&inner_msg_header, msg_len, mapping_id, g_st_app_conf.port_id[mapping_id]);
		inner_msg_header.msg_type = FWD_TUNNEL_PDU;
		
		router_header[router_table_id].D = g_st_app_conf.port_id[mapping_id];
		send_buffer = (char *)rte_pktmbuf_append(mbuf, sizeof(L2L1_MSG_HEADER)+g_st_app_conf.router_header_size[router_table_id]-sizeof(struct rte_ether_hdr));
		send_buffer =  rte_pktmbuf_mtod(mbuf, char *); 
	    rte_memcpy(send_buffer + sizeof(L2L1_MSG_HEADER) + g_st_app_conf.router_header_size[router_table_id], pdu_buffer_tmp, pdu_len);
		rte_memcpy(send_buffer, &inner_msg_header, sizeof(L2L1_MSG_HEADER));
	    rte_memcpy(send_buffer + sizeof(L2L1_MSG_HEADER), &router_header[router_table_id], g_st_app_conf.router_header_size[router_table_id]);

	
	}

	return 0;
	
}

static uint16_t fc_check_proc_tx_indication_hdr(struct rte_mbuf * mbuf)
{

	uint16_t mbuf_valid = 0;

	uint16_t mbuf_idx;
	FWD_Tx_Indication* tx_indication_hdr;
	if(mbuf == NULL)
	{
       printf("mbuf is null \n");
	}
	
	tx_indication_hdr = rte_pktmbuf_mtod(mbuf, FWD_Tx_Indication*);
	if((rte_be_to_cpu_32(tx_indication_hdr->Indication_Mask) & INDC_AVAIL_BUF_SENDING_RATE_MASK) == 0) {   // Just care Available_Buffer

			return 0;
		}

	    ut_statis_debug.total_indication_num++;

		rte_atomic32_set(&available_fpga_buff, (int32_t)rte_be_to_cpu_32(tx_indication_hdr->Available_Buffer));
	 	if(rte_be_to_cpu_32(tx_indication_hdr->Available_Buffer) == 0) {
			ut_statis_debug.indication_zero_num++;
		}
		
		/*if(rte_be_to_cpu_32(tx_indication_hdr->Available_Buffer) <81910)
		{
		    printf("available_fpga_buff:%d\n",rte_be_to_cpu_32(tx_indication_hdr->Available_Buffer) );
		}*/
		mbuf_valid++;
	return mbuf_valid;
}

#ifdef TAG_ROUTE
//L2L1_MSG_HEADER->L2L1_HEADER->BBFRAME_HEADER
int process_frame_header(struct rte_mbuf *frame_mbuf, FRAME_PARAM *frame_para, uint16_t queue_id)
{
	uint16_t dst_id = 0;
	uint16_t src_id = 0;
	uint8_t* pCodeBlock = NULL; 
	uint8_t* pFrame = NULL;
	uint16_t  Frame_size;		
	uint16_t gse_offset;
	uint32_t padding_bytes = 0;
	BBHEADER*			pBBHeader = 0;
	ISLHEADER*			pISLHeader = 0;
	L2L1_HEADER* pL2L1Header = 0;
	TX_FWD_DATA* pTL2L1Header = 0;
	struct rte_ether_hdr *eth_hdr;
	uint16_t ether_type;

	L2L1_MSG_HEADER *l2l1_msg_header;
	struct rte_mbuf* l2l1_pdu_mbuf;
	uint16_t DFL;

	eth_hdr = rte_pktmbuf_mtod(frame_mbuf, struct rte_ether_hdr *);
	ether_type = ntohs(eth_hdr->ether_type);
	int ret = 0;
	if(ether_type == 0x0800 || ether_type == 0x0806)
	{
		 ret = prepare_l2l1_pdu(frame_mbuf);
		 	 if(ret == -1)
				 		 {
							          	rte_pktmbuf_free(frame_mbuf);
											    ut_statis_debug.device_id_filter_num ++;
											    		    return -1;
													    		 }
	}


	l2l1_msg_header = rte_pktmbuf_mtod(frame_mbuf, L2L1_MSG_HEADER *);
 
#if PCIE_SWITCH
	dst_id = l2l1_msg_header->m_cDstMacAddress[3];
	if(dst_id != g_st_app_conf.router_id)
	{
		rte_pktmbuf_free(frame_mbuf);
		ut_statis_debug.device_id_filter_num ++;
		return -1;
	}
#endif

	pCodeBlock = rte_pktmbuf_mtod(frame_mbuf, uint8_t*) + sizeof(L2L1_MSG_HEADER);
	if(ether_type == 0x9101)
	{
		switch (l2l1_msg_header->msg_type)
		{
			case ENUM_FWD_Tx_FWD_Data:
			case ENUM_FWD_Rx_FWD_Data:
				 {
					pL2L1Header = (L2L1_HEADER*)pCodeBlock;
					pTL2L1Header = (TX_FWD_DATA*)pCodeBlock;
					uint16_t type = pTL2L1Header->priority;
					if(1 == type)
					{
						ut_statis_debug.bb_ctl_num[queue_id]++;
					}
					else if(2 == type)
					{
						ut_statis_debug.bb_data_num[queue_id]++;
					}
					pFrame = pCodeBlock + sizeof(L2L1_HEADER);	
					Frame_size = ntohs(pL2L1Header->frame_size);// sizeof(BBHEADER) + frame_max_payload;
					//Frame_size = pL2L1Header->frame_size;
					pBBHeader = (BBHEADER*)pFrame;
				        DFL = ntohs(pBBHeader->DFL);
					padding_bytes = Frame_size - sizeof(BBHEADER) - (DFL >> 3);
					gse_offset = sizeof(L2L1_MSG_HEADER) + sizeof(L2L1_HEADER) + sizeof(BBHEADER);
					frame_para->header_size = sizeof(BBHEADER);
					if(identify_gs(pBBHeader) < 0)	
					{
						free_mbuf(frame_mbuf);
						ut_statis_debug.bb_crc_filter_num[queue_id] ++;
						return -1;
					}
				}
				break;
			case ENUM_FWD_Tx_Indication:
				{
					if(rte_be_to_cpu_16(l2l1_msg_header->msg_len) != (uint16_t)(sizeof(L2L1_MSG_HEADER) + sizeof(FWD_Tx_Indication) - sizeof(struct rte_ether_hdr))) 
					{
						printf("Packet length check failed, ENUM_FWD_Rx_FWD_Data but l2l1_hdr->Length <= sizeof(L2L1_Header) + sizeof(FWD_Rx_FWD_Data)\n");
						rte_pktmbuf_free(frame_mbuf);
						frame_mbuf = NULL;
						return -1;
				    }

                    uint16_t len = sizeof(L2L1_MSG_HEADER);
			        rte_pktmbuf_adj(frame_mbuf, len);
			        fc_check_proc_tx_indication_hdr(frame_mbuf);
					free_mbuf(frame_mbuf);
				    return 2;
				}

			default:
				free_mbuf(frame_mbuf);
				return -1;
				break;
		}

	}
	else
	{
		switch (l2l1_msg_header->msg_type)
		{
			case ISL_PACK:
				{
					Frame_size = l2l1_msg_header->msg_len + sizeof(MAC_FRAME_HEADER) - sizeof(L2L1_MSG_HEADER);
					pISLHeader = (ISLHEADER*)pCodeBlock;
					pISLHeader->DFL = ntohs(pISLHeader->DFL);
					padding_bytes = Frame_size - sizeof(ISLHEADER) - (pISLHeader->DFL >> 3);
					gse_offset = sizeof(L2L1_MSG_HEADER) + sizeof(ISLHEADER);
					frame_para->header_size = sizeof(ISLHEADER);
					if(identify_isl(pISLHeader) < 0)		
					{
						free_mbuf(frame_mbuf);
						ut_statis_debug.isl_crc_filter_num ++;
						return -1;
					}
				}
				break;
			case MULTI_FWD_PDU_PACK:
			case MULTI_RTN_PDU_PACK:
			case FWD_TUNNEL_PDU:
			case RTN_TUNNEL_PDU:
			//case DELAY_METRIC_FRAME:
				l2l1_msg_header->msg_len = ntohs(l2l1_msg_header->msg_len);
				if(l2l1_msg_header->msg_len <= 0)
				{
					printf("l2l1_msg_header->msg_len error\n");
					free_mbuf(frame_mbuf);
					return -1;
				}
				Frame_size = l2l1_msg_header->msg_len + sizeof(MAC_FRAME_HEADER) - sizeof(L2L1_MSG_HEADER);
				break;

			default:
				free_mbuf(frame_mbuf);
				return -1;
				break;
		}
	}

	frame_para->frame_size = Frame_size;
	frame_para->padding_bytes = padding_bytes;
	frame_para->pack_type = l2l1_msg_header->msg_type;
	frame_para->gse_offset = gse_offset;

	return 0;

	
}
#else
int process_frame_header(struct rte_mbuf *frame_mbuf, FRAME_PARAM *frame_para, uint16_t queue_id)
{
	uint16_t dst_id = 0;
	uint16_t src_id = 0;
	uint8_t* pCodeBlock = NULL; 
	uint8_t* pFrame = NULL;
	uint16_t  Frame_size;		
	uint16_t gse_offset;
	uint32_t padding_bytes = 0;
	BBHEADER*			pBBHeader = 0;
	ISLHEADER*			pISLHeader = 0;
	L2L1_HEADER* pL2L1Header = 0;
	TX_FWD_DATA* pTL2L1Header = 0;
	struct rte_ether_hdr *eth_hdr;
	uint16_t ether_type;

	L2L1_MSG_HEADER *l2l1_msg_header;
	struct rte_mbuf* l2l1_pdu_mbuf;
	uint16_t DFL;

	eth_hdr = rte_pktmbuf_mtod(frame_mbuf, struct rte_ether_hdr *);
	ether_type = ntohs(eth_hdr->ether_type);
	int ret = 0;


	l2l1_msg_header = rte_pktmbuf_mtod(frame_mbuf, L2L1_MSG_HEADER *);
 


	pCodeBlock = rte_pktmbuf_mtod(frame_mbuf, uint8_t*) + sizeof(L2L1_MSG_HEADER);
	if(0x9101 == ether_type)
	{
		switch (l2l1_msg_header->msg_type)
		{
			case ENUM_FWD_Tx_FWD_Data:
			case ENUM_FWD_Rx_FWD_Data:
				 {
					pL2L1Header = (L2L1_HEADER*)pCodeBlock;
					pTL2L1Header = (TX_FWD_DATA*)pCodeBlock;
					uint16_t type = pTL2L1Header->priority;
					if(1 == type)
					{
						ut_statis_debug.bb_ctl_num[queue_id]++;
					}
					else if(2 == type)
					{
						ut_statis_debug.bb_data_num[queue_id]++;
					}
					pFrame = pCodeBlock + sizeof(L2L1_HEADER);	
					Frame_size = ntohs(pL2L1Header->frame_size);// sizeof(BBHEADER) + frame_max_payload;
					//Frame_size = pL2L1Header->frame_size;
					pBBHeader = (BBHEADER*)pFrame;
				        DFL = ntohs(pBBHeader->DFL);
					padding_bytes = Frame_size - sizeof(BBHEADER) - (DFL >> 3);
					gse_offset = sizeof(L2L1_MSG_HEADER) + sizeof(L2L1_HEADER) + sizeof(BBHEADER);
					frame_para->header_size = sizeof(BBHEADER);
					if(identify_gs(pBBHeader) < 0)	
					{
						free_mbuf(frame_mbuf);
						ut_statis_debug.bb_crc_filter_num[queue_id] ++;
						return -1;
					}
					frame_para->frame_size = Frame_size;
					frame_para->padding_bytes = padding_bytes;
					frame_para->pack_type = l2l1_msg_header->msg_type;
					frame_para->gse_offset = gse_offset;
				}
				break;
			case ENUM_FWD_Tx_Indication:
				{
					if(rte_be_to_cpu_16(l2l1_msg_header->msg_len) != (uint16_t)(sizeof(L2L1_MSG_HEADER) + sizeof(FWD_Tx_Indication) - sizeof(struct rte_ether_hdr))) 
					{
						printf("Packet length check failed, ENUM_FWD_Rx_FWD_Data but l2l1_hdr->Length <= sizeof(L2L1_Header) + sizeof(FWD_Rx_FWD_Data)\n");
						rte_pktmbuf_free(frame_mbuf);
						frame_mbuf = NULL;
						return -1;
				    }

                    uint16_t len = sizeof(L2L1_MSG_HEADER);
			        rte_pktmbuf_adj(frame_mbuf, len);
			        fc_check_proc_tx_indication_hdr(frame_mbuf);
					free_mbuf(frame_mbuf);
				    return 2;
				}

			default:
				free_mbuf(frame_mbuf);
				return -1;
				break;
		}

	}
	else if(PROTOCOL_ARP == ether_type || PROTOCOL_IPV4 == ether_type)
	{
	    frame_para->pack_type = RTN_TUNNEL_PDU;
		Mbuf_Private_Header * mph = (Mbuf_Private_Header*)rte_mbuf_to_priv(frame_mbuf);
		rte_pktmbuf_adj(frame_mbuf, sizeof(struct rte_ether_hdr));
		mph->protocol_type = ether_type;
	}
	else
	{
		free_mbuf(frame_mbuf);
		return -1;

	}

	return 0;

	
}


#endif





void decapsulate_gse(struct rte_mbuf* frame_mbuf, 	FRAME_PARAM *frame_para, uint8_t queue_id)
{

	// 要读取的各种数据包头
	BBHEADER* 			pBBHeader = 0;
	GSE_FIXED_HEADER 	fixed_header;
	GSE_VARIABLE_HEADER 		var_header;
	OPT_EXT_HEADER_INFO opt_extHeader;
	MAN_EXT_HEADER_INFO 		man_extHeader;

	uint16_t timestamp_seq = 0;
	uint16_t offset = 0;		// 解封装时，当前数据相对于起始地址的偏移量

	uint16_t protocol_type = 0; // 保存正在处理的当前扩展头的类型
	uint16_t next_type;			// 当前扩展头链接的下一扩展头类型

	uint16_t ext_header_size = 0;		// 扩展头长度：包括 data字段和next_type字段
	uint16_t var_header_size = 0;	// 可变头+扩展头长度(包括 data字段和next_type字段)
	uint8_t  temp_header_type;			// 临时变量：保存扩展头类型
	uint8_t  temp_header_size;			// 临时变量：保存扩展头长度

	// FRAME_NODE* pFrameNode = NULL;	// 采用链表方式，存储 mbuf的指针 和 frame存储的指针

	// struct rte_mbuf* ptr_IPPacket_mbuf = NULL;

	// BBFrame在内存中的位置。1个CodeBlock包含1个BBFrame（含N个GSE包）：1个L2L1Header + 1个BBHeader + N个（GSE Header + Ext Header + data）

	uint8_t* pCodeBlock = NULL;			// 1个CodeBlock的首地址	
	uint8_t* pBBFrame = NULL;			// 1个BBFrame的首地址，1个BBFrame就是（1个CodeBlock - 1个L2L1_HEADER）
	uint8_t* pGSE = NULL;				// 1个BBFrame中正在操作的GSE包的首地址
	uint8_t* pNextGSE = NULL;			// 1个BBFrame中正在操作的GSE包的下一个GSE包的首地址
	uint8_t* pExtHeader = NULL;			// 1个GSE包里的正在操作的扩展头的地址
	uint8_t* ptr_datafield = NULL;		// 1个GSE包里的数据字段的首地址

	int16_t  BBFrame_size;				// BBFrame长度
	uint32_t handle_total_size = 0;		// 已经处理的GSE包的总长度
	uint32_t padding_bytes = 0;			// 1个frame中padding的字节数
	uint8_t  pdu_frag_type;				// PDU分片包的类型：start、middle、end

	int i;
	int8_t  mac_match;			// mac地址匹配标志
	uint8_t bfirst_gse = 1;		// 标识找到了BBFrame中的第一个GSE包
	int pdu_type;				// pdu的类型：0～IP包、1～前向控制消息、2～反向控制消息
	int8_t	func_ret;
	int8_t  buffer_index;

	MAC_FRAME_HEADER mac_frame_header;
	uint16_t header_size;


    uint64_t inrouter_time = 0;
	Mbuf_Private_Header * mph = (Mbuf_Private_Header*)rte_mbuf_to_priv(frame_mbuf);
	if(1 == mph->delay_time_mark_flag)
	{
		inrouter_time = mph->inrouter_time;
		mph->delay_time_mark_flag = 0;
	}



	switch (frame_para->pack_type)
	{
		case ISL_PACK:
			header_size = sizeof(ISLHEADER);
			padding_bytes = frame_para->padding_bytes;
		    BBFrame_size  = frame_para->frame_size;
			
			break;

	    case ENUM_FWD_Tx_FWD_Data:
		case ENUM_FWD_Rx_FWD_Data:
			header_size = sizeof(BBHEADER);
			padding_bytes = frame_para->padding_bytes;
		    BBFrame_size  = frame_para->frame_size;
			break;
		default:
			break;
	}

	pGSE = rte_pktmbuf_mtod(frame_mbuf, uint8_t*) + frame_para->gse_offset;


	bfirst_gse = 1;

	do
	{
		var_header_size = 0;
		offset = decapsulate_gse_header(pGSE, &next_type, &fixed_header, &var_header);	// 解析得到GSE头的具体值
		pNextGSE = pGSE + sizeof(GSE_FIXED_HEADER) + fixed_header.GSE_Length;			// 得到下一个GSE包的首地址（如果在该frame里还有GSE包）



		// MAC地址过滤
		mac_match = filter_address(fixed_header, var_header, g_st_app_conf.local_mac[0], bfirst_gse, &previous_mac_info);
		// mac_match = 1;	// 空口MAC的匹配结果固定为（匹配）
		if(1==mac_match)
		{
			bfirst_gse = 0;
		}
		else
		{
	        
	        //write_pcap_file(10,rte_pktmbuf_mtod(frame_mbuf, uint8_t *), frame_mbuf->data_len);
		    ut_statis_debug.gse_mac_filter_num ++;
	
		}


		if(1==mac_match)
		{
			// 读取当前GSE包里的全部扩展头
			i = 0;
			opt_extHeader.header_num = 0;
			man_extHeader.header_num = 0;
			protocol_type = 0;

			while(regular_ext_header_type(next_type))
			{
				protocol_type = next_type;		// protocol_type表示当前正在处理的协议头类型值
				pExtHeader = pGSE + offset; 	// 当前正在处理的扩展头首地址

// printf("offset:%d ... ", offset);
// for(int j=0; j<8; j++)
// 	printf("%02X ", *(pExtHeader+j));
// printf("\n");

				// 分析扩展头，会修改next_type，得到紧接着的下一段数据的协议类型
				ext_header_size = get_extension_header_info(pExtHeader, protocol_type, &next_type);	
				offset += ext_header_size;

				temp_header_type = get_extension_header_type(protocol_type);
				if(OPTIONAL_EXT_HEADER==temp_header_type)
				{

					opt_extHeader.header_num++;
					opt_extHeader.header_size[i] = ext_header_size;
					opt_extHeader.header_type[i] = protocol_type;
					memcpy(opt_extHeader.header_value[i], pExtHeader, ext_header_size-2);
					opt_extHeader.next_type[i] = next_type;
					var_header_size = opt_extHeader.header_size[i];

					if(FRAG_SEQUENCE==opt_extHeader.header_type[i])
					{
						memcpy(&timestamp_seq, &(opt_extHeader.header_value[i][0]), sizeof(uint16_t));
						if(little_endian)
						{
							timestamp_seq = rte_be_to_cpu_16(timestamp_seq);
						}
					}

				}
				else
				{
					man_extHeader.header_num++;
					man_extHeader.header_size[i] = ext_header_size;
					man_extHeader.header_type[i] = protocol_type;
					memcpy(man_extHeader.header_value[i], pExtHeader, ext_header_size-2);
					man_extHeader.next_type[i] = next_type;
					var_header_size = man_extHeader.header_size[i];


				}
			}


			var_header_size += var_header.variable_header_size.fragment_ID_size 	 + var_header.variable_header_size.total_length_size +
								   var_header.variable_header_size.protocol_type_size + var_header.variable_header_size.label_size;
			ptr_datafield = pGSE + offset;	// 准备读取当前GSE包里的PDU

			pdu_frag_type = start_end_indicator(fixed_header);	// 鉴别PDU的类型


			switch(pdu_frag_type)
			{
				case WHOLE_PDU:
				{
						if(PDU_CONCAT==protocol_type)	// 最后一个扩展头类型是PDU_CONCAT，则GSE数据字段里的每个PDU数据之前都增加了2字节的长度字段
						{
							get_concat_pdu(ptr_datafield, fixed_header.GSE_Length - var_header_size);
						}
						else
						{

							pdu_type = get_single_pdu(ptr_datafield, fixed_header.GSE_Length - var_header_size, next_type, queue_id, inrouter_time);
							inrouter_time = 0;

// read_ring_mbuf(ptr_ring_data[pdu_type], (void*)&ptr_IPPacket_mbuf[0], pdu_type);
						}
						break;
					}

					case FRAG_START:
					{								
						temp_header_size = var_header_size - (var_header.variable_header_size.fragment_ID_size + var_header.variable_header_size.total_length_size);
// for(int j=0; j<256; j++)
// {
// 	if(frag_buffer_index_list[j]->size>0 || 0!=frag_buffer_index_list[j]->head->next)
// 		printf("frag_id:%d list szie:%d next:%p\n", j, frag_buffer_index_list[j]->size, frag_buffer_index_list[j]->head->next);
// }
						func_ret = start_reassemble_pdu(ptr_datafield, 
														var_header, 
														timestamp_seq, 
														temp_header_size,
														fixed_header.GSE_Length - var_header_size, 
														next_type, 
														g_frag_info,
														&buffer_index,
														queue_id);

			
						if(1==func_ret)	// 将PDU分片包组装成完整的PDU
						{

							pdu_type = finish_reassemble_pdu(&(g_frag_info[queue_id][var_header.variable_header_value.fragment_ID][buffer_index]), queue_id);

						}
						else
						{

						}
						break;
					}

					case FRAG_MIDDLE:
					case FRAG_END:
					{
						func_ret = keep_reassemble_pdu(pdu_frag_type, 
													   ptr_datafield, 
													   var_header.variable_header_value.fragment_ID, 
													   timestamp_seq,
													   fixed_header.GSE_Length - var_header_size, 
													   g_frag_info,
													   &buffer_index,
													   queue_id);

						if(1==func_ret)	// 将PDU分片包组装成完整的PDU
						{
							pdu_type = finish_reassemble_pdu(&(g_frag_info[queue_id][var_header.variable_header_value.fragment_ID][buffer_index]), queue_id);
						}

						break;
					}

					default:
                        pdu_type = -1;
						break;
				}

			    if(pdu_type < 0)
			    {
                    ut_statis_debug.reasemble_pdu_failed++;
				}

			    
			}

			pGSE = pNextGSE;
			handle_total_size += sizeof(GSE_FIXED_HEADER) + fixed_header.GSE_Length;
			ut_statis_debug.decap_gse_num[queue_id]++;
	

		} while((header_size + handle_total_size + padding_bytes) < (uint16_t)BBFrame_size);

       
        ut_statis_debug.decap_frame_num[queue_id]++;
		free_mbuf(frame_mbuf);
		handle_total_size = 0;

}





/********************************************************************************
* 功能：解析GSE包头
* 参数：
		[in]	pGSE：GSE包的首地址
		[out]	pProtocol_type：GSE包头里的Protocol_type字段
		[out]	pfixed_header：GSE包头的固定部分
		[out]	pvariable_header：GSE包头的可变部分
* 返回值：
		GSE包头长度，即扩展头或PDU相对于GSE包头的偏移量
********************************************************************************/
int16_t decapsulate_gse_header(uint8_t* pGSE, uint16_t* pProtocol_type, GSE_FIXED_HEADER* pfixed_header, GSE_VARIABLE_HEADER* pvariable_header)
{
	GSE_VARIABLE_HEADER_SIZE  var_header_size;
	GSE_VARIABLE_HEADER_VALUE var_header_value;
	int offset = 0;

	if(NULL==pGSE || NULL==pProtocol_type || NULL==pvariable_header || NULL==pfixed_header)
		return POINT_ERROR;

	memcpy(pfixed_header, pGSE, sizeof(GSE_FIXED_HEADER));
	offset += sizeof(GSE_FIXED_HEADER);

	if(little_endian)
	{
		*((uint16_t*)pfixed_header) = rte_be_to_cpu_16(*((uint16_t*)pfixed_header));
	}

	if(pfixed_header->Start_Indicator==1)
	{
		if(pfixed_header->End_Indicator==1)
		{
			var_header_size.fragment_ID_size = 0;
			var_header_size.total_length_size = 0;
		}
		else
		{
			var_header_size.fragment_ID_size = 1;
			var_header_size.total_length_size = 2;
		}
	}
	else
	{
		var_header_size.fragment_ID_size = 1;
		if(pfixed_header->End_Indicator==1)
		{
			var_header_size.total_length_size = 0;
		}
		else
		{
			var_header_size.total_length_size = 0;
		}
	}

	var_header_size.protocol_type_size = 2;

	switch(pfixed_header->Label_Type)
	{
		case LABEL_TYPE_6B:
			var_header_size.label_size = 6;
			break;
		case LABEL_TYPE_REUSE:
		case LABEL_TYPE_BROADCAST:
			var_header_size.label_size = 0;
			break;
		case LABEL_TYPE_3B:
			var_header_size.label_size = 3;
			break;
		default:
			var_header_size.label_size = 0;
			break;	
	}

	if(1==var_header_size.fragment_ID_size)
	{
		memcpy(&(var_header_value.fragment_ID), pGSE+offset, 1);
		offset += 1;
	}

	if(2==var_header_size.total_length_size)
	{	
		memcpy(&(var_header_value.total_length), pGSE+offset, 2);
		offset += 2;
	}

	if(2==var_header_size.protocol_type_size)
	{	
		memcpy(&(var_header_value.protocol_type), pGSE+offset, 2);
		offset += 2;
	}

	memcpy(var_header_value.label, pGSE+offset, var_header_size.label_size);
	offset += var_header_size.label_size;

	if(little_endian)
	{
		var_header_value.total_length  = rte_be_to_cpu_16(var_header_value.total_length);
		var_header_value.protocol_type = rte_be_to_cpu_16(var_header_value.protocol_type);
	}

	*pProtocol_type = var_header_value.protocol_type;
	memcpy(&(pvariable_header->variable_header_size), &var_header_size, sizeof(GSE_VARIABLE_HEADER_SIZE));
	memcpy(&(pvariable_header->variable_header_value), &var_header_value, sizeof(GSE_VARIABLE_HEADER_VALUE));

	return offset;
}

/********************************************************************************
* 功能：得到扩展头的信息
* 参数：
		[in]	pExt_header		：当前扩展头的首地址
		[in]	cur_header_type	：当前扩展头的类型值
		[in]	next_type		：下一个扩展头或PDU的类型值
		[out]	pbInstant		：
* 返回值：
* 		扩展头长度（包括扩展头数据字段和next_type字段）
********************************************************************************/
int16_t get_extension_header_info(uint8_t* pExt_header, uint16_t cur_header_type, uint16_t* next_type)
{
	uint16_t header_data_size = 0;
	
	uint16_t H_LEN = (cur_header_type & 0xFF00)>>8;
	uint16_t H_TYPE = cur_header_type & 0x00FF;

	if(NULL==pExt_header)
		return -1;

	if(H_LEN==0)// mandatory扩展头
	{
		switch(H_TYPE)
		{
			case PDU_CONCAT:
				header_data_size = get_pdu_concat_info(next_type, pExt_header);
				break;
			default:
				break;
		}
	}
	else		// optional扩展头
	{
		header_data_size = 2*(H_LEN-1);
		*next_type = *(uint16_t*)(pExt_header + header_data_size);	// 得到下一个扩展头或PDU的类型值
		header_data_size += 2;

		if(little_endian)
		{
			*next_type = rte_be_to_cpu_16(*next_type);
		}

		switch(H_TYPE)
		{
			case FRAG_SEQUENCE:
				break;
			default:
				break;
		}
	}

	return (header_data_size);	// ָ指向下一个扩展头数据字段或PDU的起始
}

/********************************************************************************
* 功能：判断扩展头类型是mandatory还是optional
* 参数：
		[in]	header_type		：当前扩展头的类型值
* 返回值：
* 		0～mandatory		1～optional
********************************************************************************/
uint8_t get_extension_header_type(uint16_t header_type)
{
	uint16_t H_LEN = (header_type & 0xFF00)>>8;

	if(H_LEN==0)	// mandatory
	{
		return MANDATORY_EXT_HEADER;
	}
	else			// optional
	{
		return OPTIONAL_EXT_HEADER;
	}
}

/********************************************************************************
* 功能：得到pdu_concat扩展头的信息
* 参数：
		[in]	next_type	：下一个扩展头或PDU的类型值
		[in]	pheader		：当前扩展头的数据字段的首地址
* 返回值：
* 		pdu_concat扩展头的长度
********************************************************************************/
int16_t get_pdu_concat_info(uint16_t* next_type, uint8_t* pheader)
{
	if(NULL==next_type || NULL==pheader)
		return POINT_ERROR;

	*next_type = *(uint16_t*)(pheader);
	return 2;
}



int16_t get_L2L1_Header(uint8_t* pCodeBlock, L2L1_HEADER* pL2L1_header)
{
	if(NULL==pCodeBlock || NULL==pL2L1_header)
		return POINT_ERROR;
	
	memcpy(pL2L1_header, pCodeBlock, sizeof(L2L1_HEADER));
	return pL2L1_header->frame_size;
}


void get_BBHeader(uint8_t* pBBFrame, BBHEADER* pBBHeader)
{
	if(NULL!=pBBFrame && NULL!=pBBHeader)
		memcpy(pBBHeader, pBBFrame, sizeof(BBHEADER));
}


void process_pdu_concat(void)
{
	
}

void process_fragment_sequence(void)
{
	
}

/********************************************************************************
* 功能：判断扩展头类型是否合法
* 参数：
		[in]	header_type：扩展头类型值
* 返回值：
		1～合法  0～非法
********************************************************************************/
int8_t regular_ext_header_type(uint16_t header_type)
{
	if( header_type == PDU_CONCAT		||
		header_type == TIME_STEMP		||
		header_type == FRAG_SEQUENCE	)
	{
		return 1;
	}
	else
	{
		return 0;
	}
}


/********************************************************************************
* 功能：读取GSE包里的串接的所有PDU
* 参数：
		[in]	ptr_datafield	：GSE包里数据字段的起始地址
		[in]	datafield_length：GSE包里的数据PDU长度 + 2字节的PDU长度字段
* 返回值：
		无
********************************************************************************/
void get_concat_pdu(uint8_t* ptr_datafield, uint16_t datafield_length)
{
	uint8_t* ptr_position = ptr_datafield;
	uint16_t total_read_length = 0;		// 已读取的datafield的总长度
	uint16_t read_length = 0;			// 一次读取datafield的长度
	uint16_t offset = 0;
	DE_PDU_NODE *ptr_pdu_node = 0;

	if(!ptr_datafield)
		return;

	while(total_read_length < datafield_length)
	{
		ptr_pdu_node = (DE_PDU_NODE*)malloc(sizeof(DE_PDU_NODE));	// 产生一个PDU缓存节点
		if(!ptr_pdu_node)
			continue;

		read_length = *((uint16_t*)ptr_position);				// 一次读取的字节数
		total_read_length += read_length + sizeof(read_length);	// 累计读取的字节数:PDU+2字节PDU长度字段
		ptr_position += sizeof(read_length);					// 指针移到实际PDU的起始

		ptr_pdu_node->ptr_pdu = (uint8_t*)malloc(read_length);		// 生成完整PDU的缓存区，缓存区大小由gse包头决定
		if(!ptr_pdu_node->ptr_pdu)
		{
			free(ptr_pdu_node);
			ptr_pdu_node = NULL;
			continue;
		}
		ptr_pdu_node->pdu_length = read_length;
		memcpy(ptr_pdu_node->ptr_pdu, ptr_position, read_length);	// 将PDU拷贝到PDU buffer

		ptr_position += read_length;						// 指针移到下一个pdu_concat字段
		offset += read_length;

		// 将该节点加到消息队列，通知上一层处理
		// fwrite(ptr_pdu_node->ptr_pdu, read_length, 1, file_data);
		// printf("get_concat_pdu write:%d\n", read_length);
		free(ptr_pdu_node->ptr_pdu);
		free(ptr_pdu_node);
	}
}


/********************************************************************************
* 功能：读取GSE包里的单个PDU
* 参数：
		[in]	ptr_datafield	：GSE包里数据字段的起始地址
		[in]	datafield_length：GSE包里的数据PDU长度
		[in]	protocol_type	: PDU的协议类型（IP包/控制消息）
* 返回值：
		pdu的类型：0～IP包、1～前向控制消息、2～反向控制消息
********************************************************************************/
int get_single_pdu(uint8_t* ptr_datafield, uint16_t datafield_length, uint16_t protocol_type, uint16_t port_id, uint64_t inrouter_time)
{

	uint8_t *data_start = NULL;	// mbuf里dataroom的起始地址
	int pdu_type;				// pdu类型：IP包、前向控制消息、反向控制消息
	struct rte_ether_hdr eth_hdr = { 0 };

static u_int32_t call_count = 0;
	Mbuf_Private_Header * mph = NULL;


	if(!ptr_datafield)
		return -1;

	// eth_hdr.ether_type = protocol_type;
	eth_hdr.ether_type = ntohs(protocol_type);

	switch(protocol_type)
	{
		// 数据是IP包，存入IP包缓冲区
		case PROTOCOL_IPV4:
		case PROTOCOL_IPV6:
		{
			pdu_type = IP_PACKET;

			break;
		}

		// 数据是控制信息，存入控制信息缓存区
		case PROTOCOL_CTRL:
		{
			pdu_type = (0 == ((*(ptr_datafield+1)) & 0x40)) ? CTRLMSG_FWD : CTRLMSG_RTN;
			return -1;

		//	break;
		}

		default:
			//return -1;
			break;
	}

	struct rte_mbuf* data_mbuf = rte_pktmbuf_alloc(decap_pktmbuf_pool);	// 从内存池申请一个mbuf
	if(!data_mbuf)
	{
	    ut_statis_debug.alloc_decap_pktmbuf_pool_failed++;
		return -1;
	}


	if(0 != inrouter_time)
	{
		mph = (Mbuf_Private_Header*)rte_mbuf_to_priv(data_mbuf);
		mph->delay_time_mark_flag = 1; 
		mph->inrouter_time = inrouter_time;
	}
	
	

	call_count++;
	// printf("get_single_pdu() ~ data_start:%p buf_len:%d pkt_len:%d data_len:%d priv_size:%d head_len:%d tail_len:%d datafield_len:%d\n", 
	// 	data_start, 
	// 	data_mbuf->buf_len, 
	// 	data_mbuf->pkt_len, 
	// 	data_mbuf->data_len, 
	// 	data_mbuf->priv_size,
	// 	rte_pktmbuf_headroom(data_mbuf),
	// 	rte_pktmbuf_tailroom(data_mbuf),
	// 	datafield_length);

	if(rte_pktmbuf_tailroom(data_mbuf) < (datafield_length))
	{
		rte_pktmbuf_free(data_mbuf);
		return -1;
	}
	
    mph = (Mbuf_Private_Header*)rte_mbuf_to_priv(data_mbuf);
	mph->protocol_type = protocol_type;
	data_start = (uint8_t*)rte_pktmbuf_append(data_mbuf, datafield_length);	// mbuf里dataroom的起始地址

	rte_memcpy(data_start, ptr_datafield, datafield_length);


// int n = rte_ring_count(ipgw_mgr->ring_fwd_gse_out[pdu_type]);
// printf("get_single_pdu(1) ... n:%d data_mbuf:%p call_count:%d\n", n, data_mbuf, call_count);

    ut_statis_debug.unfrag_pkt_num[port_id]++;

    #if 1
	//rte_pktmbuf_free(data_mbuf);
	if(false == enqueue_encap_ring(data_mbuf, 0))
	{
		rte_pktmbuf_free(data_mbuf);
	}

	#else
	router_func(data_mbuf);
	#endif

// int n = rte_ring_count(ipgw_mgr->ring_fwd_gse_out[pdu_type]);
// printf("get_single_pdu(2) ... n:%d data_mbuf:%p call_count:%d pdu_type:%d\n", n, data_mbuf, call_count, pdu_type);
#if 0  //process control message in control_process
    if(pdu_type>0)
    {
    	struct rte_mbuf* ctrl_mbuf[102];
    	// rte_ring_dequeue(ipgw_mgr->ring_fwd_gse_out[pdu_type], (void**)&ctrl_mbuf);
    	// rte_pktmbuf_free(ctrl_mbuf);

    	int n = rte_ring_count(ipgw_mgr->ring_fwd_gse_out[pdu_type]);
    	int nb_rx = ipgw_dequeue_fwd_gse_out(ctrl_mbuf, n, pdu_type);
    	// printf("n:%d, nb_rx:%d, pdu_type:%d\n", n, nb_rx, pdu_type);
    	for(int i=0; i<nb_rx; i++)
    	{
    		rte_pktmbuf_free(ctrl_mbuf[i]);
    	}
    }
#endif
 
	
	return pdu_type;
}


/********************************************************************************
* 功能：重组GSE包里的分片的PDU第一个分片
* 参数：
		[in]	ptr_datafield	：GSE包里数据字段的起始地址
		[in]	var_header		：GSE头的可变部分
		[in]	timestamp_seq	：属于同一个fragment_ID的分片序号
		[in]	header_length	：GSE头的total_length字段（不含）之后的头字段和扩展头的总字节数
		[in]	frag_len		：分片长度
		[in]	protocol_type	: PDU的协议类型（IP包/控制消息）
		[out]	frag_info		：保存pdu的分片信息
		[out]	buffer_index	：从frag_ID对应的Frag Buffer中，选取的buffer的索引
* 返回值：
		无
********************************************************************************/
int8_t start_reassemble_pdu(uint8_t*  ptr_datafield,  
							GSE_VARIABLE_HEADER var_header, 
							uint16_t  timestamp_seq, 
							uint16_t  header_length, 
							uint16_t  frag_len, 
							uint16_t  protocol_type, 
							FRAG_INFO frag_info[][MAX_FRAG_ID][FRAG_BUFFER_NUM],
							int8_t*   buffer_index,
							uint8_t   port_id)
{
	uint8_t frag_ID = var_header.variable_header_value.fragment_ID;
	int8_t  index = -1, i;
	int8_t* ptr_index;
	LINKNODE* pNode;
	uint8_t   frag_seq = timestamp_seq & 0x07;
	uint16_t  time_stamp = timestamp_seq >> 3;

	ut_statis_debug.fragment_count_start[port_id]++;

	if(!ptr_datafield)
		return POINT_ERROR;

	if(frag_seq > 0)	// 起始分片的序列号必须为0
	{
		return -1;
	}

	// 遍历检查该frag_ID的PDU分片是否已经收到过
	for(i=0; i<FRAG_BUFFER_NUM; ++i)
	{
		if(time_stamp == frag_info[port_id][frag_ID][i].time_stamp)	
		{
			index = i;	// 发现该PDU的分片已经接收过，定位所在FRAG BUFFER
			break;	
		}
	}

	// 该frag_ID的PDU分片还没有收到过
	if(index < 0)	
	{
		// 从frag_ID所属的 N 个 Frag Buffer 中选取一个来保存分片
		ptr_index = (int8_t*)Get_Header_Data(frag_buffer_index_list[port_id][frag_ID], &pNode);

		if(ptr_index)	// 存在已经在用的Frag Buffer，将其从已用的队首中取出再放入队尾
		{
			index = *ptr_index;
			Remove_Node_point(frag_buffer_index_list[port_id][frag_ID], pNode);
			Push_List(frag_buffer_index_list[port_id][frag_ID], pNode);
		}

	}

	// 序号frag_ID的第一个分片长度>0，说明该frag_ID已接收了其它的PDU，要抛弃已接收的PDU数据，改为接收当前PDU
	// 序号frag_ID的第一个分片长度=0，说明该frag_ID已接收了当前PDU的其它分片，继续接收当前PDU
	// 当前frag_ID的缓存区内已经接收了分片
	if(frag_info[port_id][frag_ID][index].frag_recv_num > 0)
	{
		// 已接收分片的time_stamp和当前收到的分片的time_stamp不等，抛弃已接收的分片
		if(time_stamp != frag_info[port_id][frag_ID][index].time_stamp)
		{
			if(frag_info[port_id][frag_ID][index].pdu_node.ptr_pdu)
			{
			    ut_statis_debug.free_confict_num++;
				printf("time_stamp != frag_info[port_id][frag_ID][index].time_stamp\n");
				rte_pktmbuf_free((struct rte_mbuf*)(frag_info[port_id][frag_ID][index].pdu_node.ptr_pdu));
			}
			reset_frag_info(&frag_info[port_id][frag_ID][index]);
		}
		else if(frag_info[port_id][frag_ID][index].frag_len[0] > 0)
		{
			if(frag_info[port_id][frag_ID][index].pdu_node.ptr_pdu)
			{
				rte_pktmbuf_free((struct rte_mbuf*)(frag_info[port_id][frag_ID][index].pdu_node.ptr_pdu));
		        printf("frag_info[port_id][frag_ID][index].pdu_node.ptr_pdu\n");
				ut_statis_debug.free_confict_num++;
			}
			reset_frag_info(&frag_info[port_id][frag_ID][index]);
			--frag_info[port_id][frag_ID][index].frag_recv_num;
			frag_info[port_id][frag_ID][index].pdu_recv_len -= frag_info[port_id][frag_ID][index].frag_len[0];
		}
	}

	frag_info[port_id][frag_ID][index].max_frag_seq = max(frag_info[port_id][frag_ID][index].max_frag_seq, frag_seq);
	frag_info[port_id][frag_ID][index].pdu_recv_len += frag_len;
	frag_info[port_id][frag_ID][index].frag_recv_num++;
	frag_info[port_id][frag_ID][index].frag_len[frag_seq] = frag_len;
	rte_memcpy(frag_info[port_id][frag_ID][index].data_addr[frag_seq], ptr_datafield, frag_len);
	frag_info[port_id][frag_ID][index].pdu_node.pdu_length = var_header.variable_header_value.total_length - header_length; // gse可变头长度要减去frag_id和total_length的长度
	frag_info[port_id][frag_ID][index].time_stamp = time_stamp;

	frag_info[port_id][frag_ID][index].protocol_type = protocol_type;

	switch(protocol_type)
	{
		// 数据是IP包，从IP包内存池分配缓存区
		case PROTOCOL_IPV4:
		case PROTOCOL_IPV6:
		{
			frag_info[port_id][frag_ID][index].pdu_type = IP_PACKET;
			break;
		}
		// 数据是控制信息，从控制信息内存池分配缓存区
		case PROTOCOL_CTRL:
		{
			frag_info[port_id][frag_ID][index].pdu_type = (0 == ((*(ptr_datafield+1)) & 0x40)) ? CTRLMSG_FWD : CTRLMSG_RTN;
			break;
		}

		default:
 
			break;
	}

	struct rte_mbuf* data_mbuf = rte_pktmbuf_alloc(decap_pktmbuf_pool);	// 从内存池申请一个mbuf
	if(!data_mbuf)
	{
	    printf("decap_pktmbuf_pool alloc failed\n");
		return -1;
	}

	Mbuf_Private_Header * mph = (Mbuf_Private_Header*)rte_mbuf_to_priv(data_mbuf);
	mph->protocol_type = protocol_type;

	if(rte_pktmbuf_tailroom(data_mbuf) < (frag_info[port_id][frag_ID][index].pdu_node.pdu_length + 4))
	{
		rte_pktmbuf_free(data_mbuf);
		reset_frag_info(&frag_info[port_id][frag_ID][index]);
		printf("2--start_reassemble_pdu() rte_pktmbuf_free()\n");
		return -1;
	}

	ut_statis_debug.fragment_count_start_ok++;

	rte_pktmbuf_append(data_mbuf, frag_info[port_id][frag_ID][index].pdu_node.pdu_length);  	// mbuf里dataroom的起始地址 
	frag_info[port_id][frag_ID][index].pdu_node.ptr_pdu = (uint8_t*)data_mbuf;		// 保存内存池分配的存储PDU信息和数据的mbuf指针

	*buffer_index = index;

	if(1==frag_info[port_id][frag_ID][index].recv_end && 
	   frag_info[port_id][frag_ID][index].frag_recv_num == (frag_info[port_id][frag_ID][index].max_frag_seq + 1))	// （PDU分片乱序到达时）表示已收完全部分片
	{

		return 1;	// 整理得到完整的PDU
	}

	// 开始计时：对该fragment_ID在超时时限内，要收全所有PDU分片，否则抛弃该PDU
	if(1==frag_info[port_id][frag_ID][index].frag_recv_num)
	{
		frag_info[port_id][frag_ID][index].t0_us = get_current_time();
	}

	return 0;
}


/********************************************************************************
* 功能：重组GSE包里的分片的PDU中间分片
* 参数：
		[in]	pdu_frag_type	: PDU分片包的类型 中间分片 最后分片
		[in]	ptr_datafield	：GSE包里数据字段的起始地址
		[in]	frag_ID			：GSE头里的fragment_ID字段值
		[in]	timestamp_seq	：属于同一个fragment_ID的分片序号
		[in]	frag_len		：分片长度
		[out]	frag_info[]		：保存pdu的分片信息
		[out]	buffer_index	：从frag_ID对应的Frag Buffer中，选取的buffer的索引
* 返回值：
		无
********************************************************************************/
int8_t keep_reassemble_pdu(	uint8_t   pdu_frag_type, 
							uint8_t*  ptr_datafield, 
						   	uint8_t   frag_ID, 
						   	uint16_t  timestamp_seq, 
						   	uint16_t  frag_len, 
						   	FRAG_INFO frag_info[][MAX_FRAG_ID][FRAG_BUFFER_NUM],
						   	int8_t*   buffer_index,
						   	uint8_t   port_id)
{
	int8_t  index = -1, i;
	int8_t* ptr_index;
	LINKNODE* pNode;
	uint8_t   frag_seq = timestamp_seq & 0x07;
	uint16_t  time_stamp = timestamp_seq >> 3;
     ut_statis_debug.fragment_count_end[port_id]++;
	if(!ptr_datafield)
		return POINT_ERROR;

	// 遍历检查该frag_ID的PDU分片是否已经收到过
	for(i=0; i<FRAG_BUFFER_NUM; ++i)
	{
		if(time_stamp == frag_info[port_id][frag_ID][i].time_stamp)	
		{
			index = i;	// 发现该PDU的分片已经接收过，定位所在FRAG BUFFER
			break;	
		}
	}

	// 该frag_ID的PDU分片还没有收到过
	if(index < 0)	
	{

		// 从frag_ID所属的 N 个 Frag Buffer 中选取一个来保存分片
		ptr_index = (int8_t*)Get_Header_Data(frag_buffer_index_list[port_id][frag_ID], &pNode);

		if(ptr_index)	// 存在已经在用的Frag Buffer，将其从已用的队首中取出再放入队尾
		{
			index = *ptr_index;
			Remove_Node_point(frag_buffer_index_list[port_id][frag_ID], pNode);
			Push_List(frag_buffer_index_list[port_id][frag_ID], pNode);
		}


		// 将选取的Frag Buffer的信息初始化
		// if(frag_info[frag_ID][i].pdu_node.ptr_pdu)
		// {
		// 	rte_pktmbuf_free((struct rte_mbuf*)(frag_info[frag_ID][index].pdu_node.ptr_pdu));
		// }
		// reset_frag_info(&frag_info[frag_ID][index]);
	}

	// 该分片的长度大于0，表明之前收到过该分片序号的分片数据；要用刚收到的分片数据替换旧的分片数据
	if(frag_info[port_id][frag_ID][index].frag_len[frag_seq] > 0 && frag_info[port_id][frag_ID][index].frag_recv_num > 0)
	{
		--frag_info[port_id][frag_ID][index].frag_recv_num;
		frag_info[port_id][frag_ID][index].pdu_recv_len -= frag_info[port_id][frag_ID][index].frag_len[frag_seq];
	}

	if(FRAG_END == pdu_frag_type)
	{
		
		frag_info[port_id][frag_ID][index].pdu_recv_len += (frag_len - 4);	// 最后一个分片包，减掉4字节的CRC32校验码，才是PDU的数据长度
		frag_info[port_id][frag_ID][index].recv_end = 1;
	}
	else
	{
		frag_info[port_id][frag_ID][index].pdu_recv_len += frag_len;
	}


	if( frag_info[port_id][frag_ID][index].pdu_node.pdu_length > 0 	&&
		frag_info[port_id][frag_ID][index].pdu_node.pdu_length < frag_info[port_id][frag_ID][index].pdu_recv_len)
	{

		if(frag_info[port_id][frag_ID][index].pdu_node.ptr_pdu)
		{
			rte_pktmbuf_free((struct rte_mbuf*)(frag_info[port_id][frag_ID][index].pdu_node.ptr_pdu));
			printf("frag_info[port_id][frag_ID][index].pdu_node.ptr_pdu\n");
			LOG(LOG_WARNING, LOG_MODULE_MAIN, LOG_WARNING, 
			    LOG_CONTENT("free conflict pdu 1"));
			ut_statis_debug.free_confict_num++;
		}
		reset_frag_info(&frag_info[port_id][frag_ID][index]);

		return -1;
	}


	frag_info[port_id][frag_ID][index].max_frag_seq = max(frag_info[port_id][frag_ID][index].max_frag_seq, frag_seq);
	frag_info[port_id][frag_ID][index].frag_recv_num++;
	frag_info[port_id][frag_ID][index].frag_len[frag_seq] = frag_len;
	rte_memcpy(frag_info[port_id][frag_ID][index].data_addr[frag_seq], ptr_datafield, frag_len);
	frag_info[port_id][frag_ID][index].time_stamp = time_stamp;
	*buffer_index = index;

	if(1==frag_info[port_id][frag_ID][index].recv_end && 
	   frag_info[port_id][frag_ID][index].frag_recv_num == (frag_info[port_id][frag_ID][index].max_frag_seq + 1))	// （PDU分片乱序到达时）表示已收完全部分片
	{
		return 1;	// 整理得到完整的PDU
	}

	// 开始计时：对该fragment_ID在超时时限内，要收全所有PDU分片，否则抛弃该PDU
	if(1==frag_info[port_id][frag_ID][index].frag_recv_num)
	{
		frag_info[port_id][frag_ID][index].t0_us = get_current_time();
	}

	return 0;
}


/********************************************************************************
* 功能：重组GSE包里的分片的PDU最后一个分片
* 参数：
		[out]	frag_info	：保存pdu的分片信息的结构体指针s
* 返回值：
		0：成功重组PDU  小于0：错误
********************************************************************************/
int8_t finish_reassemble_pdu(FRAG_INFO* frag_info, uint16_t port_id)
{
	uint32_t i, CRC32, offset=0;
	// int pdu_type;						// pdu类型：IP包、前向控制消息、反向控制消息

	uint8_t *data_start = NULL;			// mbuf里dataroom的起始地址
	int8_t ret;
	struct rte_ether_hdr eth_hdr = { 0 };

	if(!frag_info)
		return POINT_ERROR;

	// 预先申请的mbuf地址为NULL，抛弃该fragID的分片缓存数据
	if(!frag_info->pdu_node.ptr_pdu)
	{
		reset_frag_info(frag_info);
		printf("finish_reassemble_pdu error 1\n");
		return POINT_ERROR;
	}

	 ut_statis_debug.fragment_count_finish++;
	// pdu_type = frag_info->pdu_type;

	struct rte_mbuf* data_mbuf = (struct rte_mbuf*)(frag_info->pdu_node.ptr_pdu); 	// mbuf的起始地址

	data_start = rte_pktmbuf_mtod(data_mbuf, uint8_t*);

	// mbuf的data room长度小于MAC帧长度，释放该mbuf并初始化该frag_ID的frag Buffer
	if( rte_pktmbuf_data_len(data_mbuf) < (frag_info->pdu_recv_len) )
	{
	    printf("finish_reassemble_pdu:pdu size exceed max mbuf size\n");
		rte_pktmbuf_free(data_mbuf);
		reset_frag_info(frag_info);
		return ret = SIZE_ERROR;
	}

	// eth_hdr.ether_type = frag_info->protocol_type;
	eth_hdr.ether_type = ntohs(frag_info->protocol_type);

	switch(frag_info->pdu_type)
	{
		case IP_PACKET:
		{
			break;
		}
		case CTRLMSG_FWD:
		case CTRLMSG_RTN:
		{


			break;
		}

		default:
			rte_pktmbuf_free(data_mbuf);
			reset_frag_info(frag_info);

			printf("finish_reassemble_pdu error 2\n");
			return ret = -1;
			break;
	}


	//rte_memcpy(data_start, &eth_hdr, sizeof(struct rte_ether_hdr));
	//offset = sizeof(struct rte_ether_hdr);


	for(i=0; i<frag_info->frag_recv_num; i++)
	{
		if(frag_info->data_addr[i])
		{
			if(i < (frag_info->frag_recv_num-1U))
			{
				rte_memcpy(data_start + offset, frag_info->data_addr[i], frag_info->frag_len[i]);
				offset += frag_info->frag_len[i];
			}
			else
			{
				rte_memcpy(data_start + offset, frag_info->data_addr[i], frag_info->frag_len[i] - 4);//CRC 4 bytes
				offset += frag_info->frag_len[i] - 4;
				frag_info->crc = *((uint32_t*)(frag_info->data_addr[i] + frag_info->frag_len[i] - 4));
				frag_info->crc = ntohl(frag_info->crc);
				//memcpy(&frag_info->crc, frag_info->data_addr[i] + frag_info->frag_len[i] - 4, sizeof(uint32_t));
			}
		}
		else	// 保存每个分片的存储地址，只要有一个地址非法，抛弃整个PDU
		{
			rte_pktmbuf_free(data_mbuf);
			reset_frag_info(frag_info);
			printf("finish_reassemble_pdu error 3\n");
			return POINT_ERROR;
		}
	}


	if(offset == frag_info->pdu_node.pdu_length)	/*不要加上CRC32长度*/
	{
		CRC32 = crc32_lookup_table(data_start, frag_info->pdu_node.pdu_length);
		if(CRC32 != frag_info->crc)	// CRC32校验码不相等
		{
			rte_pktmbuf_free(data_mbuf);
			ret = -2;
            printf("CRC32:%X - %X\n", CRC32, frag_info->crc);

		}
		else
		{
			frag_info->t1_us = get_current_time();

			 ut_statis_debug.fragment_count_finish_ok[port_id]++;
			 #if 1
             //write_pcap_file(3,rte_pktmbuf_mtod(data_mbuf, uint8_t *), offset);
	         //rte_pktmbuf_free(data_mbuf);
	         if(false == enqueue_encap_ring(data_mbuf, 0))
	         {
		         rte_pktmbuf_free(data_mbuf);
		         ut_statis_debug.enqueue_encap_ring_failed++;
	         }

	         #else
			
             router_func(data_mbuf);
			 #endif
		
			 ret = 0;
		}
	}
	else
	{
		rte_pktmbuf_free(data_mbuf);
		ret = -3;
	}	

	reset_frag_info(frag_info);
	
	return ret;

}


uint8_t start_end_indicator(GSE_FIXED_HEADER header)
{
	uint8_t ret = (header.Start_Indicator<<1) + header.End_Indicator;
	return ret;
}


int8_t identify_gs(BBHEADER* pBBHeader)
{
	if(!pBBHeader)
		return -1;

	uint8_t crc = mcs_get_crc8((uint8_t*)(pBBHeader), sizeof(BBHEADER)-1);		// 校验CRC8
	if(crc != pBBHeader->CRC8)
	{
		printf("crc error\n");
		return -1;
	}

	// MATYPE1
	if(0==pBBHeader->TS_GS || 1==pBBHeader->TS_GS)
	{
		return 0;
	}
	else
	{
		printf("TS_GS error\n");
		return -1;
	}
}

int8_t identify_isl(ISLHEADER* pISLHeader)
{
	if(!pISLHeader)
		return -1;

	uint32_t crc32 = crc32_lookup_table((uint8_t*)pISLHeader, sizeof(ISLHEADER)-4);		// 校验CRC32
	if(crc32 != pISLHeader->CRC32)
	{
		printf("crc error\n");
		return -1;
	}

	return 0;

}



/********************************************************************************
* 功能：过滤掉MAC地址不合法的GSE包
* 参数：
 [in]	fixed_header	：GSE包头固定部分
 [in]	var_header		：GSE包头可变部分
 [in]	local_air_mac[]	：本地空口MAC地址
 [in]	bfirst_gse		：gse包是frame中第一个包标志
 [in][out]	ptr_previous_mac_info	：上一次收到的MAC地址的信息
* 返回值：
 1～MAC地址合法		小于0～错误代码
********************************************************************************/
int8_t filter_address(GSE_FIXED_HEADER fixed_header, GSE_VARIABLE_HEADER var_header, uint8_t local_air_mac[], uint8_t bfirst_gse, PREVIOUS_MAC_INFO* ptr_previous_mac_info)
{
	int8_t address_size, func_ret = 1;

	if(bfirst_gse)	// frame的第一个gse包必须给出显性的MAC地址
	{
		if(/*LABEL_TYPE_BROADCAST==fixed_header.Label_Type ||*/ LABEL_TYPE_REUSE==fixed_header.Label_Type)
		{
			return (func_ret = -1);
		}
	}

	if(!ptr_previous_mac_info)
	{
		return (func_ret = POINT_ERROR);
	}

	// 广播，无条件全收
	if(LABEL_TYPE_BROADCAST==fixed_header.Label_Type)
	{
		ptr_previous_mac_info->label_type = fixed_header.Label_Type;
		return (func_ret = 1);
	}

// printf("current Label_Type:%d, last label_type:%d label:", fixed_header.Label_Type, ptr_previous_mac_info->label_type);
// for(int i=0; i<6; ++i)
// {
// 	printf("%02X", var_header.var_header_value.label[i]);
// }
// printf(" last mac address:");
// for(int i=0; i<6; ++i)
// {
// 	printf("%02x", ptr_previous_mac_info->mac_address[i]);
// }
// printf("\n");

	//// label_type=11的gse包的前一个gse包label_type不能为10
	/*if(LABEL_TYPE_REUSE==fixed_header.Label_Type && LABEL_TYPE_BROADCAST==ptr_previous_mac_info->label_type)
	{
		return (func_ret = -2);
	}*/

	if(LABEL_TYPE_6B==fixed_header.Label_Type || LABEL_TYPE_3B==fixed_header.Label_Type)
	{
		address_size = (LABEL_TYPE_6B==fixed_header.Label_Type) ? 6 : 3;
		func_ret = equal_mac(var_header.variable_header_value.label, local_air_mac, address_size);

		ptr_previous_mac_info->label_type = fixed_header.Label_Type;
		ptr_previous_mac_info->mac_length = address_size;
		memcpy(ptr_previous_mac_info->mac_address, var_header.variable_header_value.label, address_size);
		
	}
	else if(LABEL_TYPE_REUSE==fixed_header.Label_Type)
	{
		// func_ret = equal_mac(var_header.var_header_value.label, ptr_previous_mac_info->mac_address, ptr_previous_mac_info->mac_length);

		if(LABEL_TYPE_BROADCAST==ptr_previous_mac_info->label_type)
			func_ret = 1;
		else
			func_ret = equal_mac(local_air_mac, ptr_previous_mac_info->mac_address, ptr_previous_mac_info->mac_length);
			
	}



	return func_ret;
}


inline void free_mbuf(struct rte_mbuf* mbuf)
{
	rte_pktmbuf_free(mbuf);
}

// inline void free_mbuf(FRAME_NODE* frame_node)
// {
// 	rte_pktmbuf_free(frame_node->mbuf);
// 	free(frame_node);
// 	frame_node = NULL;
// }





/********************************************************************************
* 功能：计算CRC8校验码
* 参数：
 [in]	pdata：PDU数据的首地址
 [in]	len：PDU数据字节数
* 返回值：
 CRC8校验码
* 方法说明：
 生成多项式 X8+X5+X4+1
********************************************************************************/
uint8_t calc_CRC8(uint8_t *pdata, uint32_t len)
{
	uint8_t Gx = 0x31;	//0xC5;	// 生成多项式
	uint8_t CRC8 = 0;
	int i = 0;

	while (0 != len--)
	{
		CRC8 ^= (*pdata++);

		for (i = 0; i < 8; i++)
		{
			if (CRC8 & 0x80)
			{
				CRC8 <<= 1;
				CRC8 ^= Gx;
			}
			else
			{
				CRC8 <<= 1;
			}
		}
	}

	return CRC8;
}

// 将解封装后的PDU从接收mbuf转移到数据mbuf
// PDU的协议类型、目的IP、包长度
void move_pdu(uint8_t* dst_addr, uint8_t* src_addr, uint16_t data_length, uint16_t protocol_type)
{
	(void)protocol_type;

  	// struct rte_ipv4_hdr* ptr_ipv4_header, ipv4_header;
	// ptr_ipv4_header = (struct rte_ipv4_hdr*)src_addr;

    // 1. 将数据转移到新的mbuf的dataroom
    rte_memcpy(dst_addr, src_addr, data_length);                  // 将PDU拷贝到mbuf的dataroom


    // 2. 将ip包的目的IP和包长度写入mbuf的headroom，方便以后查询
  	// int  offset1 = 0, offset2 = 0;	// 距离mbuf的headroom的起始地址偏移
	// int  len = 0;    
	// if(PROTOCOL_IPV4 == protocol_type)
    // {
    //     offset2 = 16;
    //     len = 4;	
    // }
    // else
    // {
    //     offset2 = 24;
    //     len = 16;	
    // }
    // rte_memcpy(mbuf->buf_addr, &protocol_type, sizeof(uint16_t));         // IP协议类型
    // offset1 += sizeof(uint16_t);
	// if(len>0)
    // {
	// 	rte_memcpy((char*)(mbuf->buf_addr) + offset1, ptr_data + offset2, len);
    // 	offset1 += len;
	// }
    // rte_memcpy((char*)(mbuf->buf_addr) + offset1, &data_length, sizeof(uint16_t));

	// printf("data_mbuf:%p data_len:%d data_length:%d\n", mbuf, mbuf->data_len, data_length);
}










