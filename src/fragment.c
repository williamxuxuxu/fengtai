
#include "fragment.h"
#include "define_const.h"
#include "circular_queue.h"
#include "common_dpdk.h"

#include "global_var.h"
#include "stats.h"

extern uint8_t get_broadcast_type(const uint8_t MAC_address[]);

/********************************************************************************
* 分片：是把一个PDU分别封装成N个GSE packet，而多个GSE packet可以合并放在一个frame
* 交互对象：调度器
* 调度器传递的参数：
* 判断是否需要分片
* 分片fragmentID
* 进行分片：确定分片的个数、每个分片的大小，将分片信息存入分片信息数组
* 通知调度器分片的结果
********************************************************************************/

/********************************************************************************
* 功能：初始化分片所形成的参数信息，在程序启动时执行
* 参数：
 [out]	pfragmentID_queue：记录fragmentID使用情况的环形缓冲区
* 返回值：
 无
********************************************************************************/
void init_fragment_info(CIRCULAR_QUEUE *pfragmentID_queue)
{
	InitCircleQueue(pfragmentID_queue, 256);
}

/********************************************************************************
* 功能：判断一个PDU是否需要分片
* 参数：
 [in]	bNewFrame：			  将要封装的frame是否是一个空frame
 [in]	pNode_PDU：			  PDU信息的指针
 [in]	frame_usable_payload：Frame的可用剩余长度(bits)
 [in]	frame_MODCOD：		  frame的MODCOD
* 返回值：
 一个PDU是否需要分片的结果
* 方法说明：
 要求PDU剩余长度 > frame剩余长度；新frame可以进行分片；非新frame需要PDU的MODCOD >= frame的MODCOD才可以进行分片
********************************************************************************/
uint8_t decide_fragment(uint8_t bNewFrame, EN_PDU_NODE *pNode_PDU, uint16_t frame_usable_payload, uint8_t frame_MODCOD)
{
	uint8_t ret = FALSE;
	uint8_t broadcast_type;
	uint8_t gse_header_length = 0;

	if (NULL == pNode_PDU)
		return FALSE;

	broadcast_type = get_broadcast_type(pNode_PDU->MAC_address);
	gse_header_length = (BROADCAST_TYPE_YES == broadcast_type) ? GSE_HEADER_LENGTH_INTEGRITY_PDU2 : GSE_HEADER_LENGTH_INTEGRITY_PDU1;

	if ((pNode_PDU->pdu_size + gse_header_length) > frame_usable_payload)	// PDU长度 > frame剩余长度
	{
		if (bNewFrame)
		{
			ret = TRUE;
		}
		else
		{
			ret = (pNode_PDU->MODCOD <= frame_MODCOD) ? TRUE : FALSE;

			if (ret && GSE_HEADER_LENGTH_FRAGMENT_PDU_S1 >= frame_usable_payload)  // 加上GSE报文头后，容纳不下第一个分片
			{
				ret = FALSE;
			}
		}
	}
	else
	{
		ret = FALSE;
	}

	return ret;
}

