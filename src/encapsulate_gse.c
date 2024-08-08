
#include <stdio.h>
#include <sys/time.h>
#include "fragment.h"

#include "common_dpdk.h"
#include "encapsulate_gse.h"
#include "global_var.h"
#include "general_module.h"


#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wunused-value"

// MAC地址相同/不同
#define	MAC_SAME		1
#define	MAC_DIFFERENCE  0





/********************************************************************************
* 功能：将一个PDU的信息（PDU长度、PDU存储地址）添加到一个PDU信息组（一个PDU信息组可以封装到一个GSE包里）
* 参数：
 [in]	pNode_PDU	：从MODCOD队列里取出的PDU的信息
 [out]	pPDU_group	：PDU信息组存储区的指针
 [in]	label_type	: GSE包的label类型（6字节、3字节、广播、重用）
 [in]	label_size	: GSE包的label字节数
* 返回值：
		 0～成功	 POINT_ERROR～指针错误代码
 ********************************************************************************/
int8_t add_to_pdu_group(const EN_PDU_NODE *pPDU_Node, PDU_GROUP *pPDU_group, uint8_t label_type, uint8_t label_size)
{
	if (NULL == pPDU_Node || NULL == pPDU_group)
	{
		return POINT_ERROR;
	}

	pPDU_group->priority_group = pPDU_Node->priority_group;
	pPDU_group->pdu_num++;									// PDU组信息：PDU个数加1
	pPDU_group->fragment_flag = pPDU_Node->fragment_flag;
	pPDU_group->fragment_type = pPDU_Node->fragment_type;
	pPDU_group->fragment_ID = pPDU_Node->fragment_ID;
	pPDU_group->crc32 = pPDU_Node->crc32;

	if (1 == pPDU_group->pdu_num)		// 组中只有一个PDU时，要保存目的MAC地址和PDU的协议类型
	{
		// memcpy(pPDU_group->MAC_address, pPDU_Node->MAC_address, 6);
		rte_memcpy(pPDU_group->MAC_address, pPDU_Node->MAC_address, 6);
		pPDU_group->protocol_type = pPDU_Node->protocoltype;
		pPDU_group->PDU_total_length = pPDU_Node->pdu_size;	// PDU组总长度只增加PDU_size
		if(1==pPDU_group->fragment_flag)
		{
			pPDU_group->fragmented_PDU_length = pPDU_Node->pdu_total_length;
		}
		pPDU_group->label_type = label_type;
		pPDU_group->label_size = label_size;
		pPDU_group->modcod = pPDU_Node->MODCOD;

		// 在加入第一个PDU时，添加扩展头（目前先不加任何扩展头，通过参数设置）
		add_opt_extension_header(&(pPDU_group->opt_ext_header_info), NULL, pPDU_group->protocol_type);
		add_man_extension_header(&(pPDU_group->man_ext_header_info), NULL, pPDU_group->protocol_type);
	}
	else if (pPDU_group->pdu_num > 1)	// 组中PDU个数>1，说明有多个PDU进行串接，每个PDU的长度要+2字节长度字段
	{
		if (2 == pPDU_group->pdu_num)	// 当前PDU是第2个加入PDU组的PDU，第一个PDU的PDU_size补充+2
		{
			pPDU_group->PDU_total_length += pPDU_Node->pdu_size + 2 + 2;	// PDU组信息：若有N个PDU串接，则PDU长度=pdu_size+2
		}
		else
		{
			pPDU_group->PDU_total_length += pPDU_Node->pdu_size + 2;		// PDU组信息：若有N个PDU串接，则PDU长度=pdu_size+2
		}
	}

	if(pPDU_group->pdu_num <= PDU_MAX_NUM)
	{
		pPDU_group->one_PDU[pPDU_group->pdu_num - 1].mbuf_address = pPDU_Node->mbuf_address;
		pPDU_group->one_PDU[pPDU_group->pdu_num - 1].pStart = pPDU_Node->pPDU_start;	// PDU组信息：PDU存储地址
		pPDU_group->one_PDU[pPDU_group->pdu_num - 1].pdu_size = pPDU_Node->pdu_size;	// PDU组信息：PDU长度
		pPDU_group->one_PDU[pPDU_group->pdu_num - 1].priority = pPDU_Node->priority;
		pPDU_group->one_PDU[pPDU_group->pdu_num - 1].enq_time = pPDU_Node->enq_time;
		pPDU_group->one_PDU[pPDU_group->pdu_num - 1].deq_modcod_time = pPDU_Node->deq_modcod_time;
		pPDU_group->one_PDU[pPDU_group->pdu_num - 1].deq_time = pPDU_Node->deq_time;
		pPDU_group->one_PDU[pPDU_group->pdu_num - 1].read_time = pPDU_Node->read_time;
		pPDU_group->one_PDU[pPDU_group->pdu_num - 1].pPDU_address = pPDU_Node->pPDU_address;
		pPDU_group->one_PDU[pPDU_group->pdu_num - 1].vip_flag = pPDU_Node->vip_flag;
		pPDU_group->one_PDU[pPDU_group->pdu_num - 1].mobility = pPDU_Node->mobility;
	}
	return 0;
}


void reset_pdu_group(PDU_GROUP *pPDU_group)
{
	if(NULL != pPDU_group)
	{
		memset(pPDU_group, 0, sizeof(PDU_GROUP));
	}
}


void reset_gse_group(GSE_GROUP *pGSE_group)
{
	if(NULL != pGSE_group)
	{
		pGSE_group->priority_group = 0;
		pGSE_group->encode_info.modcod = 0;
		pGSE_group->gse_num = 0;
		pGSE_group->total_length = 0;
	}
}


