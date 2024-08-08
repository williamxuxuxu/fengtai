
#include <stdio.h>

#include "common_dpdk.h"
#include "define_struct.h"
#include "encapsulate_frame.h"
#include "global_var.h"
#include "stats.h"
#include "router.h"
#include "rcv_send.h"
#include "system_log.h"
#include "sys_thread.h"
#include "general_module.h"
#include "ring_queue.h"
#include "encapsulate.h"

extern uint64_t outroute_bytes[MAX_DEVICE_NUM];
extern volatile int     force_quit;
extern struct rte_mempool * encap_pktmbuf_pool ;

uint16_t PKT_OVER_HEAD = 24; // with CRC32
extern rte_atomic32_t available_fpga_buff;

/* 编码信息	*/
uint8_t VL_SNR_set[16] =
{ 	0,// QPSK 2/9			(MODCOD)、normal(code_type)、on(pilot_insertion)、1(spreading_factor)
	1,// π/2 BPSK 1/5	(MODCOD)、medium(code_type)、on(pilot_insertion)、1(spreading_factor)
	2,// π/2 BPSK 11/45	(MODCOD)、medium(code_type)、on(pilot_insertion)、1(spreading_factor)
	3,// π/2 BPSK 1/3	(MODCOD)、medium(code_type)、on(pilot_insertion)、1(spreading_factor)
	4,// π/2 BPSK 1/5	(MODCOD)、short (code_type)、on(pilot_insertion)、2(spreading_factor)
	5,// π/2 BPSK 11/45 	(MODCOD)、short (code_type)、on(pilot_insertion)、2(spreading_factor)
	6,		// unassigned
	7,		// unassigned
	8,		// unassigned
	9,// π/2 BPSK 1/5	(MODCOD)、short (code_type)、on(pilot_insertion)、 1(spreading_factor)
	10,	// π/2 BPSK 4/15	(MODCOD)、short (code_type)、on(pilot_insertion)、 1(spreading_factor)
	11,	// π/2 BPSK 1/3	(MODCOD)、short (code_type)、on(pilot_insertion)、 1(spreading_factor)
	12,	// dummy		 	(MODCOD)、  -   (code_type)、off(pilot_insertion)、1(spreading_factor)
	13,		// unassigned
	14,		// unassigned
	15 };		// unassigned

static const uint8_t S2_map_table[4] = { 0, 1, 2, 3 };


static uint8_t pls_code_normal_cb_insert_pilot[] = {   2,  // Dummy
													// 1/4	1/3   2/5	1/2   3/5	2/3   3/4	4/5   5/6	8/9  9/10
	                                                     5,  9,    13,   17,   21,   25,   29,   33,   37,   41,   45,  // QPSK
													                           49,   53,   57,         61,   65,   69,  // 8PSK
													                                 73,   77,   81,   85,   89,   93,  // 16APSK
													                                       97,  101,  105,  109,  113   // 32APSK
												   };

static uint8_t pls_code_short_cb_insert_pilot[] =  {  2,  // Dummy
													// 1/4	1/3   2/5	1/2   3/5	2/3   3/4	4/5   5/6	8/9  9/10
													     7,  11,   15,   19,   23,   27,   31,   35,   39,   43,    0,  // QPSK
													                           51,   55,   59,         63,   67,    0,  // 8PSK
													                                 75,   79,   83,   87,   91,    0,  // 16APSK
													                                       99,  103,  107,  111,    0   // 32APSK
												   };  // 0 means do NOT exist




/********************************************************************************
* 功能：初始化BBHeader，同时也初始化L2L1_Header的序列号字段
* 参数：
 [in][out]	pBBHeader：被初始化的BBHeader指针
 [in][out]	pL2L1_header：L2往L1发生的数据包的包头
* 返回值：	无
********************************************************************************/
void init_BBHeader(BBHEADER *pBBHeader, TX_FWD_DATA *pL2L1_header)
{
	if(!pL2L1_header || !pBBHeader)
	{
		return;
	}

	pBBHeader->TS_GS = 1;	// 持续GS流方式
	pBBHeader->SIS_MIS = 1;	// 单输入流
	pBBHeader->CCM_ACM = 0;	// ACM方式
	pBBHeader->ISSYI = 0;	// 不使用输入流同步
	pBBHeader->NPD = 0;		// 不使用空包删除
	pBBHeader->RO = 0;		// 实际上需要根据配置（文件）
	pBBHeader->Matype2 = 0;	// 保留 置0
	pBBHeader->UPL = 0;		// 持续GS流 置0
	pBBHeader->DFL = 0;		// 实际值需要根据现场计算结果
	pBBHeader->SYNC = 0;	// 持续GS流不存在同步字节
	pBBHeader->SYNCD = 0;	// 持续GS流
	pBBHeader->CRC8 = 0;	// 实际值需要根据现场计算结果

	memset(pL2L1_header, 0, sizeof(L2L1_HEADER));
}