/********************************************************************************
* 功能：对一个PDU进行分片
 分片结果信息保存在fragment_buffer中
* 参数：
 [in]	bNewFrame			：将要封装的frame是否是一个空frame
 [in]	frame_max_payload	：PDU的MODCOD对应的frame净载荷长度
 [in]	frame_usable_payload：frame剩余长度(bits)
 [in]	pPDU_Node			：存储一个PDU的信息的存储变量指针
 [out]	fragment_buffer		：保存分片相关信息的数组
 [out]	pfragmentID_queue	：记录fragmentID使用情况的环形缓冲区
 [in]	CRC32				: 对pdu进行CRC校验得到的CRC32校验值			
* 返回值：
 分片结果	FragmentID～成功	 -1～分配fragmentID失败
* 方法说明：
 分配 fragmentID
 计算需要分成多少分片，每个分片的长度
 把分片信息存入fragment_buffer和pfragmentID_queue
********************************************************************************/
int16_t run_fragment(uint8_t bNewFrame, uint16_t frame_max_payload, uint16_t frame_usable_payload, EN_PDU_NODE *pPDU_Node,
					 FRAGMENT_BUFFER *pfragment_buffer, CIRCULAR_QUEUE *pfragmentID_queue, uint32_t CRC32, uint16_t mapping_id)
{
	uint32_t fragment_total_num = 0;	// PDU被分片的个数，包括 Start-packet、middle-Packet、End-Packet
	uint32_t PDU_remaining_size;		// PDU除掉Start-packet以后剩余的PDU长度，初始化为PDU长度

	static uint8_t last_FragmentID[MAX_DEVICE_NUM]={0};	
	uint8_t FragmentID;
	uint8_t bret;
	uint8_t broadcast_type;

	uint8_t gse_header_length1 = 0;		// start-packet 的 GSE 报文头长度
	uint8_t gse_header_length2 = GSE_HEADER_LENGTH_FRAGMENT_PDU_M1;	// middle-packet 和 end-packet 的 GSE 报文头长度

	// 分片包的长度（剔除掉GSE报文头长度）
	uint16_t start_size = 0;
	uint16_t middle_size = 0;
	uint16_t end_size = 0;

	uint16_t serial_ext_header_size = 4;		// 被分片的PDU要求加上4字节的分片序列号扩展头

	if (NULL == pPDU_Node)
	{
		return POINT_ERROR;
	}
	
	bret = allocate_fragmentID(&FragmentID, last_FragmentID[mapping_id], pfragment_buffer);	// 首先分配 fragmentID
	if (!bret || 0==pPDU_Node->pdu_size)
		return -1;
	last_FragmentID[mapping_id] = FragmentID;



	broadcast_type = get_broadcast_type(pPDU_Node->MAC_address);
	
	if(BROADCAST_TYPE_YES == broadcast_type) 
	{
		gse_header_length1 = GSE_HEADER_LENGTH_FRAGMENT_PDU_S2;
	 }
	 else if(3==pPDU_Node->mac_length)  // 3 byte
	 {
		gse_header_length1 =  GSE_HEADER_LENGTH_FRAGMENT_PDU_S3;
	 }
	 else   // 6 byte
	 {
		gse_header_length1 =  GSE_HEADER_LENGTH_FRAGMENT_PDU_S1;
	 }

	start_size = bNewFrame ? (frame_max_payload - gse_header_length1) : (frame_usable_payload - gse_header_length1);	// start-packet 中所含PDU字节数
	start_size -= serial_ext_header_size;
	PDU_remaining_size = pPDU_Node->pdu_size - start_size;

	// 计算中间包所含PDU字节数
	if (frame_max_payload <= GSE_MAX_SIZE)
	{
		middle_size = frame_max_payload - gse_header_length2 - serial_ext_header_size;		// frame的最大净载荷不超过GSE最大包长度，middle-packet封装在一个GSE包中
	}
	else
	{
		middle_size = frame_max_payload / 2 - gse_header_length2 - serial_ext_header_size;	// frame的最大净载荷超过GSE最大包长度，middle-packet封装在2个GSE包中，自定义2个包长度相等
	}

	end_size = PDU_remaining_size % middle_size;			// 计算最后一个分片的长度[0, middle_size)，包括crc校验码
	fragment_total_num = PDU_remaining_size / middle_size;	// 计算需要多少个完整的 frame

	if (end_size > 0)	// 最后一个分片的长度(0, frame_max_payload)，最后需要一个不满的frame
	{
		fragment_total_num++;
	}
	fragment_total_num++;

	ut_statis_debug.total_frag_num += fragment_total_num;
		

	// 把分片信息存入分片信息数组
	pfragment_buffer[FragmentID].used = 1;
	pfragment_buffer[FragmentID].fragment_info.FragmentID = FragmentID;
	pfragment_buffer[FragmentID].fragment_info.total_num = fragment_total_num;
	pfragment_buffer[FragmentID].fragment_info.transfered_num = 0;
	pfragment_buffer[FragmentID].fragment_info.middle_size = middle_size;
	pfragment_buffer[FragmentID].fragment_info.start_size = start_size;
	pfragment_buffer[FragmentID].fragment_info.end_size = end_size - sizeof(CRC32);		// 只记下PDU的末段字节数，CRC32校验码从CRC32字段读取
	pfragment_buffer[FragmentID].fragment_info.CRC32 = CRC32;

	// memcpy(&(pfragment_buffer[FragmentID].pdu_node), pPDU_Node, sizeof(PDU_NODE));
	rte_memcpy(&(pfragment_buffer[FragmentID].pdu_node), pPDU_Node, sizeof(EN_PDU_NODE));

	


	Push(pfragmentID_queue, FragmentID);	// 将 FragmentID 加到循环队列
	bret = IsEmpty(pfragmentID_queue);
	TraverseQueue(pfragmentID_queue);
	return FragmentID;
}