/********************************************************************************
* 功能： 添加可选扩展头
* 参数：
		[in] pheader_info	： 可选扩展头的信息，该信息保存其后参数值
		[in] header_type	： 当前可选扩展头的类型
		[in] value_size		： 当前可选扩展头的值的长度字节
		[in] pheader_value	： 指向当前可选扩展头的值
		[in] next_type		： 下一个可选扩展头的类型
* 返回值：无
* 说明：
		next_type[N] 表示第(N+1)个扩展头的类型，而第(N+1)个扩展头的类型必须在加入的时候才能确定，
		即，在处理第(N)个扩展头时还无法确定next_type[N]的值
********************************************************************************/
int8_t add_opt_extension_header(OPT_EXT_HEADER_INFO *pheader_info, OPT_EXT_HEADER_INFO *src_header, uint16_t pdu_type)
{
	int8_t ret = 0, i;

	if (NULL == pheader_info || NULL == src_header)
		return POINT_ERROR;

	if ((pheader_info->header_num + src_header->header_num) > EXT_HEADER_MAX_NUM)	// 已封装的扩展头的个数+待封装的扩展头的个数 > 最大允许个数，不添加新的扩展头
		return -1;

	if (pheader_info->header_num > 0)
	{
		pheader_info->next_type[pheader_info->header_num - 1] = src_header->header_type[0];
	}

	for(i=0; i<src_header->header_num; ++i)
	{
		pheader_info->header_type[pheader_info->header_num] = src_header->header_type[i];
		pheader_info->header_size[pheader_info->header_num] = src_header->header_size[i] + 2;
		pheader_info->next_type[pheader_info->header_num] = src_header->next_type[i];
		rte_memcpy(pheader_info->header_value[pheader_info->header_num], src_header->header_value[i], src_header->header_size[i]);		
		pheader_info->header_num++;
	}
	pheader_info->next_type[pheader_info->header_num - 1] = rte_cpu_to_be_16(pdu_type);	// 最后一个扩展头的next_type必须是PDU的类型

	return ret = 1;
}


/********************************************************************************
* 功能： 添加强制扩展头
* 参数：
		[in] pheader_info	： 当前强制扩展头信息，该信息保存其后参数值
		[in] header_type	： 当前强制扩展头的类型
		[in] value_size		： 当前强制扩展头的值的长度字节
		[in] pheader_value	： 指向当前强制扩展头的值
		[in] next_type		： 当前强制扩展头next字段的值
		[in] value_address	： 为0-使用header_address指向的数据作为扩展头数据，非0-使用header_value保存的数据作为扩展头数据
* 返回值：无
* 说明： 
		next_type[N] 表示第(N+1)个扩展头的类型，而第(N+1)个扩展头的类型必须在加入的时候才能确定，
		即，在处理第(N)个扩展头时还无法确定next_type[N]的值；
		对于较短的扩展头数据，保存在header_value字段；
		对于较长的扩展头数据，将其数据指针保存在header_address字段，通过指针再去访问数据。
********************************************************************************/
int8_t add_man_extension_header(MAN_EXT_HEADER_INFO *pheader_info, MAN_EXT_HEADER_INFO *src_header, uint16_t pdu_type)
{
	int8_t ret = 0, i;

	if (NULL == pheader_info || NULL == src_header)
		return POINT_ERROR;

	if ((pheader_info->header_num + src_header->header_num) > EXT_HEADER_MAX_NUM)	// 已封装的扩展头的个数+待封装的扩展头的个数 > 最大允许个数，不添加新的扩展头
		return -1;

	if (pheader_info->header_num > 0)
	{
		pheader_info->next_type[pheader_info->header_num - 1] = src_header->header_type[0];
	}

	for(i=0; i<src_header->header_num; ++i)
	{
		pheader_info->header_type[pheader_info->header_num] = src_header->header_type[i];
		pheader_info->header_size[pheader_info->header_num] = src_header->header_size[i] + 2;
		pheader_info->next_type[pheader_info->header_num] = src_header->next_type[i];
		
		pheader_info->value_address[pheader_info->header_num] = src_header->value_address[i];
		if (src_header->value_address[i] && src_header->header_value[i] && src_header->header_size[i])
		{
			memcpy(pheader_info->header_value[pheader_info->header_num], src_header->header_value[i], src_header->header_size[i]);
		}
		else
		{
			pheader_info->header_address[pheader_info->header_num] = src_header->header_address[i];
		}		
		
		pheader_info->header_num++;
	}
	pheader_info->next_type[pheader_info->header_num - 1] = rte_cpu_to_be_16(pdu_type);	// 最后一个扩展头的next_type必须是PDU的类型

	return ret = 1;
}