/********************************************************************************
* 功能：封装BBHeader
* 参数：
*	 [in]	initialBBHeader：被初始化的BBHeader
 	 [in]	bbframe_size：BBFrame的大小 Bytes（包括 BBHeader + data + padding）
 	 [in]	data_size：要封装的数据长度 Bytes
 	 [out]	pBBHeader：需要被封装的BBHeader的指针
* 返回值：
 	 0～封装成功  小于0～错误代码
* 方法说明：
 	 使用初始化了的initialBBHeader的值，数据字段值和CRC8校验码在函数里计算
********************************************************************************/
int8_t package_BBHeader(BBHEADER initialBBHeader, uint16_t bbframe_size, uint16_t data_size, BBHEADER *pBBHeader)
{
	if (!pBBHeader)
	{
		return POINT_ERROR;
	}

	// memcpy(pBBHeader, &initialBBHeader, sizeof(BBHEADER));	// 先拷贝初始化了的initialBBHeader的值
	rte_memcpy(pBBHeader, &initialBBHeader, sizeof(BBHEADER));

	// 再按照实际数据的长度（bytes）修改 DFL（数据字段长度bits）的值，及现场计算CRC8校验码
	if (data_size <= (bbframe_size - sizeof(BBHEADER)))
	{
		// pBBHeader->DFL = data_size * 8;
		pBBHeader->DFL = rte_cpu_to_be_16(data_size * 8);
//		pBBHeader->CRC8 = crc8_lookup_table((uint8_t*) pBBHeader, sizeof(BBHEADER) - 1);
		pBBHeader->CRC8 = mcs_get_crc8((uint8_t*) pBBHeader, sizeof(BBHEADER) - 1);
	}
	else
	{
		return SIZE_ERROR;
	}

	return 0;
}


void init_ISLHeader(ISLHEADER *pISLHeader, TX_FWD_DATA *pL2L1_header)
{
	if(!pL2L1_header || !pISLHeader)
	{
		return;
	}

	pISLHeader->D = 0;
	pISLHeader->M = 0;	
	pISLHeader->E = 0;	
	pISLHeader->Link_Control = 0;	
	pISLHeader->Frame_Sequence = 0;		
	pISLHeader->DFL = 0;		
	pISLHeader->CRC32 = 0;	
	memset(pL2L1_header, 0, sizeof(L2L1_HEADER));
}

int8_t package_ISLHeader(ISLHEADER initialISLHeader, uint16_t islframe_size, uint16_t data_size, ISLHEADER *pISLHeader)
{
	if (!pISLHeader)
	{
		return POINT_ERROR;
	}

	rte_memcpy(pISLHeader, &initialISLHeader, sizeof(ISLHEADER));

	if (data_size <= (islframe_size - sizeof(ISLHEADER)))
	{
	    pISLHeader->DFL = data_size * 8;
		pISLHeader->CRC32 = crc32_lookup_table((uint8_t*) pISLHeader, sizeof(ISLHEADER) - 4);
		   
	}
	else
	{
		return SIZE_ERROR;
	}

	return 0;
}

uint16_t isl_padding(uint16_t islframe_size, ISLHEADER *pISLHeader)
{
	uint16_t padding_bytes = 0;


	padding_bytes = islframe_size - sizeof(ISLHEADER) - (pISLHeader->DFL >> 3);
	
	return padding_bytes;
}


/********************************************************************************
* 功能：计算一个BBFrame中的padding字节数
* 参数：
 [in]	bbframe_size：基带帧BBFrame的字节数（包括 BBHeader + data + padding）
 [in]	pBBHeader：	  被封装的BBHeader的指针
* 返回值：
 需要padding的字节数
* 方法说明：
 由于BBFrame的bit总长度是8的整倍数，GSE包的长度单位是字节，因此padding的长度也可以字节为单位
********************************************************************************/
uint16_t padding(uint16_t bbframe_size, BBHEADER *pBBHeader)
{
	uint16_t padding_bytes = 0;

	if (pBBHeader)
	{
		// padding_bytes = bbframe_size - sizeof(BBHEADER) - (pBBHeader->DFL >> 3);
		padding_bytes = bbframe_size - sizeof(BBHEADER) - (rte_be_to_cpu_16(pBBHeader->DFL) >> 3);
	}

	return padding_bytes;
}

uint8_t mcs_get_pls_code(uint8_t modcod, bool normal_code_block_size)
{
	if(modcod > 28) {
		return 0;
	}
	
	if(normal_code_block_size == true) {
		return pls_code_normal_cb_insert_pilot[modcod];
	}
	else {
		return pls_code_short_cb_insert_pilot[modcod];
	}
}