/********************************************************************************
* 功能：分配fragmentID
* 参数：
 [in]	pFragmentID：分配成功的ID值
 [out]	fragment_buffer：保存分片相关信息的数组
* 返回值：
 fragmentID分配是否成功标志
********************************************************************************/
uint8_t allocate_fragmentID(uint8_t *pFragmentID, uint8_t last_FragmentID, FRAGMENT_BUFFER fragment_buffer[])
{
	uint8_t bfindID = FALSE;
	int i = 0;

	for (i = last_FragmentID+1; i < FRAGMENT_MAX_NUM; i++)
	{
		if (0 == fragment_buffer[i].used)
		{
			*pFragmentID = i;
			bfindID = TRUE;
			break;
		}
	}

	if(!bfindID)
	{
		for (i = 0; i < last_FragmentID+1; i++)
		{
			if (0 == fragment_buffer[i].used)
			{
				*pFragmentID = i;
				bfindID = TRUE;
				break;
			}
		}
	}

	return bfindID;
}




/********************************************************************************
* 功能：进行分片的处理过程
 由调度器调用该函数
* 参数：
 [in]	bNewFrame：				将要封装的frame是否是一个空frame
 [in]	frame_max_payload：		PDU_MODCOD对应的frame净载荷长度
 [in]	frame_usable_payload：	frame可用长度(Bytes)
 [in]	pMODCOD_Node：			存储一个PDU的信息的存储变量指针
 [out]	fragment_buffer：		保存fragmentID等相关信息的数组
 [out]	pfragmentID_queue：		记录fragmentID使用情况的环形缓冲区
* 返回值：
 分片的fragmentID
 依靠fragmentID确定分片信息的位置
********************************************************************************/
int16_t do_fragment(uint8_t bNewFrame, uint16_t frame_max_payload, uint16_t frame_usable_payload,
					EN_PDU_NODE *pPDU_Node, FRAGMENT_BUFFER *pfragment_buffer,	CIRCULAR_QUEUE *pfragmentID_queue, uint16_t mapping_id)
{
	int16_t ret = -1;
	uint32_t CRC32;

	CRC32 = crc32_lookup_table(pPDU_Node->pPDU_start, pPDU_Node->pdu_size);
	pPDU_Node->pdu_size += sizeof(CRC32);	// 将CRC32校验码视作PDU的一部分进行处理
	ret = run_fragment(bNewFrame, frame_max_payload, frame_usable_payload, pPDU_Node, pfragment_buffer, pfragmentID_queue, CRC32, mapping_id);
	pPDU_Node->pdu_size -= sizeof(CRC32);	// 处理完后PDU size复原

	return ret;
}


/********************************************************************************
* 功能：获取gse封装的分片ID值
 由调度器调用该函数
* 参数：
 [in]	pfragmentID_queue：		记录fragmentID使用情况的环形缓冲区
* 返回值：
 分片的fragmentID
 依靠fragmentID确定分片信息的位置
********************************************************************************/
int16_t get_fragmentID(CIRCULAR_QUEUE *pfragmentID_queue)
{
	int16_t *pValue = (int16_t*)GetElement(pfragmentID_queue, 0);

	if (pValue)
		return (*pValue);
	else
		return -1;
}