/********************************************************************************
* 功能：将一个PDU添加到一个PDU GROUP，可得到扩展头长度/数据字段
* 参数：
 [in]	pNode_PDU：从MODCOD队列里取出的PDU的信息
 [out]	pPDU_group：PDU信息组存储区的指针
 [in]	label_type	: GSE包的label类型（6字节、3字节、广播、重用）
 [in]	label_size	: GSE包的label字节数
* 返回值：无
* 步骤说明：
	1. 把PDU信息加入到PDU_GROUP
	2. 添加扩展头到PDU_GROUP
	3. 计算PDU的封装类型和封装包长度
********************************************************************************/
void update_pdu_group(EN_PDU_NODE *pPDU_Node, PDU_GROUP *pPDU_group, uint8_t label_type, uint8_t label_size, uint16_t mapping_id)
{
	uint16_t gse_total_length;	// GSE包总长度
	uint16_t datafield_length;	// GSE包的数据字段长度
	uint16_t ext_header_length;	// GSE包的扩展头长度
	GSE_VARIABLE_HEADER_SIZE gse_var_header_size;	// 保存GSE报头可变部分各个字段的长度信息
	int8_t encapsulation_type;	// 封装类型
	static uint16_t frag_count[MAX_DEVICE_NUM][MAX_FRAGMENT_ID];
	static uint8_t  fragment_ID[MAX_DEVICE_NUM] = {0};//djc
	static uint8_t  first_enter_flag[MAX_DEVICE_NUM] = {0};
	uint16_t time_seq;



	if (NULL == pPDU_Node || NULL == pPDU_group)
		return;

	add_to_pdu_group(pPDU_Node, pPDU_group, label_type, label_size);	// 将从MODCOD队列取出的一个PDU添加到一个（空的或非空的）PDU_GROUP

	if (1 == pPDU_Node->fragment_flag)	// PDU的分片加上序列号扩展头
	{
		OPT_EXT_HEADER_INFO frag_sequence;
		//added by djc below
        if(0 == first_enter_flag[mapping_id])
	    {
            fragment_ID[mapping_id] = pPDU_Node->fragment_ID;
			first_enter_flag[mapping_id] = 1;
	    }
		
        if(fragment_ID[mapping_id] != pPDU_Node->fragment_ID)
        {
			fragment_ID[mapping_id] = pPDU_Node->fragment_ID;
			++frag_count[mapping_id][fragment_ID[mapping_id]];
        }
		if(frag_count[mapping_id][fragment_ID[mapping_id]] >= 8192)
		{
			frag_count[mapping_id][fragment_ID[mapping_id]] = 0;
		}
		//added by djc above
		frag_sequence.header_num = 1;
		frag_sequence.header_type[0] = FRAG_SEQUENCE;
		frag_sequence.header_size[0] = 2;
		time_seq = (frag_count[mapping_id][fragment_ID[mapping_id]] << 3) | pPDU_Node->fragment_seq;
		frag_sequence.next_type[0] = pPDU_group->protocol_type;

		if(little_endian)
		{
			frag_sequence.header_type[0] = rte_cpu_to_be_16(frag_sequence.header_type[0]);
			time_seq = rte_cpu_to_be_16(time_seq);
			frag_sequence.next_type[0] = rte_cpu_to_be_16(frag_sequence.next_type[0]);
		}
		rte_memcpy(frag_sequence.header_value[0], &time_seq, 2);

// printf("header value:%02X %02X next type:%04X\n", 
// 		frag_sequence.header_value[0][0], frag_sequence.header_value[0][1],	frag_sequence.next_type[0]);	
		
		add_opt_extension_header(&(pPDU_group->opt_ext_header_info), &frag_sequence, pPDU_group->protocol_type);

		
	
	}
	else if (2 == pPDU_group->pdu_num)
	{
		MAN_EXT_HEADER_INFO pdu_concat;
		pdu_concat.header_num = 1;
		pdu_concat.value_address[0] = 1;	// 用header_value的值作为扩展头的数据
		pdu_concat.header_type[0] = PDU_CONCAT;
		pdu_concat.header_size[0] = 0;
		pdu_concat.header_value[0][0] = 0;
		pdu_concat.header_address[0] = NULL;
		pdu_concat.next_type[0] = pPDU_group->protocol_type;

		if(little_endian)
		{
			pdu_concat.header_type[0] = rte_cpu_to_be_16(pdu_concat.header_type[0]);
			pdu_concat.header_value[0][0] = rte_cpu_to_be_16(pdu_concat.header_value[0][0]);
			pdu_concat.next_type[0] = rte_cpu_to_be_16(pdu_concat.next_type[0]);
		}

		add_man_extension_header(&(pPDU_group->man_ext_header_info), &pdu_concat, pPDU_group->protocol_type);
	}

	ext_header_length = calc_ext_header_length(pPDU_group);	// 确定扩展头长度
	datafield_length = calc_datafield_length(pPDU_group);	// 确定数据长度（包括扩展头）

	encapsulation_type = get_encapsulation_type(pPDU_group->pdu_num, pPDU_group->fragment_flag);	// 判断封装类型
	gse_var_header_size = calc_gse_variable_header_size(encapsulation_type, pPDU_Node->fragment_type, label_type, datafield_length);
	gse_total_length = sizeof(GSE_FIXED_HEADER) + calc_gse_length(gse_var_header_size, datafield_length);

	pPDU_group->gse_header_length = sizeof(GSE_FIXED_HEADER) + gse_var_header_size.fragment_ID_size + 
									gse_var_header_size.total_length_size + gse_var_header_size.protocol_type_size +
									gse_var_header_size.label_size;
	pPDU_group->ext_header_length = ext_header_length;
	pPDU_group->gse_data_length   = datafield_length;
	pPDU_group->gse_total_length  = gse_total_length;
}


inline void update_gse_group(FRAME_PAYLOAD *pframe_payload, ENCAP_PARAM *pEncap_param, uint8_t bResetSRc)
{
	if(pEncap_param && pframe_payload)
	{
		update_frame_payload(pframe_payload, pEncap_param->pPDU_Group->gse_total_length);
		encapsulate_gse(pEncap_param->pPDU_Group, pEncap_param->pGSE_Group, bResetSRc);
	}
}