/********************************************************************************
* 功能：封装L2发送给L1信息的报文头L2L1_HEADER
* 参数：
 [in]	frame_size：	基带帧BBFrame的大小 Bytes
 [in]	encode_info：	基带帧BBFrame要采用的调制编码方式
 [out]	pL2L1_header：	指向报文头L2L1_HEADER的指针
* 返回值：
 0～封装成功  小于0～错误代码
********************************************************************************/
#define FWD_Tx_FWD_Data_PRI_DATA  ((uint8_t) 2)
int8_t package_L2L1_Header(uint16_t frame_size, ENCODE_INFO encode_info, TX_FWD_DATA *pL2L1_header)
{
	if (!pL2L1_header)
		return POINT_ERROR;
	 memset(pL2L1_header, 0, sizeof(TX_FWD_DATA));

	
	//pL2L1_sub_header->Request_ID			  = rte_cpu_to_be_32(rte_be_to_cpu_32(pL2L1_sub_header->Request_ID) + 1);
//	pL2L1_header->request_id			  = rte_cpu_to_be_32((uint32_t)rte_atomic32_add_return(&tx_fwd_data_req_id, 1));
	pL2L1_header->base_band_signal_type	  = 1;
	pL2L1_header->base_singal_version = 1;
	pL2L1_header->scramble_index		  = 1;	// do NOT know
//	pL2L1_sub_header->PLS_Code				  = map_PLScode(encode_info);
	pL2L1_header->psl_code				  = mcs_get_pls_code(encode_info.modcod, true);
	pL2L1_header->vl_snr_set			  = 1; //encode_info.VL_SNR_set;
	pL2L1_header->priority				  = FWD_Tx_FWD_Data_PRI_DATA;
	pL2L1_header->KBCH_Length			  = rte_cpu_to_be_16(frame_size);


	pL2L1_header->Super_Frame_Number  = 0;
	
	
	pL2L1_header->Timestamp_in_seconds	  = 0;
	pL2L1_header->Timestamp_in_nanosecond = 0;

	return 0;
}

/********************************************************************************
* 功能：进行PLScode的编码值的映射
* 参数：
 [in]	encode_info：基带帧BBFrame要采用的调制编码方式
* 返回值：
 PLScode的编码值
* 方法说明：
 用MODCOD值、code_type和pilot_insertion定位
 采用DVB-S2标准时，MODCOD取值[0,31]；采用DVB-S2X标准时，MODCOD取值[64,127]。
********************************************************************************/
uint8_t map_PLScode(ENCODE_INFO encode_info)
{
	uint8_t PLS_code = 255;

	if (encode_info.modcod <= 31)			// DVB-S2标准
	{
		PLS_code = 4 * encode_info.modcod + S2_map_table[encode_info.code_type << 1 | encode_info.pilot_insertion];
	}
	else if (64 <= encode_info.modcod && encode_info.modcod <= 127)		// DVB-S2X标准
	{
		PLS_code = 2 * encode_info.modcod + S2_map_table[encode_info.pilot_insertion];
	}

	return PLS_code;
}