inline void init_frame_payload(FRAME_PAYLOAD *ptr_frame_payload, uint8_t modcod)
{
	if(ptr_frame_payload)
	{
		ptr_frame_payload->max_payload = map_modcod_payload(modcod);
		ptr_frame_payload->used_payload = 0;
		ptr_frame_payload->usable_payload = ptr_frame_payload->max_payload;
		ptr_frame_payload->frame_modcod = modcod;
	}
}

inline void recalc_frame_payload(FRAME_PAYLOAD *ptr_frame_payload, uint8_t modcod)
{
	if(ptr_frame_payload)
	{
		ptr_frame_payload->max_payload = map_modcod_payload(modcod);
		ptr_frame_payload->usable_payload = ptr_frame_payload->max_payload - ptr_frame_payload->used_payload;
		ptr_frame_payload->frame_modcod = modcod;
	}
}


inline void update_frame_payload(FRAME_PAYLOAD *ptr_frame_payload, uint16_t delta_length)
{
	if(ptr_frame_payload)
	{
		ptr_frame_payload->usable_payload -= delta_length;
		ptr_frame_payload->used_payload += delta_length;
	}
}



/********************************************************************************
* 功能： 确定GSE头的label
* 参数：
		[in]	mac_length1    ：当前要封装的PDU的mac地址字节长度
		[in]	mac_address1[] ：当前要封装的PDU的mac地址
		[in]	mac_length2    ：当前已经保存最近一次使用的mac地址字节长度
		[in]	mac_address2[] ：当前已经保存最近一次使用的mac地址
		[in]	null_frame 	 ：当前frame是否已经填入了GSE包，1表示已经填入
		[out]	label_type	 ：GSE包的label类型（6字节、3字节、广播、重用）
* 返回值：
 		GSE头的label字段的长度
********************************************************************************/
uint8_t decide_label(uint8_t mac_length1, uint8_t mac_address1[], uint8_t mac_length2, uint8_t mac_address2[], uint8_t null_frame, uint8_t* label_type)
{
	uint8_t label_size = 0, same_mac;

	if(!label_type)
	{
		return 0;
	}

	uint8_t broadcast_type = get_broadcast_type(mac_address1);

	if(broadcast_type)	// 是广播地址，则label_type=10，没有label字段
	{
		*label_type = LABEL_TYPE_BROADCAST;
		label_size = 0;
	}
	else
	{
			// printf("null_frame:%d mac_address1=", null_frame);
			// for(int i=0; i<6; ++i)
			// {
			// 	printf("%02X", mac_address1[i]);
			// }
			// printf("\tmac_address2=");
			// for(int i=0; i<6; ++i)
			// {
			// 	printf("%02X", mac_address2[i]);
			// }
			// printf("\n");

		if(!null_frame && mac_length1==mac_length2)	// 该GSE包不是frame的第一个数据包，则label_type=10，没有label字段
		{
			same_mac = equal_mac(mac_address1, mac_address2, mac_length1);
			if(same_mac)
			{
				*label_type = LABEL_TYPE_REUSE;
				label_size = 0;
			}
			else
			{
				*label_type = (6==mac_length1) ? LABEL_TYPE_6B : LABEL_TYPE_3B;
				label_size = mac_length1;
			}
		}
		else
		{
			*label_type = (6==mac_length1) ? LABEL_TYPE_6B : LABEL_TYPE_3B;
			label_size = mac_length1;
		}
	}

	// if(label_size)
	// {
	// 	// memcpy(mac_address2, mac_address1, mac_length1);
	// 	rte_memcpy(mac_address2, mac_address1, mac_length1);
	// 	*mac_length2 = mac_length1;
	// }

	return label_size;
}


/********************************************************************************
* 功能： 更新GSE封装过程中，最近一次封装所使用的MAC地址（GSE报头中的label）
* 参数：
		[in]	mac_length1    ：当前要封装的PDU的mac地址字节长度
		[in]	mac_address1[] ：当前要封装的PDU的mac地址
		[in]	mac_length2    ：当前要保存的mac地址字节长度
		[in]	mac_address2[] ：当前要保存的mac地址
* 返回值：
		无
********************************************************************************/
void update_last_label(uint8_t mac_length1, uint8_t mac_address1[], uint8_t *mac_length2, uint8_t mac_address2[])
{
	// memcpy(mac_address2, mac_address1, mac_length1);
	rte_memcpy(mac_address2, mac_address1, mac_length1);
	*mac_length2 = mac_length1;
}