uint32_t write_codeblock(L2L1_MSG_HEADER *inner_msg_header, TX_FWD_DATA *pL2L1_header, uint8_t *frame_header, uint16_t frame_header_size, GSE_GROUP* pGSE_group, uint16_t padding_bytes, uint16_t mapping_id)
{
	uint8_t i = 0;
	uint8_t j = 0;
	uint16_t offset = 0;
	uint16_t PDU_num = 0;
	ONE_PDU *pOne_PDU = 0;
	ONE_GSE *pOne_GSE = 0;
	uint8_t bPDUConcat = 0;

	uint8_t l2l1_header_size = 0;

	// uint16_t *pProtocol_type = NULL;			// 指向GSE包头的Protocol_type字段的指针
	// GSE_FIXED_HEADER *pFixed_header = NULL;		// 指向GSE报文头的指针

	uint8_t	buffer[FIFO_ITEM_SIZE];
	
	if(!inner_msg_header || !pGSE_group || !pGSE_group->pOneGSE)		// 指针为空，返回写入的字节数为0
		return 0;

	// 写入L2L1_header，先写入一个buffer，下一步在将buffer里的数据写入codeblock buffer
	// memcpy(buffer, pL2L1_header, sizeof(L2L1_HEADER));

	#if 1
	rte_memcpy(buffer + offset, inner_msg_header, sizeof(L2L1_MSG_HEADER));
	offset += sizeof(L2L1_MSG_HEADER);
	#endif
    #if 0
	uint8_t src_mac[6] = {0x68, 0x05, 0xCA, 0x88, 0x87, 0xF7};
    uint8_t ether_header[14];
    uint8_t dst_mac[6] = {0x68, 0x05, 0xCA, 0x88, 0x87, 0xF6};	// 目的MAC地址
    #define ETH_P_DEAN  0x8874 //自定义的以太网协议type
    memcpy (ether_header, dst_mac, 6);
	
    memcpy (ether_header + 6, src_mac, 6);
    ether_header[12] = (ETH_P_DEAN & 0xFF00) >> 8;
    ether_header[13] = ETH_P_DEAN & 0x00FF;
	
    memcpy(buffer, ether_header, 14);
	offset = 14;
    #endif

	if(NULL != pL2L1_header)
	{
	    rte_memcpy(buffer + offset, pL2L1_header, sizeof(L2L1_HEADER));
	    offset += sizeof(L2L1_HEADER);
		l2l1_header_size = sizeof(L2L1_HEADER);
	}


	// 写入BBHeader，先写入一个buffer，下一步在将buffer里的数据写入codeblock buffer
	// memcpy(buffer+offset, pBBHeader, sizeof(BBHEADER));
	rte_memcpy(buffer+offset, frame_header, frame_header_size);
	offset += frame_header_size;						// CodeBlock Buffer的接收数据地址按已接收的BBHEADER字节数移动

	//Take_Lock(&(pCodeBlockFifo->lock_write_fifo));
	//fifo_write_special(pCodeBlockFifo, 0, buffer, sizeof(L2L1_HEADER)+sizeof(BBHEADER), 0);
	
	// 依次放入GSE包，pGSE_info每+1，表示移到下一个GSE包
	for (i = 0; i < pGSE_group->gse_num; i++)
	{
		bPDUConcat = 0;

		pOne_GSE = pGSE_group->pOneGSE + i;
		PDU_num = pOne_GSE->pdu_num;		// 读取当前的一个GSE包里有多少个PDU

	    memcpy(buffer+offset, pOne_GSE->GSE_header, pOne_GSE->GSE_header_size);		// 放入GSE报文头
		//fifo_write_special(pCodeBlockFifo, offset, pOne_GSE->GSE_header, pOne_GSE->GSE_header_size, 0);	// 放入GSE报文头
		offset += pOne_GSE->GSE_header_size;						// CodeBlock Buffer的接收数据地址按已接收的GSE_header字节数移动


		// 放入option扩展头
		for (j = 0; j < (pOne_GSE->opt_ext_header_info.header_num); j++)
		{
			// 扩展头的数据部分
			memcpy(buffer+offset, &(pOne_GSE->opt_ext_header_info.header_value[j]), pOne_GSE->opt_ext_header_info.header_size[j] - 2);
			//fifo_write_special(pCodeBlockFifo, offset, &(pOne_GSE->opt_ext_header_info.header_value[j]), pOne_GSE->opt_ext_header_info.header_size[j] - 2, 0);
			offset += pOne_GSE->opt_ext_header_info.header_size[j] - 2;

			// 扩展头的next type部分
//			*(uint16_t*)(buffer+offset) = pOne_GSE->opt_ext_header_info.next_type[j];
			//fifo_write_special(pCodeBlockFifo, offset, &(pOne_GSE->opt_ext_header_info.next_type[j]), 2, 0);
			memcpy(buffer+offset, &(pOne_GSE->opt_ext_header_info.next_type[j]), 2);
			offset += 2;

		}

		// 放入mandatory扩展头
		for (j = 0; j < (pOne_GSE->man_ext_header_info.header_num); j++)
		{
			// 扩展头的数据部分存储区地址
			if (pOne_GSE->man_ext_header_info.value_address)
			{
				//fifo_write_special(pCodeBlockFifo, offset, &(pOne_GSE->man_ext_header_info.next_type[j]), pOne_GSE->man_ext_header_info.header_size[j] - 2, 0);
				memcpy(buffer+offset, &(pOne_GSE->man_ext_header_info.next_type[j]), pOne_GSE->man_ext_header_info.header_size[j] - 2);
			}
			else
			{
				//fifo_write_special(pCodeBlockFifo, offset, &(pOne_GSE->man_ext_header_info.next_type[j]), pOne_GSE->man_ext_header_info.header_size[j] - 2, 0);
				memcpy(buffer+offset, &(pOne_GSE->man_ext_header_info.next_type[j]), pOne_GSE->man_ext_header_info.header_size[j] - 2);
			}
			offset += pOne_GSE->man_ext_header_info.header_size[j] - 2;

			// 扩展头的next type部分
//			*(uint16_t*)(buffer+offset) = pOne_GSE->man_ext_header_info.next_type[j];
			//fifo_write_special(pCodeBlockFifo, offset, &(pOne_GSE->man_ext_header_info.next_type[j]), 2, 0);
			memcpy(buffer+offset, &(pOne_GSE->man_ext_header_info.next_type[j]), 2);
			offset += 2;

		}

		// 把GSE报文头从中转buffer写入codeblock buffer：固定头+可变头+扩展头
//		fifo_write_special(pCodeBlockFifo, 0, buffer, offset, 0);

		// 按PDU在GSE包里的排列顺序，将PDU拷贝入CodeBlock缓冲区
		for (j = 0; j < PDU_num; j++)
		{
			pOne_PDU = &((pGSE_group->pOneGSE + i)->one_PDU[j]);		// 读取当前的一个GSE包里第一个PDU信息:PDU地址、PDU长度
			if(bPDUConcat)
			{
//				*(uint16_t*) (buffer + offset) = pOne_PDU->pdu_size;
				//fifo_write_special(pCodeBlockFifo, offset, &(pOne_PDU->pdu_size), 2, 0);
				uint32_t pdu_size = htonl(pOne_PDU->pdu_size);
				memcpy(buffer+offset, &(pdu_size), 2);

				offset += 2;
			}

			// 直接从PDU的物理存储区拷贝到codeblock buffer
			//fifo_write_special(pCodeBlockFifo, offset, pOne_PDU->pStart, pOne_PDU->pdu_size, 0);
			memcpy(buffer+offset, pOne_PDU->pStart, pOne_PDU->pdu_size);
			offset += pOne_PDU->pdu_size;		// CodeBlock Buffer的接收数据地址按已接收的PDU字节数移动

			// 如果该PDU是分片PDU，且是最后一个分片（该GSE包只能包含这一个PDU分片）
			if(!pOne_GSE->fragment_flag)
			{
				rte_pktmbuf_free((struct rte_mbuf *)(pOne_PDU->mbuf_address));
				ut_statis_debug.mbuf_free_normal_num[mapping_id] ++;
			}
			else
			{
				if(FRAGMENT_TYPE_E==pOne_GSE->fragment_type)
				{
				    memcpy(buffer+offset, &(pOne_GSE->crc32), sizeof(pOne_GSE->crc32));
					//fifo_write_special(pCodeBlockFifo, offset, &(pOne_GSE->crc32), sizeof(pOne_GSE->crc32), 0);
					offset += sizeof(pOne_GSE->crc32);

					rte_pktmbuf_free((struct rte_mbuf*)(pOne_PDU->mbuf_address));
					ut_statis_debug.mbuf_free_normal_num[mapping_id]++;
				}
				else
				{
					// printf("no execute rte_pktmbuf_free:%lu frag:%d type:%d\n", test_free_count, pOne_GSE->fragment_flag, pOne_GSE->fragment_type);
				}
			}

		}
	}

	if (padding_bytes > 0)		// 如果需要padding，frame的剩余字节补0
	{
		//memset((char*)pCodeBlockFifo + pCodeBlockFifo->fifo_offset + pCodeBlockFifo->index_write * pCodeBlockFifo->item_size + offset, 0, padding_bytes);
		//pCodeBlockFifo->write_byte += padding_bytes;
		memset(buffer+offset, 0, padding_bytes);
		offset += padding_bytes;
	}

	ut_statis_debug.encap_frame_count[mapping_id]++;
	ut_statis_debug.encap_gse_count[mapping_id] += pGSE_group->gse_num;
	// printf("gse_num:%d, padding_bytes:%d gse_num_sum:%d frame_count:%d\n", 
	// 		pGSE_group->gse_num, padding_bytes, gse_num_sum, frame_count);
	
    #if 1
	//ROUTERHEADER *router_header;
    //router_header = (ROUTERHEADER *)(buffer+l2l1_header_size +sizeof(L2L1_MSG_HEADER)+frame_header_size);
    struct rte_mbuf* send_mbuf = get_encap_mbuf(); 
	if(NULL == send_mbuf)
	{
	    printf("memory alloc failed\n");
		return 0;
	}

	uint8_t *tmp_buffer;
	tmp_buffer = rte_pktmbuf_mtod(send_mbuf, uint8_t*);
	memcpy(tmp_buffer, buffer, offset);
    tmp_buffer = (uint8_t*)rte_pktmbuf_append(send_mbuf, offset);	
	enqueue_snd_bb_ring(send_mbuf, mapping_id);

	outroute_bytes[mapping_id] += offset;
	#else
	mbuf = rte_pktmbuf_alloc(encap_pktmbuf_pool);
	if(NULL == mbuf)
	{
        return 0;
	}
	else
	{
	    char * data_start;
	    data_start = (uint8_t*)rte_pktmbuf_append(mbuf, offset);	
	    rte_memcpy(data_start, buffer, offset);
     
	    //write_pcap_file(10,rte_pktmbuf_mtod(mbuf, uint8_t *), mbuf->data_len);
		rte_pktmbuf_free(mbuf);
		
	}
	#endif

	return (offset-l2l1_header_size -sizeof(L2L1_MSG_HEADER));
}