/********************************************************************************
* 功能： 封装GSE头的固定部分
* 参数：
 [in]	encapsulation_type：GSE包的封装类型（完整PDU、PDU分片）
 [in]	label_type：GSE包的label类型（6字节、3字节、重用）
 [in]	fragment_type：GSE包的分片类型（首个分片、中间分片、最后分片）
 [in]	data_length：GSE包的数据字段长度
 [out]	pvariable_header_size：GSE包头可变部分的长度
* 返回值：
 GSE头的固定部分的值
********************************************************************************/
GSE_FIXED_HEADER package_gse_fixed_header(int8_t encapsulation_type, uint8_t label_type, uint8_t fragment_type, uint16_t data_length, GSE_VARIABLE_HEADER_SIZE *pvariable_header_size)
{
	GSE_FIXED_HEADER GSE_fixed_header;
	memset(&GSE_fixed_header, 0, sizeof(GSE_FIXED_HEADER));

	if (NULL == pvariable_header_size)
	{
		return GSE_fixed_header;
	}

	GSE_fixed_header.Label_Type = label_type;

	switch (encapsulation_type)		// 封装类型决定 Start_Indicator、End_Indicator，分片类型决定Label
	{
		case ENCAPSULATION_TYPE_WHOLE:		// GSE包由一个完整的PDU组成
		case ENCAPSULATION_TYPE_CONCAT:		// GSE包由N个完整的PDU串接组成
		{
			GSE_fixed_header.Start_Indicator = 1;
			GSE_fixed_header.End_Indicator = 1;
			break;
		}
		case ENCAPSULATION_TYPE_FRAG:		// GSE包由一个完整的PDU的分片组成
		{
			switch (fragment_type)
			{
				case FRAGMENT_TYPE_S:		// PDU分片是Start-Packet
				{
					GSE_fixed_header.Start_Indicator = 1;
					GSE_fixed_header.End_Indicator = 0;
					break;
				}

				case FRAGMENT_TYPE_M:		// PDU分片是Middle-Packet
				{
					GSE_fixed_header.Start_Indicator = 0;
					GSE_fixed_header.End_Indicator = 0;
					break;
				}

				case FRAGMENT_TYPE_E:		// PDU分片是End-Packet
				{
					GSE_fixed_header.Start_Indicator = 0;
					GSE_fixed_header.End_Indicator = 1;
					break;
				}

				default:
					break;
			}
			break;
		}
		default:
			break;
	}

	*pvariable_header_size = calc_gse_variable_header_size(encapsulation_type, fragment_type, GSE_fixed_header.Label_Type, data_length);	// 计算得到GSE包的包头可变部分长度
	GSE_fixed_header.GSE_Length = calc_gse_length(*pvariable_header_size, data_length);	// 计算得到GSE包的总长度

	return GSE_fixed_header;
}

/********************************************************************************
* 功能： 计算GSE包的总长度
* 参数：
		[in]	encapsulation_type：GSE包的封装类型（完整PDU、PDU分片）
		[in]	fragment_type：GSE包的分片类型（首个分片、中间分片、最后分片）
		[in]	label_type：GSE包的label类型（6字节、3字节、重用）
		[in]	data_length：GSE包的数据字段长度
* 返回值：
 		GSE包头的可变部分长度
********************************************************************************/
GSE_VARIABLE_HEADER_SIZE calc_gse_variable_header_size(int8_t encapsulation_type, uint8_t fragment_type, uint16_t label_type, uint16_t data_length)
{
	GSE_VARIABLE_HEADER_SIZE variable_header_size = {0};
//	memset(&variable_header_size, 0, sizeof(GSE_VARIABLE_HEADER_SIZE));	// 把GSE头可变部分的各个字段长度清0

	// GSE的数据字段存在则Protocol_type字段存在，否则Protocol_type字段不存在
	if (data_length > 0)
	{
		variable_header_size.protocol_type_size = 2;
	}

	// 封装类型是分片，必有Fragment_ID字段
	if (ENCAPSULATION_TYPE_FRAG == encapsulation_type)
	{
		variable_header_size.fragment_ID_size = 1;

		if (FRAGMENT_TYPE_S == fragment_type)	// 只有分片是 Start-Packet 才有 total_length 字段
		{
			variable_header_size.total_length_size = 2;
		}
	}

	// 通过GSE头固定部分的label_type字段来确定label的字节数
	switch (label_type)
	{
		case LABEL_TYPE_6B:
			variable_header_size.label_size = 6;
			break;
		case LABEL_TYPE_REUSE:
		case LABEL_TYPE_BROADCAST:
			variable_header_size.label_size = 0;
			break;
		case LABEL_TYPE_3B:
			variable_header_size.label_size = 3;
			break;
		default:
			break;
	}

	return variable_header_size;
}

/********************************************************************************
* 功能： 计算GSE包的gse_length字段值
* 参数：
 [in]	pVariable_header_length：存储区指针，用于保存GSE头可变部分的各个字段长度
 [in]	data_length：GSE包的数据字段长度
* 返回值：
 GSE包的总长度
********************************************************************************/
uint16_t calc_gse_length(GSE_VARIABLE_HEADER_SIZE variable_header_size, uint16_t data_length)
{
	uint16_t gse_length = variable_header_size.protocol_type_size + variable_header_size.fragment_ID_size
						+ variable_header_size.total_length_size  + variable_header_size.label_size	+ data_length;
	return gse_length;
}