#ifdef TAG_ROUTE
void encode_inner_header(L2L1_MSG_HEADER *inner_msg_header,  uint16_t frame_size, uint8_t mapping_id, uint8_t direction)
{
    if(g_st_app_conf.device_id[mapping_id] > 255)//device num > 256 
    {
        printf("error device num\n");
	}

    #if PCIE_SWITCH
	inner_msg_header->m_cDstMacAddress[3] = g_st_app_conf.device_id[mapping_id];
	*(uint16_t *)&inner_msg_header->m_cDstMacAddress[4] = g_st_app_conf.next_hop_id[mapping_id];
	#else
	memcpy(inner_msg_header->m_cDstMacAddress, g_st_app_conf.dst_mac[mapping_id], 6);
	#endif

	memcpy(inner_msg_header->m_cSrcMacAddress, g_st_app_conf.local_mac[direction], 6);
	//inner_msg_header->m_cSrcMacAddress[3] = g_st_app_conf.router_id;
	inner_msg_header->V = 1;
	inner_msg_header->m_cType = rte_cpu_to_be_16(g_st_app_conf.protocol_type[mapping_id]);//星上板间数据
        	inner_msg_header->R = 0;
	if((BBFRAME_PACK == g_st_app_conf.port_to_pkt_type[mapping_id])
		||(ENUM_FWD_Tx_FWD_Data == g_st_app_conf.port_to_pkt_type[mapping_id]))
	{
	    inner_msg_header->msg_len = rte_cpu_to_be_16(sizeof(L2L1_MSG_HEADER) - sizeof(MAC_FRAME_HEADER)+ sizeof(L2L1_HEADER) + frame_size);
	}
	else
	{
        inner_msg_header->msg_len = rte_cpu_to_be_16(sizeof(L2L1_MSG_HEADER) - sizeof(MAC_FRAME_HEADER) + frame_size);
	}
	
	inner_msg_header->msg_type = g_st_app_conf.port_to_pkt_type[mapping_id];
	inner_msg_header->reserve = 0;

	return;

}
#else
void encode_inner_header(L2L1_MSG_HEADER *inner_msg_header,  uint16_t frame_size, uint8_t mapping_id, uint8_t direction)
{
    if(g_st_app_conf.device_id[mapping_id] > 255)//device num > 256 
    {
        printf("error device num\n");
	}
	
	inner_msg_header->m_cDstMacAddress[0] = 0x10;  // 00:1B:21:E2:90:0E
	inner_msg_header->m_cDstMacAddress[1] = 0x00;
	inner_msg_header->m_cDstMacAddress[2] = 0x00;
	inner_msg_header->m_cDstMacAddress[3] = 0x00;
	inner_msg_header->m_cDstMacAddress[4] = 0x00;
	inner_msg_header->m_cDstMacAddress[5] = 0x50;
	memcpy(inner_msg_header->m_cSrcMacAddress, g_st_app_conf.local_mac[direction], 6);
	inner_msg_header->V = 1;
	inner_msg_header->m_cType = rte_cpu_to_be_16(0x9101);//星上板间数据
    inner_msg_header->R = 0;
	if((BBFRAME_PACK == g_st_app_conf.port_to_pkt_type[mapping_id])
		||(ENUM_FWD_Tx_FWD_Data == g_st_app_conf.port_to_pkt_type[mapping_id]))
	{
	    inner_msg_header->msg_len = rte_cpu_to_be_16(sizeof(L2L1_MSG_HEADER) - sizeof(MAC_FRAME_HEADER)+ sizeof(L2L1_HEADER) + frame_size);
	}
	else
	{
        inner_msg_header->msg_len = rte_cpu_to_be_16(sizeof(L2L1_MSG_HEADER) - sizeof(MAC_FRAME_HEADER) + frame_size);
	}
	
	inner_msg_header->msg_type = g_st_app_conf.port_to_pkt_type[mapping_id];
	inner_msg_header->reserve = 0;

	return;

}

#endif

/********************************************************************************
* 功能：   frame封装处理过程
 调用 package_BBHeader() 生成 BBHeader
 调用 padding() 计算padding字节数
 调用 package_L2L1_Header() 生成 L2L1_Header
 调用 write_codeblock() 将 BBFrame 写入 code block缓冲区
* 参数：
 [in]	GSE_group：将要被封装到一个frame里的所有GSE包的信息
 [in]	frame_payload：frame的payload字节数（包括 data + padding，不包括 BBHeader）
 [in]	data_size：	  frame的数据字段的字节数
 [in]	encode_info： 采用的编码调制方式等信息
 [in]	pFrame_param：封装frame需要的参数
* 返回值：
 0～处理成功		小于0～错误代码
********************************************************************************/
int8_t encapsulate_frame(GSE_GROUP GSE_group, uint16_t frame_max_payload, uint16_t data_size, ENCODE_INFO encode_info, FRAME_PARAM *pFrame_param, uint16_t mapping_id)
{
	BBHEADER BBHeader;	// BBHeader变量，只限frame封装器使用，保存BBHeader每一次的输出值
	ISLHEADER ISLHeader;	
	L2L1_MSG_HEADER inner_msg_header;
	uint16_t padding_bytes = 0;
	int8_t ret1 = 0;
	int32_t ret2 = 0;
	int8_t ret3 = 0;
	uint16_t frame_size;

	ONE_PDU *pOne_PDU = 0;		(void)pOne_PDU;
	ONE_GSE *pOne_GSE = 0;		(void)pOne_GSE;
	uint64_t offset = 0;		(void)offset;
	uint8_t	buffer[FRAME_HEADER_SIZE] = {0};
	uint16_t buffer_size = 0;

	if(!pFrame_param)
	{
		return POINT_ERROR;
	}

	if(0==GSE_group.gse_num)
	{
		return SIZE_ERROR;
	}

	

	switch (g_st_app_conf.port_to_pkt_type[mapping_id]) 
	{
		  case ISL_PACK:
		  	   {
		  	       frame_size = sizeof(ISLHEADER) + frame_max_payload;
				   
		  	       ret1 = package_ISLHeader(pFrame_param->initialISLHeader, frame_size, data_size, &ISLHeader);
	               if (ret1 < 0)
	               {
	                   return ret1;
	               }
			
			       padding_bytes = isl_padding(frame_size, &ISLHeader);
				   ISLHeader.DFL = htons(ISLHeader.DFL);
		  	   }
			   break;
	
		  case BBFRAME_PACK:
		  case ENUM_FWD_Tx_FWD_Data:
			   {
			      frame_size = sizeof(BBHEADER) + frame_max_payload;
				  
			      ret1 = package_BBHeader(pFrame_param->initialBBHeader, frame_size, data_size, &BBHeader);
	              if (ret1 < 0)
	              {
	                  return ret1;
	              }
			     
				  padding_bytes = padding(frame_size, &BBHeader);
                  //BBHeader.DFL = htons(BBHeader.DFL);
				  
		      }
			  break;
	
		  default:
			  break;
	
    }

	encode_inner_header(&inner_msg_header, frame_size, mapping_id, g_st_app_conf.port_id[mapping_id]);

	++(pFrame_param->frame_count);

	switch (g_st_app_conf.port_to_pkt_type[mapping_id]) 
	{
		  case ISL_PACK:
		       {   
		           memcpy(buffer, &ISLHeader, sizeof(ISLHEADER));
				   buffer_size = sizeof(ISLHEADER);
				   ret2 = write_codeblock(&inner_msg_header, NULL, buffer, buffer_size, &GSE_group, padding_bytes,mapping_id);
		  	   }
			   break;
	
		  case BBFRAME_PACK:
		  case ENUM_FWD_Tx_FWD_Data:
			  {
			  	   ret1 = package_L2L1_Header(frame_size, encode_info, &(pFrame_param->L2L1_header));
	               if (ret1 < 0)
	               {
		               return ret1;
	               }
			       memcpy(buffer, &BBHeader, sizeof(BBHEADER));
				   buffer_size = sizeof(BBHEADER);
				   ret2 = write_codeblock(&inner_msg_header, &(pFrame_param->L2L1_header), buffer, buffer_size, &GSE_group, padding_bytes, mapping_id);
		      }
			  break;
	
		  default:
			  break;
	
    }
	
	

	if (0 == ret2)
	{
		ret3 = POINT_ERROR;
	}
	else if (ret2 == frame_size)
	{
		ret3 = 0;
	}
	else
	{
		ret3 = SIZE_ERROR;
	}

	return ret3;
}