/********************************************************************************
* 功能：封装GSE头的可变部分
* 参数：
 [in]	variable_header_size：GSE头可变部分每一个字段的长度
 [in]	PDU_group：一个GSE包中含有的所有PDU的信息
 [out]	pvariable_header：保存GSE头可变部分实际值的存储区
* 返回值：
 0～成功		小于0～错误码
********************************************************************************/
int8_t package_gse_variable_header(GSE_VARIABLE_HEADER_SIZE variable_header_size, PDU_GROUP *pPDU_group, uint8_t *pvariable_header)
{
	uint8_t offset = 0;
	int i;
//	uint16_t ext_header_bytes = 0;	// 扩展头的总字节数

	if (NULL == pPDU_group || NULL == pvariable_header)
	{
		return POINT_ERROR;
	}

	// 查看有没有 fragment_ID 字段
	// fragment_ID字段的长度符合规定（1字节），赋值；否则，返回长度错误码
	if (variable_header_size.fragment_ID_size > 0)
	{
		if (1 == variable_header_size.fragment_ID_size)
		{
			*(pvariable_header + offset) = pPDU_group->fragment_ID;
			offset += variable_header_size.fragment_ID_size;
		}
		else
		{
			return SIZE_ERROR;
		}
	}

	// 查看有没有total_length字段
	// total_length字段的长度符合规定（2字节）；否则，返回长度错误码
	if (variable_header_size.total_length_size > 0)
	{
		if (2 == variable_header_size.total_length_size)
		{
			offset += variable_header_size.total_length_size;	// 此时还没有确定后面两个字段的大小，只能先空出buffer，稍后在填充数据
		}
		else
		{
			return SIZE_ERROR;
		}
	}

	// 查看有没有Protocol_type字段
	// Protocol_type字段的长度符合规定（2字节），赋值；否则，返回长度错误码
	if (variable_header_size.protocol_type_size > 0)
	{
		if (2 == variable_header_size.protocol_type_size)
		{
			if (pPDU_group->opt_ext_header_info.header_num > 0)			// 有optional扩展头，protocol_type字段是第一个扩展头的类型
			{
				*(uint16_t*)(pvariable_header + offset) = pPDU_group->opt_ext_header_info.header_type[0];
			}
			else if (pPDU_group->man_ext_header_info.header_num > 0)	// 没有optional扩展头，但有mandatory扩展头，protocol_type字段是第一个扩展头的类型
			{
				*(uint16_t*)(pvariable_header + offset) = pPDU_group->man_ext_header_info.header_type[0];
			}
			else
			{
				rte_memcpy(pvariable_header + offset, &(pPDU_group->protocol_type), variable_header_size.protocol_type_size);
				if(little_endian)
				{
					*((uint16_t*)(pvariable_header + offset)) = rte_cpu_to_be_16(*((uint16_t*)(pvariable_header + offset)));
				}
			}

			offset += variable_header_size.protocol_type_size;
		}
		else
		{
			return SIZE_ERROR;
		}
	}

	// 查看有没有label_size字段
	// label_size字段的长度符合规定（3或6字节），赋值；否则，返回长度错误码
	if (variable_header_size.label_size > 0)
	{
		if (3 == variable_header_size.label_size || 6 == variable_header_size.label_size)
		{
			rte_memcpy(pvariable_header + offset, pPDU_group->MAC_address, variable_header_size.label_size);
			offset += variable_header_size.label_size;	// offset增加后指向数据字段的首地址
		}
		else
		{
			return SIZE_ERROR;
		}
	}

	// 此时再确定 total_length 字段的值
	if (2 == variable_header_size.total_length_size)
	{
		// 统计扩展头的总字节数
//		for(i=0; i<pPDU_group->opt_ext_header_info.header_num; i++)
//		{
//			ext_header_bytes = pPDU_group->opt_ext_header_info.header_size[i];
//		}
//		for(i=0; i<pPDU_group->man_ext_header_info.header_num; i++)
//		{
//			ext_header_bytes = pPDU_group->man_ext_header_info.header_size[i];
//		}

		*((uint16_t*)(pvariable_header + variable_header_size.fragment_ID_size)) = variable_header_size.protocol_type_size + variable_header_size.label_size + pPDU_group->fragmented_PDU_length;

		if(pPDU_group->man_ext_header_info.header_num <= EXT_HEADER_MAX_NUM)
		{
			for (i = 0; i < pPDU_group->opt_ext_header_info.header_num; i++)
			{
				*((uint16_t*)(pvariable_header + variable_header_size.fragment_ID_size)) += pPDU_group->opt_ext_header_info.header_size[i];
			}
		}

		if(pPDU_group->man_ext_header_info.header_num <= EXT_HEADER_MAX_NUM)
		{
			for (i = 0; i < pPDU_group->man_ext_header_info.header_num; i++)
			{
				*((uint16_t*)(pvariable_header + variable_header_size.fragment_ID_size)) += pPDU_group->man_ext_header_info.header_size[i];
			}
		}

		if(little_endian)
		{
			*((uint16_t*)(pvariable_header + variable_header_size.fragment_ID_size)) = 
					rte_cpu_to_be_16(*((uint16_t*)(pvariable_header + variable_header_size.fragment_ID_size)));
		}
	}

	return 0;	// 成功
}


/********************************************************************************
* 功能：判断GSE的封装类型
* 参数：
 [in]	pdu_num：一个GSE包所含PDU的个数
 [in]	fragment_flag： PDU分片标志
* 返回值：
 GSE的封装类型
* 方法说明：
 PDU个数为1时，可能是一个完整PDU，也可能是PDU的一个分片；PDU个数>1时，不能进行分片，只能是PDU串接
********************************************************************************/
int8_t get_encapsulation_type(uint16_t pdu_num, uint8_t fragment_flag)
{
	int8_t encapsulation_type = GSE_PARAM_ERROR;

	// 由 pdu_num、fragment_flag、fragment_type 判断 encapsulation_type
	if (1 == pdu_num)		// PDU个数为1，可能是一个完整PDU，也可能是PDU的一个分片
	{
		if (1 == fragment_flag)		// 只有PDU个数=1才能进行分片
		{
			encapsulation_type = ENCAPSULATION_TYPE_FRAG;
		}
		else
		{
			encapsulation_type = ENCAPSULATION_TYPE_WHOLE;
		}
	}
	else if (pdu_num > 1)	// PDU个数>1
	{
		if (1 == fragment_flag)		// PDU个数>1时，不能进行分片
		{
			return GSE_PARAM_ERROR;
		}
		else
		{
			encapsulation_type = ENCAPSULATION_TYPE_CONCAT;
		}
	}

	return encapsulation_type;
}