#if 0
static int fc_bbframe_send_ignore_interval(uint16_t us_serial_number)
{
    struct rte_mbuf *pkts_burst[1];

    uint64_t cur_indication_tsc   = 0;
    uint64_t pre_indication_tsc   = 0;
    uint64_t indication_intvl_tsc = 100;//unit is 200us
	char *data_start;
      us_serial_number =2;

    while(force_quit == false) {
		/*if(snd_bb_ring_is_null(us_serial_number) == 1) 
		{  // NO data to Tx

			// wait next round Tx
			ut_statis_debug.free_indication_num++;
			rte_atomic32_clear(&available_fpga_buff);
			continue;
		}*/

		uint32_t fpga_buff_byte = (uint32_t)rte_atomic32_read(&available_fpga_buff);
		if(fpga_buff_byte == 0) {
			//printf("available_fpga_buff is zero\n");
			ut_statis_debug.free_indication_num++;
			continue;
		}
		else {
			rte_atomic32_clear(&available_fpga_buff);
		}

		cur_indication_tsc  = get_current_time_us();
	    if(cur_indication_tsc-pre_indication_tsc < indication_intvl_tsc) {  // too close indication
	        //printf("use how long time:%d\n", cur_indication_tsc-pre_indication_tsc);
		    continue;
	    }
	    pre_indication_tsc = cur_indication_tsc;
		uint32_t has_send_bytes = 0;
		
        while(force_quit == false && has_send_bytes < fpga_buff_byte && cur_indication_tsc < pre_indication_tsc + indication_intvl_tsc)
        {
			if(dequeue_snd_bb_ring(pkts_burst, 1, us_serial_number) == 0) 
			{	// deq failed
				continue;
			}

			if(0 == has_send_bytes)
            {
                ut_statis_debug.used_indication_num++;
			}

		    data_start = rte_pktmbuf_mtod(pkts_burst[0], char*);

		    snd_pkt(data_start, pkts_burst[0]->data_len,  g_st_app_conf.port_id[us_serial_number]);
			rte_pktmbuf_free(pkts_burst[0]); 
			has_send_bytes       += pkts_burst[0]->data_len;
			cur_indication_tsc = get_current_time_us();
        } 
		

		
		ut_statis_debug.total_indication_bytes += fpga_buff_byte;
		ut_statis_debug.total_bb_bytes += has_send_bytes;
    }

    return 0;
}
#endif


#if 1
static int fc_bbframe_send_ignore_interval(uint16_t us_serial_number)
{
    struct rte_mbuf *pkts_burst[1];

    uint64_t cur_indication_tsc   = 0;
    uint64_t pre_indication_tsc   = 0;
    uint64_t indication_intvl_tsc = 100;//unit is 200us
	char *data_start = NULL;
	uint32_t send_bytes = 0;

	uint64_t wire_size;
	uint64_t link_speed_bps = 560 * 1000000;
	uint64_t pps;
	uint64_t cpp;
	uint64_t tx_cycles;
    uint16_t mbuf_len;
    us_serial_number =2;
	
 
    while(force_quit == false) {
#if 0
		uint32_t fpga_buff_byte = (uint32_t)rte_atomic32_read(&available_fpga_buff);
		if(fpga_buff_byte == 0) {
			//printf("available_fpga_buff is zero\n");
			ut_statis_debug.free_indication_num++;
			continue;
		}
		else
		{
            rte_atomic32_clear(&available_fpga_buff);
		}
		#endif

		while(1)
		{
                for(uint8_t i=0; i<g_st_app_conf.encap_ring_queue_num; i++)
	        {
			    if(dequeue_snd_bb_ring(pkts_burst, 1, i) == 0) 
			    {	// deq failed
				    continue;
			    }
				
				data_start = rte_pktmbuf_mtod(pkts_burst[0], char*);
				send_bytes		 += pkts_burst[0]->data_len;

			//if((send_bytes < fpga_buff_byte)&&(NULL != data_start))
			{
                            //printf("snd bbframe\n");
			    snd_pkt(data_start, pkts_burst[0]->data_len,	0);
		            mbuf_len  = rte_pktmbuf_pkt_len(pkts_burst[0]);
			    rte_pktmbuf_free(pkts_burst[0]); 
				data_start = NULL;
				ut_statis_debug.total_bb_bytes += send_bytes;
				//uf_len  = rte_pktmbuf_pkt_len(pkts_burst[0]);
			    wire_size = (mbuf_len + PKT_OVER_HEAD) * 8;
			    pps       = link_speed_bps / wire_size;
			    cpp       = 1000000 / pps;
			    tx_cycles = cpp;
				cur_indication_tsc  = get_current_time_us();
				while(force_quit == false && get_current_time_us() < cur_indication_tsc + tx_cycles);
			}
                        }
			#if 0
			else
			{
			   // rte_atomic32_clear(&available_fpga_buff);
				ut_statis_debug.used_indication_num++;
			   
			    send_bytes = pkts_burst[0]->data_len;
				ut_statis_debug.total_indication_bytes += fpga_buff_byte; 
			
			    break;
			}
			#endif
		}
                usleep(100);

    }

    return 0;
}
#else
static int fc_bbframe_send_ignore_interval(uint16_t us_serial_number)
{
    struct rte_mbuf *pkts_burst[1];

    uint64_t cur_indication_tsc   = 0;
    uint64_t pre_indication_tsc   = 0;
    uint64_t indication_intvl_tsc = 200;//unit is 200us
	char *data_start;

    while(force_quit == false) {
		if(snd_bb_ring_is_null(us_serial_number) == 1) 
		{  // NO data to Tx

			// wait next round Tx
			rte_atomic32_clear(&available_fpga_buff);
			continue;
		}
        #if 0
		uint32_t fpga_buff_byte = (uint32_t)rte_atomic32_read(&available_fpga_buff);
		if(fpga_buff_byte == 0) {
			//printf("available_fpga_buff is zero\n");
			continue;
		}
		else {
			rte_atomic32_clear(&available_fpga_buff);
		}
		#endif

		cur_indication_tsc  = get_current_time_us();
	    if(cur_indication_tsc-pre_indication_tsc < indication_intvl_tsc) {  // too close indication
	        //printf("use how long time:%d\n", cur_indication_tsc-pre_indication_tsc);
		    continue;
	    }
	    pre_indication_tsc = cur_indication_tsc;
		uint32_t has_send_bytes = 0;
        while(force_quit == false /*&& has_send_bytes < fpga_buff_byte && */&&get_current_time_us() < pre_indication_tsc + indication_intvl_tsc)
        {
			if(dequeue_snd_bb_ring(pkts_burst, 1, us_serial_number) == 0) 
			{	// deq failed
				continue;
			}

		    data_start = rte_pktmbuf_mtod(pkts_burst[0], char*);

		    snd_pkt(data_start, pkts_burst[0]->data_len,  g_st_app_conf.port_id[us_serial_number]);
			rte_pktmbuf_free(pkts_burst[0]); 
			has_send_bytes       += pkts_burst[0]->data_len;
        } 
    }

    return 0;
}
#endif



void * snd_bbframe_thread (void * pv_arg)
{ 

    THREAD_HANDLE* pst_handle = (THREAD_HANDLE*)pv_arg;
    uint16_t us_serial_number = pst_handle->pParam;


	printf("Hi, I'm thread snd_bbframe_thread,threadid:%d\n", us_serial_number);
	LOG(LOG_NOTICE, LOG_MODULE_DECAP, LOG_NOTICE,
	    LOG_CONTENT("Hi, I'm thread snd_bbframe_thread,threadid:%d"), us_serial_number);

	fc_bbframe_send_ignore_interval(us_serial_number);



	LOG(LOG_ERR, LOG_MODULE_DECAP, LOG_ERR,
	    LOG_CONTENT("snd_bbframe_thread is over"));

    return NULL;
}