/********************************************************************************
* 功能：判断GSE的是否是广播数据
* 参数：
 [in]	MAC_address：GSE要送达的目的地址
* 返回值：
 广播类型标识
* 方法说明：
 将MAC_address与广播地址进行比较
********************************************************************************/
uint8_t get_broadcast_type(const uint8_t MAC_address[])
{
	uint8_t i = 0;
	uint8_t broadcast_type = BROADCAST_TYPE_YES;

	for (i = 0; i < 6; i++)
	{
		if (BOARDCAST_ADDRESS[i] != MAC_address[i])
		{
			broadcast_type = BROADCAST_TYPE_NO;
			break;
		}
	}

	return broadcast_type;
}


/********************************************************************************
* 功能：计算GSE的数据的字节数（不含扩展头）
* 参数：
 [in]	PDU_group：组成一个GSE包所有的PDU的信息
* 返回值：
 GSE的数据字段的字节数
* 方法说明：
 根据PDU的个数，逐个PDU字节数+2进行累加；加入第一个PDU时不加2字节
********************************************************************************/
uint32_t calc_data_length(PDU_GROUP *pPDU_group)
{
	uint32_t data_length = 0;
	int i = 0;

	if (NULL == pPDU_group)
	{
		return 0;
	}

	if (1 == pPDU_group->pdu_num)
	{
		data_length = (pPDU_group->one_PDU[0]).pdu_size;

		// 分片PDU的end packet在计算总长度时，要加上crc校验码的长度
		if(pPDU_group->fragment_flag && FRAGMENT_TYPE_E==pPDU_group->fragment_type)
		{
			data_length += sizeof(pPDU_group->crc32);
		}
	}
	else
	{
		for (i = 0; i < pPDU_group->pdu_num; ++i)
		{
			data_length += (pPDU_group->one_PDU[i]).pdu_size + 2;
		}
	}

	return data_length;
}

/********************************************************************************
* 功能：计算GSE的扩展头字节数
* 参数：
 [in]	PDU_group：组成一个GSE包所有的PDU的信息
* 返回值：
 GSE的扩展头字节数
********************************************************************************/
uint32_t calc_ext_header_length(PDU_GROUP *pPDU_group)
{
	uint32_t data_length = 0;
	int i = 0;

	if (NULL == pPDU_group)
	{
		return 0;
	}

	if(pPDU_group->man_ext_header_info.header_num <= EXT_HEADER_MAX_NUM)
	{
		for (i = 0; i < pPDU_group->opt_ext_header_info.header_num; i++)
		{
			data_length += pPDU_group->opt_ext_header_info.header_size[i];
		}
	}

	if(pPDU_group->man_ext_header_info.header_num <= EXT_HEADER_MAX_NUM)
	{
		for (i = 0; i < pPDU_group->man_ext_header_info.header_num; i++)
		{
			data_length += pPDU_group->man_ext_header_info.header_size[i];
		}
	}

	return data_length;
}


/********************************************************************************
* 功能：计算GSE的数据字段的字节数
* 参数：
 [in]	PDU_group：组成一个GSE包所有的PDU的信息
* 返回值：
 GSE的数据字段的字节数
* 方法说明：
 根据PDU的个数，逐个PDU字节数+2进行累加；加入第一个PDU时不加2字节
********************************************************************************/
uint16_t calc_datafield_length(PDU_GROUP *pPDU_group)
{
	uint16_t data_length = 0;
	int i = 0;

	if (NULL == pPDU_group)
	{
		return 0;
	}

	if (1 == pPDU_group->pdu_num)
	{
		data_length = (pPDU_group->one_PDU[0]).pdu_size;

		// 分片PDU的end packet在计算总长度时，要加上crc校验码的长度
		if(pPDU_group->fragment_flag && FRAGMENT_TYPE_E==pPDU_group->fragment_type)
		{
			data_length += sizeof(pPDU_group->crc32);
		}
	}
	else
	{
		for (i = 0; i < pPDU_group->pdu_num && pPDU_group->pdu_num <= PDU_MAX_NUM; ++i)
		{
			data_length += (pPDU_group->one_PDU[i]).pdu_size + 2;
		}
	}

	if(pPDU_group->opt_ext_header_info.header_num <= EXT_HEADER_MAX_NUM)
	{
		for (i = 0; i < pPDU_group->opt_ext_header_info.header_num; i++)
		{
			data_length += pPDU_group->opt_ext_header_info.header_size[i];
		}
	}

	if(pPDU_group->man_ext_header_info.header_num <= EXT_HEADER_MAX_NUM)
	{
		for (i = 0; i < pPDU_group->man_ext_header_info.header_num; i++)
		{
			data_length += pPDU_group->man_ext_header_info.header_size[i];
		}
	}

	return data_length;
}

/********************************************************************************
* 功能：   GSE封装处理过程：把 READY_GSE 所包含的N个PDU组封装到一个GSE包里，并把这个GSE包添加到 GSE_GROUP
 ① 确定封装类型 encapsulation_type
 ② 判断GSE是否是通过广播发送
 ③ 计算GSE的数据字段长度
 ④ 调用 package_gse_fixed_header()    生成 GSE头固定部分
 ⑤ 调用 package_gse_variable_header() 生成 GSE头可变部分
 ⑥ 统计一个frame里包含了的GSE包的总长度
 ⑦ 把刚生成的一个GSE包的信息转存到 GSE_GROUP，准备供 frame_encapsulator 使用
 重复 ①～⑦ 直至GSE_num个GSE包封装完毕
* 参数：
 [in]		pReady_GSE	：准备好被封装成N个GSE包的N个PDU组
 [out]		pGSE_group	：由一系列完整的GSE数据包信息组成的集合
 [out]		bResetSrc	：对 READY_GSE 进行GSE封装完成后，是否重置READY_GSE的标志 非0～重置
* 返回值：	0～处理成功		小于0～错误代码
********************************************************************************/
int8_t encapsulate_gse(PDU_GROUP *pPDU_group, GSE_GROUP *pGSE_group, uint8_t bResetSrc)
{
	int8_t encapsulation_type;
	uint8_t label_type;
	uint16_t datafield_length;			// GSE的数据字段（含扩展头）的总长度
	GSE_FIXED_HEADER GSE_fixed_header;
	uint16_t tmp_gse_fixed_header;

	ONE_GSE one_GSE;
	GSE_VARIABLE_HEADER_SIZE variable_header_size;
	uint8_t variable_field_value[11];	// 用于保存GSE头可变部分各个字段的值，最长11个字节

#ifdef TEST_ENCAPSULATE_GSE
	uint32_t total_size = 0;
	ONE_GSE* ptemp = 0;
	int j;
#endif

	// 确认组成GSE包的PDU存在
	if (NULL == pPDU_group || NULL == pGSE_group || !pPDU_group->pdu_num)
	{
		return POINT_ERROR;
	}

	{
		pGSE_group->total_length += pPDU_group->gse_total_length;
		pGSE_group->priority_group = pPDU_group->priority_group;
		pGSE_group->gse_num++;			// gse group中的gse packet个数加1

		if(1==pGSE_group->gse_num)
		{
			pGSE_group->encode_info.modcod = pPDU_group->modcod;
			pGSE_group->frame_create_time = get_current_time();
		}
	}



	// 把这一个PDU_GROUP封装成一个GSE包，加入到sGSE_GROUP
	{
		encapsulation_type = get_encapsulation_type(pPDU_group->pdu_num, pPDU_group->fragment_flag);	// 判断封装类型
		label_type = pPDU_group->label_type; 					// 判断传播类型（单播、组播、广播）
		datafield_length = 0;
		datafield_length = calc_datafield_length(pPDU_group);	// 计算GSE的数据字段+扩展头的字节数

		// 确定GSE头固定部分的各个字段的值，同时计算出GSE头可变部分的字节数
		GSE_fixed_header = package_gse_fixed_header(encapsulation_type, label_type, pPDU_group->fragment_type, datafield_length, &variable_header_size);
		// 确定GSE头可变部分的各个字段的值
		package_gse_variable_header(variable_header_size, pPDU_group, variable_field_value);
		

		// 累加统计所有 GSE 包头长度的总和
//		gse_header_length = variable_header_size.fragment_ID_size
//							+ variable_header_size.total_length_size
//							+ variable_header_size.protocol_type_size
//							+ variable_header_size.label_size + sizeof(GSE_FIXED_HEADER);


		// 把刚生成的一个GSE包的信息 转存到 ONE_GSE

		// 赋值：一个GSE报文头固定部分的各个字段值
		rte_memcpy(one_GSE.GSE_header, &GSE_fixed_header, sizeof(GSE_FIXED_HEADER));
		if(little_endian)
		{
			*((uint16_t*)&one_GSE.GSE_header[0]) = rte_cpu_to_be_16(*((uint16_t*)&one_GSE.GSE_header[0]));
		}

		// 赋值：一个GSE报文头可变部分的各个字段值
		rte_memcpy(one_GSE.GSE_header + sizeof(GSE_FIXED_HEADER), variable_field_value, GSE_fixed_header.GSE_Length - datafield_length);

		// 确定GSE头的总长度
		one_GSE.GSE_header_size = sizeof(GSE_FIXED_HEADER) + GSE_fixed_header.GSE_Length - datafield_length;		
		one_GSE.pdu_num = pPDU_group->pdu_num;		// 一个GSE包里的PDU个数

		// 传递一个GSE包里所有PDU的信息
		rte_memcpy(one_GSE.one_PDU, pPDU_group->one_PDU, one_GSE.pdu_num * sizeof(ONE_PDU));

		// PDU的分片信息
		one_GSE.fragment_flag = pPDU_group->fragment_flag;
		one_GSE.fragment_type = pPDU_group->fragment_type;
		one_GSE.fragment_ID	  = pPDU_group->fragment_ID;
		one_GSE.crc32 = htonl(pPDU_group->crc32);
		
		
		// GSE扩展头信息
		rte_memcpy(&(one_GSE.opt_ext_header_info), &(pPDU_group->opt_ext_header_info), sizeof(OPT_EXT_HEADER_INFO));	// 直接传递一个GSE包里所有可选扩展头的信息
		rte_memcpy(&(one_GSE.man_ext_header_info), &(pPDU_group->man_ext_header_info), sizeof(MAN_EXT_HEADER_INFO));	// 直接传递一个GSE包里所有强制扩展头的信息

		if (one_GSE.opt_ext_header_info.header_num > 0 && one_GSE.man_ext_header_info.header_num > 0)
		{
			one_GSE.opt_ext_header_info.next_type[one_GSE.opt_ext_header_info.header_num - 1] = one_GSE.man_ext_header_info.header_type[0];
		}

		// 存入刚生成的GSE包信息，提供给frame封装器
		rte_memcpy(pGSE_group->pOneGSE + (pGSE_group->gse_num - 1), &one_GSE, sizeof(ONE_GSE));


	}

	if(bResetSrc)
	{
		reset_pdu_group(pPDU_group);	// 数据迁移完，重置PDU_GROUP
	}
	return 0;
}




