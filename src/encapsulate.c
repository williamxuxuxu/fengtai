#include <stdio.h>
#include <malloc.h>
#include <error.h>
#include <sys/ioctl.h>
#include <time.h>

#include "encapsulate.h"
#include "define_const.h"
#include "define_struct.h"
#include "general_module.h"
#include "pdu_header.h"
#include "sys_thread.h"
#include "global_var.h"
#include "stats.h"
#include "ether.h"
#include "system_log.h"
#include "circular_queue.h"
#include "fragment.h"
#include "encapsulate_frame.h"
#include "encapsulate_gse.h"
#include "ring_queue.h"
#include "router.h"
#include "rcv_send.h"
#include "tun.h"
//for test
#include "decapsulate.h"
#include "timer.h"
extern FRAG_INFO g_frag_info[MAX_DEVICE_NUM][MAX_FRAG_ID][FRAG_BUFFER_NUM];
//for test


#define GSE_MAX_PDU_NUM     1   // 一个GSE包中最多包含的PDU个数
#define RESET_PDU_GROUP     1
#define RESET_READY_GSE     1


extern FILE* test_file_record_1;
uint8_t BOARDCAST_ADDRESS[6];


uint64_t force_padding_count = 0;
extern uint64_t outroute_bytes[MAX_DEVICE_NUM];

extern volatile unsigned long long common_user_pkt_recv_num;

// 生成多项式 x32 + x26 + x23 + x22 + x16 + x12 + x11 + x10 + x8 + x7 + x5 + x4 + x2 + x + 1
unsigned int poly_32 = 0x04C11DB7;  // 1 0000 0100 1100 0001 0001 1101 1011 0111
// 生成多项式 X8+X7+X6+X4+X2+1
unsigned char poly_8 = 0x31;        // 1 1101 0101
FRAME_PAYLOAD frame_payload[ENCAP_Q_NUM];
struct rte_mempool * encap_pktmbuf_pool = NULL;
PDU_GROUP *g_pPDU_group[ENCAP_Q_NUM];		
// EXTERN READY_GSE *g_pReady_GSE;	
GSE_GROUP *g_pGSE_group[ENCAP_Q_NUM];	
ENCAP_INFO encap_info;

void init_encap_mem(void)
{
    int i;
    FRAME_PARAM* pFrameParam_do = NULL;

    crc8_init_table(poly_8);
    crc32_init_table(poly_32);

    memset(BOARDCAST_ADDRESS, 0xFF, sizeof(uint8_t) * 6);

    // 初始化分片参数
    for(i = 0; i < g_st_app_conf.encap_ring_queue_num; ++i) {
        encap_info.encap_param[i].pfragment_buffer = (FRAGMENT_BUFFER*) malloc(FRAGMENT_MAX_NUM * sizeof(FRAGMENT_BUFFER));
        memset(encap_info.encap_param[i].pfragment_buffer, 0, FRAGMENT_MAX_NUM * sizeof(FRAGMENT_BUFFER));
        init_fragment_info(&(encap_info.encap_param[i].fragmentID_queue));

        g_pPDU_group[i] = (PDU_GROUP*) malloc(sizeof(PDU_GROUP));
        memset(g_pPDU_group[i], 0, sizeof(PDU_GROUP));


        g_pGSE_group[i] = (GSE_GROUP*) malloc(sizeof(GSE_GROUP));
        memset(g_pGSE_group[i], 0, sizeof(GSE_GROUP));
        g_pGSE_group[i]->pOneGSE = (ONE_GSE*) malloc(GSE_MAX_NUM * sizeof(ONE_GSE));

        encap_info.encap_param[i].pPDU_Group = g_pPDU_group[i];
        // encap_info.encap_param.pReady_GSE = g_pReady_GSE;
        encap_info.encap_param[i].pGSE_Group = g_pGSE_group[i];

        encap_info.padding_info[i].used_size = 0;
        encap_info.padding_info[i].frame_size = 0;
        encap_info.padding_info[i].total_frame = 0;
        encap_info.padding_info[i].total_padding = 0;

        memset(&frame_payload[i], 0, sizeof(FRAME_PAYLOAD));

        pFrameParam_do = &encap_info.frame_param[i];
        init_BBHeader(&(pFrameParam_do->initialBBHeader), &(pFrameParam_do->L2L1_header));
        init_ISLHeader(&(pFrameParam_do->initialISLHeader), &(pFrameParam_do->L2L1_header));
    }
	/* create the mbuf pool */
    encap_pktmbuf_pool = rte_pktmbuf_pool_create("en_mbuf_pool", 128000U, 0, 0, 8192, 0);


}


struct rte_mbuf *get_encap_mbuf(void)
{
    struct rte_mbuf *mbuf = NULL;
    mbuf = rte_pktmbuf_alloc(encap_pktmbuf_pool);
    if(NULL == mbuf) {
        LOG(LOG_ERR, LOG_MODULE_RX, LOG_ERR,
            LOG_CONTENT("get mbuf failed from encap pool\n"));
		printf("get mbuf failed from encap pool\n");
    }
    return mbuf;
}


int8_t read_enapsulate_buffer(EN_PDU_NODE *pdu_node, struct rte_mbuf* mbuf)
{
	Mbuf_Private_Header * mph = (Mbuf_Private_Header*)rte_mbuf_to_priv(mbuf);
	

    pdu_node->pdu_size       = rte_pktmbuf_data_len(mbuf);
    pdu_node->pPDU_start     = rte_pktmbuf_mtod(mbuf, uint8_t*);
    pdu_node->mac_length     = 0;
    pdu_node->MODCOD         = g_st_app_conf.modcode;
    pdu_node->vip_flag       = 1;
    pdu_node->mobility       = 1;
    pdu_node->userID         = 0;
    pdu_node->priority       = 3;
    pdu_node->protocoltype   = mph->protocol_type;
    pdu_node->priority_group = 1;
    pdu_node->pPDU_address   = pdu_node->pPDU_start;
    pdu_node->mbuf_address   = mbuf;
    pdu_node->pdu_total_length = pdu_node->pdu_size;
    memset(pdu_node->MAC_address, 0xff, 6);

    return SUCCEED_FETCH_MODCOD;
}


uint8_t make_frame(ENCAP_PARAM *encap_param, FRAME_PAYLOAD *frame_payload, FRAME_PARAM *frame_param, uint16_t mapping_id)
{
    uint8_t null_frame = 0;
    //TBD init modecode
    int ret = 0;


    if(encap_param && frame_payload && frame_param) {
        ret = encapsulate_frame(*(encap_param->pGSE_Group), frame_payload->max_payload, frame_payload->used_payload, encap_param->pGSE_Group->encode_info, frame_param, mapping_id);
        if(ret < 0)
        {
            printf("make_frame failed\n");
		}
	    memset(frame_payload, 0, sizeof(FRAME_PAYLOAD));    // 发送完一帧frame后，将frame参数清零
        reset_gse_group(encap_param->pGSE_Group);
        null_frame = 1;
    }

    return null_frame;
}


int8_t force_send(ENCAP_PARAM* ptr_encap_param, FRAME_PAYLOAD *pframe_payload, FRAME_PARAM* pFrameParam, uint8_t insert_dummy, uint16_t mapping_id)
{

    if(!insert_dummy) { // 不插dummy frame，是读fast block的情况
        // if(time_diff >= alg_param.frame_timeout_us)  // 判断超时，强制发送未填满的BBFrame
        {
            if(ptr_encap_param->pGSE_Group->gse_num > 0) {
				   ut_statis_debug.force_send_num++;
                update_gse_group(pframe_payload, ptr_encap_param, RESET_PDU_GROUP);
                make_frame(ptr_encap_param, pframe_payload, pFrameParam, mapping_id);
                ++force_padding_count;
                // printf(">>>>>> make_frame pdu_src:%p\n", ptr_encap_param);
            }
        }
    }


    return 0;
}




uint8_t  last_mac_address[6] = {0};     // 封装过程中，最近一个显示使用的PDU目的MAC地址
uint8_t  last_mac_length = 0;           // 封装过程中，最近一个显示使用的PDU目的MAC地址的字节数

GSE_VARIABLE_HEADER_SIZE gse_var_header_size;   // 保存GSE报头可变部分各个字段的长度信息

uint8_t get_frag_pkt(uint16_t mapping_id)
{
    ENCAP_PARAM* ptr_encap_param_do = NULL;
    ptr_encap_param_do = &encap_info.encap_param[mapping_id]; // 读出了PDU，才fast的封装参数指针进行后续处理

    int16_t fragmentID = -1;

    memset(&(ptr_encap_param_do->using_pdu), 0, sizeof(EN_PDU_NODE));
    fragmentID = fetch_pdu_fragment(ptr_encap_param_do->pfragment_buffer, &(ptr_encap_param_do->fragmentID_queue), &(ptr_encap_param_do->using_pdu));
    if(fragmentID >= 0) {
        ut_statis_debug.encap_end_frag_num[mapping_id]++;
        return SUCCEED_FETCH_FRAGMENT;
    }

    return SUCCEED_FETCH_MODCOD;

}

uint8_t get_whole_pkt(struct rte_mbuf* mbuf, uint16_t mapping_id)
{
    int8_t  read_ok = -1;
    ENCAP_PARAM* ptr_encap_param_do = NULL;
    ptr_encap_param_do = &encap_info.encap_param[mapping_id]; // 读出了PDU，才fast的封装参数指针进行后续处理
    memset(&(ptr_encap_param_do->using_pdu), 0, sizeof(EN_PDU_NODE));

    read_ok = read_enapsulate_buffer(&(ptr_encap_param_do->using_pdu), mbuf);

    return read_ok;


}

void encapsulate_isl_bb(int8_t      read_ok, uint16_t mapping_id)
{

    ENCAP_PARAM* ptr_encap_param_do = NULL;
    FRAME_PARAM* pFrameParam_do = NULL;
    FRAME_PAYLOAD *ptr_frame_payload_do = NULL;


    ptr_encap_param_do = &encap_info.encap_param[mapping_id]; // 读出了PDU，才fast的封装参数指针进行后续处理
    ptr_frame_payload_do = &(frame_payload[mapping_id]);
    pFrameParam_do = &encap_info.frame_param[mapping_id];


    uint8_t label_type, label_size;      // label类型、label长度、label值
    uint8_t null_frame = 0;                         // frame是否载有有效payload标志

    if(SUCCEED_FETCH_MODCOD == read_ok || SUCCEED_FETCH_FRAGMENT == read_ok) {

        // 如果frame的max payload为0，表示这是一个未初始化（使用）的frame，要用当前PDU的modcod进行初始化
        if(0 == ptr_frame_payload_do->max_payload) {
            init_frame_payload(ptr_frame_payload_do, ptr_encap_param_do->using_pdu.MODCOD);
            null_frame = 1;
        } else {
            null_frame = 0;
        }
        // printf("read_ok:%d using_pdu.protocoltype:%d\n", read_ok, ptr_encap_param_do->using_pdu.protocoltype);

        // 确定当前读出的PDU的label（包括label type、label size、label alue）
        label_size = decide_label(ptr_encap_param_do->using_pdu.mac_length, ptr_encap_param_do->using_pdu.MAC_address,
                                  last_mac_length, last_mac_address,
                                  null_frame, &label_type);


        if(0 == ptr_encap_param_do->pGSE_Group->encode_info.modcod) {
            // frame采用modcod=0，更换为PDU的modcod
            ptr_encap_param_do->pGSE_Group->encode_info.modcod = ptr_encap_param_do->using_pdu.MODCOD;
        }

        update_pdu_group(&(ptr_encap_param_do->using_pdu), ptr_encap_param_do->pPDU_Group, label_type, label_size, mapping_id);

        // 1.1  frame的剩余容量足够容纳完整的PDU
        if(ptr_frame_payload_do->usable_payload >= ptr_encap_param_do->pPDU_Group->gse_total_length) {
	
            update_gse_group(ptr_frame_payload_do, ptr_encap_param_do, RESET_PDU_GROUP);
            // 在将一个GSE包加入到GSE组之后，才更新最近使用的label
            update_last_label(ptr_encap_param_do->using_pdu.mac_length, ptr_encap_param_do->using_pdu.MAC_address,
                              &last_mac_length, last_mac_address);

            // 1.1.1  加入当前PDU的GSE包后，frame剩余容量太小，直接封装GSE为BBFrame并发送
            if(ptr_frame_payload_do->usable_payload < g_st_app_conf.padding_threshold) {
                // 进行gse and frame封装，并把frame写入code block buffer。padding在函数中进行
                null_frame = make_frame(ptr_encap_param_do, ptr_frame_payload_do, pFrameParam_do, mapping_id);
                //TBD change modcode
            }
        }
        // 1.2  frame的剩余容量不能容纳完整的PDU
        else {
            // 1.2.1  frame剩余容量太小，先把已有的GSE数据发走，再用当前PDU（using_pdu）组建新的GSE包和frame
            if(ptr_frame_payload_do->usable_payload < g_st_app_conf.padding_threshold) {
                null_frame = make_frame(ptr_encap_param_do, ptr_frame_payload_do, pFrameParam_do, mapping_id);      // 进行gse and frame封装，并把frame写入code block buffer。padding在函数中进行
                init_frame_payload(ptr_frame_payload_do, ptr_encap_param_do->pPDU_Group->modcod);   // 把当前PDU（using_pdu）加入到READY_GSE，作为准备进行新的GSE封装的材料）

                // 重新计算label size
                label_size = decide_label(ptr_encap_param_do->using_pdu.mac_length, ptr_encap_param_do->using_pdu.MAC_address,
                                          last_mac_length, last_mac_address,
                                          null_frame, &label_type);
                // 这个GSE包是加入到GSE组的第一个包，必须更新最近使用的label
                update_last_label(ptr_encap_param_do->using_pdu.mac_length, ptr_encap_param_do->using_pdu.MAC_address,
                                  &last_mac_length, last_mac_address);
                // 添加扩展头（目前先不加任何扩展头，通过参数设置）
                // add_opt_extension_header(&(ptr_encap_param_do->pPDU_Group->opt_ext_header_info), 0, 0, NULL, 0);
                // add_man_extension_header(&(ptr_encap_param_do->pPDU_Group->man_ext_header_info), 0, 0, NULL, 0, 1);
                update_pdu_group(&(ptr_encap_param_do->using_pdu), ptr_encap_param_do->pPDU_Group, label_type, label_size, mapping_id);

                // 新建frame的可用payload能容纳当前PDU
                if(ptr_frame_payload_do->usable_payload >= ptr_encap_param_do->pPDU_Group->gse_total_length) {
                    update_gse_group(ptr_frame_payload_do, ptr_encap_param_do, RESET_PDU_GROUP);
                    null_frame = 0; // frame未填入GSE数据标志置0：表示frame已有数据
                } else {
                    // 进行分片
                    fragment_process(ptr_encap_param_do, ptr_frame_payload_do, pFrameParam_do, label_type, label_size, &null_frame, mapping_id);
                }
            }
            // 1.2.2  frame剩余容量允许using_pdu分片
            else {
                update_last_label(ptr_encap_param_do->using_pdu.mac_length, ptr_encap_param_do->using_pdu.MAC_address,
                                  &last_mac_length, last_mac_address);
                // 对当前PDU（using_pdu）进行分片处理，分片结果存入数组fragment_buffer，然后立即从fragment_buffer中取出其第一个分片包
                fragment_process(ptr_encap_param_do, ptr_frame_payload_do, pFrameParam_do, label_type, label_size, &null_frame, mapping_id);
            }
        }
    }
	else 
	{
        force_send(ptr_encap_param_do, &frame_payload[mapping_id], pFrameParam_do, 0, mapping_id);
        return;
    }

}


void fragment_process(ENCAP_PARAM* ptr_encap_param, FRAME_PAYLOAD* ptr_frame_payload, FRAME_PARAM* pFrameParam, uint8_t label_type, uint8_t label_size, uint8_t* ptr_null_frame, uint16_t mapping_id)
{
    int16_t frag_ID = -1;
    (void)frag_ID;

    if(!ptr_encap_param || !ptr_frame_payload || !pFrameParam || !ptr_null_frame) {
        return;
    }

    reset_pdu_group(ptr_encap_param->pPDU_Group);       // PDU的分片重新加入PDU_GROUP前，先重置PDU_GROUP
    // 进行分片
    do_fragment(*ptr_null_frame, ptr_frame_payload->max_payload, ptr_frame_payload->usable_payload, &(ptr_encap_param->using_pdu),
                ptr_encap_param->pfragment_buffer, &(ptr_encap_param->fragmentID_queue), mapping_id);
    // 读取第一个分片

    frag_ID = fetch_pdu_fragment(ptr_encap_param->pfragment_buffer, &(ptr_encap_param->fragmentID_queue), &(ptr_encap_param->using_pdu));
    if(frag_ID >= 0) {
        ut_statis_debug.encap_start_frag_num[mapping_id]++;
    }


// if(*(ptr_encap_param->using_pdu.pPDU_start) != 0x45)
// printf("data: %02X %02X %02X %02X %02X %02X\n",
//  *(ptr_encap_param->using_pdu.pPDU_start), *(ptr_encap_param->using_pdu.pPDU_start + 1),
//  *(ptr_encap_param->using_pdu.pPDU_start+2), *(ptr_encap_param->using_pdu.pPDU_start + 3),
//  *(ptr_encap_param->using_pdu.pPDU_start+4), *(ptr_encap_param->using_pdu.pPDU_start + 5),
//  *(ptr_encap_param->using_pdu.pPDU_start+6), *(ptr_encap_param->using_pdu.pPDU_start + 7));

    update_pdu_group(&(ptr_encap_param->using_pdu), ptr_encap_param->pPDU_Group, label_type, label_size, mapping_id);   // 将分片加入到PDU_GROUP
    update_gse_group(ptr_frame_payload, ptr_encap_param, RESET_PDU_GROUP);                                  // 将PDU_GROUP加入到GSE_GROUP

    *ptr_null_frame = make_frame(ptr_encap_param, ptr_frame_payload, pFrameParam, mapping_id);                      // 进行gse and frame封装，并把frame写入code block buffer。padding在函数中进行


}





uint8_t padding_rule(uint16_t frame_usable_payload, uint16_t padding_threshold)
{
    int8_t allow_padding = 0;

    if(frame_usable_payload < padding_threshold) {
        allow_padding = 1;
    }

    return allow_padding;
}


/********************************************************************************
* 功能：从PDU分片缓冲区中取PDU信息
* 参数：
         [in][out]  fragment_buffer：PDU分片缓冲区
         [in]       fragmentID_queue：存储fragmentID的循环队列
         [out]      pMODCOD_Node：指针，所代表的存储区用来保存一个PDU的信息
* 返回值：
         0～fragmentID     小于0～错误代码
********************************************************************************/
int16_t fetch_pdu_fragment(FRAGMENT_BUFFER *fragment_buffer, CIRCULAR_QUEUE *pfragmentID_queue, EN_PDU_NODE *pPDU_Node)
{
    int16_t ID = INVALID_FRAGMENT_ID_VALUE;

    uint16_t size = 0;          // 用来保存从PDU中读取数据的长度
    uint8_t *pFragment = NULL;  // 用来保存从PDU中读取数据的起始地址


    if (NULL == pPDU_Node) {
        printf("fetch_pdu_fragment:error mem\n");
        return POINT_ERROR;
    }

    ID = get_fragmentID(pfragmentID_queue);     // 从环形队列取出ID值


    if (INVALID_ID_VALUE(ID)) { // 判断环形队列队首存储的ID值是否合法
		LOG(LOG_ERR, LOG_MODULE_ENCAP, LOG_ERR,
            LOG_CONTENT("fetch_pdu_fragment:error ID"));
		
        return ERROR_FRAGMENT_ID_INVALID;
    }


    if (fragment_buffer[ID].used) { // ID合法，判断该ID是否已被标识为已分配使用
        // 已经传输的分片个数 <= 分片的总个数，就继续读取PDU分片
        if (fragment_buffer[ID].fragment_info.transfered_num == 0) {    // 要读取的是PDU的第一个分片
            size = fragment_buffer[ID].fragment_info.start_size;
            pPDU_Node->fragment_type = FRAGMENT_TYPE_S;
        } else if (fragment_buffer[ID].fragment_info.transfered_num == (fragment_buffer[ID].fragment_info.total_num - 1)) { // 要读取的是PDU的最后一个分片
            size = fragment_buffer[ID].fragment_info.end_size;
            pPDU_Node->fragment_type = FRAGMENT_TYPE_E;
        } else if (fragment_buffer[ID].fragment_info.transfered_num < (fragment_buffer[ID].fragment_info.total_num - 1)) {
            size = fragment_buffer[ID].fragment_info.middle_size;
            pPDU_Node->fragment_type = FRAGMENT_TYPE_M;
        } else {
            printf("fetch_pdu_fragment:error frag size\n");
            return ERROR_FRAGMENT_SIZE;
        }

        // 找到PDU在物理缓冲区里的需要读取的起始位置
        if(fragment_buffer[ID].fragment_info.transfered_num <= 1) {
            pFragment = fragment_buffer[ID].pdu_node.pPDU_start + fragment_buffer[ID].fragment_info.start_size * fragment_buffer[ID].fragment_info.transfered_num;
        } else {
            pFragment = fragment_buffer[ID].pdu_node.pPDU_start + fragment_buffer[ID].fragment_info.start_size + fragment_buffer[ID].fragment_info.middle_size * (fragment_buffer[ID].fragment_info.transfered_num - 1);
        }

        pPDU_Node->pdu_size = size;
        pPDU_Node->pPDU_start = pFragment;
        pPDU_Node->mac_length = fragment_buffer[ID].pdu_node.mac_length;
        rte_memcpy(pPDU_Node->MAC_address, fragment_buffer[ID].pdu_node.MAC_address, 6);// memcpy(pPDU_Node->MAC_address, fragment_buffer[ID].pdu_node.MAC_address, 6);
        pPDU_Node->MODCOD = fragment_buffer[ID].pdu_node.MODCOD;
        pPDU_Node->vip_flag = fragment_buffer[ID].pdu_node.vip_flag;
        pPDU_Node->mobility = fragment_buffer[ID].pdu_node.mobility;
        pPDU_Node->priority = fragment_buffer[ID].pdu_node.priority;
        pPDU_Node->priority_group = fragment_buffer[ID].pdu_node.priority_group;
        pPDU_Node->pPDU_address = fragment_buffer[ID].pdu_node.pPDU_start;
        pPDU_Node->mbuf_address = fragment_buffer[ID].pdu_node.mbuf_address;

        pPDU_Node->enq_time = fragment_buffer[ID].pdu_node.enq_time;

        pPDU_Node->fragment_flag = 1;
        pPDU_Node->fragment_ID = ID;
        pPDU_Node->fragment_seq = fragment_buffer[ID].fragment_info.transfered_num;
        pPDU_Node->pdu_total_length = fragment_buffer[ID].pdu_node.pdu_total_length;
        pPDU_Node->crc32 = fragment_buffer[ID].fragment_info.CRC32;

        // rte_memcpy(pPDU_Node, &(fragment_buffer[ID].pdu_node), sizeof(PDU_NODE));

        fragment_buffer[ID].fragment_info.transfered_num++; // 读取该分片时，将已经传输的分片数目+1

        if (fragment_buffer[ID].fragment_info.transfered_num >= fragment_buffer[ID].fragment_info.total_num) { // 这次读取的是end-packet
            fragment_buffer[ID].used = 0;           // 标识这个ID处于待分配状态
            SetElement(pfragmentID_queue, 0, -1);   // 在从循环队列队首删除之前，把ID值置为-1无效值
            Pop(pfragmentID_queue);                 // 从环形队列里把该ID剔除
        }

        return ID;
    } else {
        return ERROR_FRAGMENT_ID_USED;
    }
}


uint8_t get_min_avail_modcod(uint8_t modcod, uint8_t taget_modcod, uint32_t gse_total_length)
{
    uint8_t min_modcod = modcod;
    uint16_t frame_length = map_modcod_payload(modcod);
    int32_t padding_bytes = (int32_t)frame_length - (int32_t)gse_total_length;

    while(padding_bytes > 0 && modcod > taget_modcod) {
        min_modcod = modcod;
        --modcod;
        frame_length = map_modcod_payload(modcod);
        padding_bytes = frame_length - gse_total_length;
        if(padding_bytes <= 0)
            break;
    }

    return min_modcod;
}





void calc_delay_metric_time(uint64_t inrouter_time, uint64_t outrouter_time, uint8_t mapping_id)
{
	uint64_t diff_time_us;

	diff_time_us = outrouter_time - inrouter_time;
	static uint64_t calc_num[MAX_DEVICE_NUM] = {0};
    calc_num[mapping_id]++;
	if((0 == ut_statis_debug.max_delay_metric[mapping_id])
		&&(0 != diff_time_us))
	{
        ut_statis_debug.min_delay_metric[mapping_id] = diff_time_us;
		ut_statis_debug.avg_delay_metric[mapping_id] = diff_time_us;
		ut_statis_debug.max_delay_metric[mapping_id] = diff_time_us;
		ut_statis_debug.total_delay_metric[mapping_id] = diff_time_us;
	}
	else if(0 != diff_time_us)
	{
        if(diff_time_us < ut_statis_debug.min_delay_metric[mapping_id])
        {
            ut_statis_debug.min_delay_metric[mapping_id] = diff_time_us;
		}
		
		if(diff_time_us > ut_statis_debug.max_delay_metric[mapping_id])
        {
            ut_statis_debug.max_delay_metric[mapping_id] = diff_time_us;
		}

		ut_statis_debug.total_delay_metric[mapping_id] += diff_time_us;
		ut_statis_debug.avg_delay_metric[mapping_id] = ut_statis_debug.total_delay_metric[mapping_id]/calc_num[mapping_id];
	}
	
}


__thread uint8_t pdu_buffer[MAX_THREAD_NUM][PDU_BUFFER_SIZE];
__thread uint16_t pdu_buffer_offset[MAX_THREAD_NUM] = {0};



void encapsulate_pdu(struct rte_mbuf* mbuf, uint8_t mapping_id)
{
    FRAME_PAYLOAD *ptr_frame_payload_do = NULL;
    uint16_t data_len = 0;
	uint16_t data_len_tmp = 0;
    uint16_t frame_size = 0;//LEN + ROUTER_HEADER + PDU.......
	char *send_buffer;
    L2L1_MSG_HEADER inner_msg_header;
	uint16_t router_table_id;
	ROUTERHEADER *router_header;
	ROUTERHEADER *router_header_tmp;
    static uint64_t pre_seq_num = 0;
    ptr_frame_payload_do = &(frame_payload[mapping_id]);

    if(NULL != mbuf)
    {
		#if 0
		uint64_t seq_num;
    	uint64_t unique_num;
		send_buffer = rte_pktmbuf_mtod(mbuf, char *);
        send_buffer = send_buffer + 18 + 28;
	    memcpy(&seq_num, send_buffer, sizeof(uint32_t));
        seq_num = ntohl(seq_num);

		if(pre_seq_num == 0)
		{
            pre_seq_num = seq_num;
		}
		else
		{
            if((seq_num - pre_seq_num) > 1)
            {
                if((seq_num != 180879362)&&(seq_num != 180944898))
                {
                    debug_log(1, test_file_record_1, "%lu\n", seq_num);
			    }
			}
		}
		
		if((seq_num != 180879362)&&(seq_num != 180944898))
        {
		    pre_seq_num = seq_num;
		}

		send_buffer = send_buffer + 12;
	    memcpy(&unique_num, send_buffer, sizeof(uint16_t));
        unique_num = ntohs(unique_num);

		if(unique_num == 12851)
		{
            ut_statis_debug.fwd_pdu_num++;
			
		}
		
        
		#endif
        router_header_tmp = rte_pktmbuf_mtod(mbuf, ROUTERHEADER *);
        data_len = mbuf->data_len;
	    if(data_len >= PDU_BUFFER_SIZE)
	    {
			LOG(LOG_ERR, LOG_MODULE_ENCAP, LOG_ERR,
                LOG_CONTENT("buffer size less than pkt len"));
	        rte_pktmbuf_free(mbuf);
		    return;
        }

		if(DIRECTION_RTN == router_header_tmp->D)
		{
            router_table_id = decide_router_table(mbuf);
		
		    router_header = rte_pktmbuf_mtod(mbuf, ROUTERHEADER *);

           // uint16_t router_header_segment_size = sizeof(uint16_t)*(router_header->LAST_ENTRY + 1);
	        uint16_t router_header_size = ROUTER_HEADER_LEN + sizeof(uint16_t)*(router_header->LAST_ENTRY + 1);
	        rte_pktmbuf_adj(mbuf, router_header_size);
		}
    }



    // 如果frame的max payload为0，表示这是一个未初始化（使用）的frame，要用当前PDU的modcod进行初始化
    if(0 == ptr_frame_payload_do->max_payload) {
		ptr_frame_payload_do->max_payload = PDU_BUFFER_SIZE - sizeof(L2L1_MSG_HEADER);
        ptr_frame_payload_do->used_payload = 0;
        ptr_frame_payload_do->usable_payload = PDU_BUFFER_SIZE - sizeof(L2L1_MSG_HEADER);
    }


	
	//LEN + ROUTER_HEADER + PDU 
    if((ptr_frame_payload_do->usable_payload > data_len + g_st_app_conf.router_header_size[router_table_id]) 
		&&(0 != data_len))// data_len != 0 is for forcing send
	{
	    if(DIRECTION_RTN == router_header_tmp->D)
	    {
            data_len += g_st_app_conf.router_header_size[router_table_id];
	    }    
		
		data_len_tmp = htons(data_len);
		memcpy(pdu_buffer[mapping_id] + pdu_buffer_offset[mapping_id], &data_len_tmp, 2);
		pdu_buffer_offset[mapping_id] += 2;	
		
	    if(DIRECTION_RTN == router_header_tmp->D)
	    {
            memcpy(pdu_buffer[mapping_id] + pdu_buffer_offset[mapping_id], &router_header[router_table_id], g_st_app_conf.router_header_size[router_table_id]);
		    pdu_buffer_offset[mapping_id] += g_st_app_conf.router_header_size[router_table_id];
	    } 
	        
	    	
		uint8_t *src = rte_pktmbuf_mtod(mbuf, uint8_t *);
        memcpy(pdu_buffer[mapping_id] + pdu_buffer_offset[mapping_id], src, mbuf->data_len);	
		
	    rte_pktmbuf_free(mbuf);      		
        pdu_buffer_offset[mapping_id] += mbuf->data_len;

        data_len += 2;

        ut_statis_debug.snd_mpe_pdu_num[mapping_id]++;
        update_frame_payload(ptr_frame_payload_do, data_len);

        uint16_t len = PDU_BUFFER_SIZE - sizeof(L2L1_MSG_HEADER) - pdu_buffer_offset[mapping_id] ;
		if(len != ptr_frame_payload_do->usable_payload)
		{
        	LOG(LOG_ERR, LOG_MODULE_ENCAP, LOG_ERR,
                LOG_CONTENT("encapsulate_pdu:ecap pdu len error 1"));
		}
    } 
	else 
	{

        if(0 != pdu_buffer_offset[mapping_id])
        {
            frame_size = pdu_buffer_offset[mapping_id]; //PDU
   
            encode_inner_header(&inner_msg_header, frame_size, mapping_id, g_st_app_conf.port_id[mapping_id]);
		
            struct rte_mbuf* send_mbuf = get_encap_mbuf();
            if(NULL != send_mbuf)
            {
 
        	    send_buffer = (char *)rte_pktmbuf_append(send_mbuf, pdu_buffer_offset[mapping_id] + sizeof(L2L1_MSG_HEADER));	
	
			    rte_memcpy(send_buffer, &inner_msg_header, sizeof(L2L1_MSG_HEADER));
	
	            rte_memcpy(send_buffer + sizeof(L2L1_MSG_HEADER), pdu_buffer[mapping_id], pdu_buffer_offset[mapping_id]);
		  
			    snd_pkt(send_buffer, pdu_buffer_offset[mapping_id] + sizeof(L2L1_MSG_HEADER), g_st_app_conf.port_id[mapping_id]);
			    outroute_bytes[mapping_id] += send_mbuf->data_len;
				ut_statis_debug.snd_mpe_data_num[mapping_id]++;
				rte_pktmbuf_free(send_mbuf);
			
            }
		    else
		    {
                printf("send_mbuf allocated failed\n");
		    }
		
            memset(pdu_buffer[mapping_id], 0, PDU_BUFFER_SIZE);
		    ptr_frame_payload_do->max_payload = PDU_BUFFER_SIZE - sizeof(L2L1_MSG_HEADER);
            ptr_frame_payload_do->used_payload = 0;
            ptr_frame_payload_do->usable_payload = PDU_BUFFER_SIZE - sizeof(L2L1_MSG_HEADER);
            pdu_buffer_offset[mapping_id] = 0;
			
			if(NULL != mbuf)
			{

			    if(DIRECTION_RTN == router_header_tmp->D)
			    {
			        data_len += g_st_app_conf.router_header_size[router_table_id];
			    }
				
				data_len_tmp = htons(data_len);
		        memcpy(pdu_buffer[mapping_id] + pdu_buffer_offset[mapping_id], &data_len_tmp, 2);
		
		        pdu_buffer_offset[mapping_id] += 2;	
	            if(DIRECTION_RTN == router_header_tmp->D)
			    {
	                memcpy(pdu_buffer[mapping_id] + pdu_buffer_offset[mapping_id], &router_header[router_table_id], g_st_app_conf.router_header_size[router_table_id]);
		            pdu_buffer_offset[mapping_id] += g_st_app_conf.router_header_size[router_table_id];
	            }
				
		        uint8_t *src = rte_pktmbuf_mtod(mbuf, uint8_t *);
                memcpy(pdu_buffer[mapping_id] + pdu_buffer_offset[mapping_id], src, mbuf->data_len);	
              		
                pdu_buffer_offset[mapping_id] += mbuf->data_len;
			    data_len += 2;
		        rte_pktmbuf_free(mbuf); 
			}
		   
           

            ut_statis_debug.snd_mpe_pdu_num[mapping_id]++;
            update_frame_payload(ptr_frame_payload_do, data_len);
	    	uint16_t len = PDU_BUFFER_SIZE - sizeof(L2L1_MSG_HEADER) - pdu_buffer_offset[mapping_id] ;
	    	if(len != ptr_frame_payload_do->usable_payload)
	    	{
                LOG(LOG_ERR, LOG_MODULE_ENCAP, LOG_ERR,
                    LOG_CONTENT("encapsulate_pdu:ecap pdu len error 2"));
	    	}
        }

    }
}



#ifdef TAG_ROUTE
void snd_single_pdu(struct rte_mbuf* mbuf, uint8_t mapping_id)
{
    //FRAME_PAYLOAD *ptr_frame_payload_do = NULL;
    uint16_t data_len = 0;
   // uint16_t frame_size = 0;//LEN + ROUTER_HEADER + PDU.......
	char *send_buffer;
    L2L1_MSG_HEADER inner_msg_header;
	uint16_t router_table_id;
    //uint16_t pdu_buffer_offset = 0;
	ROUTERHEADER *router_header_tmp;
	//uint8_t direction;
	uint16_t msg_len;

	//for test
	static uint64_t pre_seq_num = 0;
	//static uint64_t count = 0;
	//char *seq_num_addr;
	uint64_t seq_num;
	uint64_t unique_num;
	#if 0
	seq_num_addr = rte_pktmbuf_mtod(mbuf, uint8_t *);
    seq_num_addr = seq_num_addr + 18 + 28;
	memcpy(&seq_num, seq_num_addr, sizeof(uint32_t));
    seq_num = ntohl(seq_num);
    debug_log(1, test_file_record_1, "%d\n", seq_num);
    #endif

	//for test
    
    if(NULL != mbuf)
    {
        router_header_tmp = rte_pktmbuf_mtod(mbuf, ROUTERHEADER *);
        data_len = mbuf->data_len;
	    if(data_len >= PDU_BUFFER_SIZE)
	    {
			LOG(LOG_ERR, LOG_MODULE_ENCAP, LOG_ERR,
                LOG_CONTENT("buffer size less than pkt len"));
	        rte_pktmbuf_free(mbuf);
		    return;
        }
    }
	else
	{
		return;
	}

	struct rte_mbuf* send_mbuf = get_encap_mbuf(); 
	if(NULL != send_mbuf)
	{
		uint16_t router_header_size = ROUTER_HEADER_LEN + sizeof(uint16_t)*(router_header_tmp->LAST_ENTRY + 1);
		uint16_t last_id = ntohs(router_header_tmp->segment[router_header_tmp->LAST_ENTRY]);
        if(last_id == g_st_app_conf.local_id)
        {
        	struct rte_ether_hdr * ether_hdr = rte_pktmbuf_mtod(send_mbuf, struct rte_ether_hdr *);
	    	rte_pktmbuf_adj(mbuf, router_header_size);
			msg_len = mbuf->data_len + sizeof(struct rte_ether_hdr);
			memcpy(&ether_hdr->d_addr, g_st_app_conf.dst_mac[mapping_id], 6);
            memcpy(&ether_hdr->s_addr, g_st_app_conf.local_mac[g_st_app_conf.port_id[mapping_id]], 6);
			ether_hdr->ether_type = rte_cpu_to_be_16(g_st_app_conf.protocol_type[mapping_id]);
			//ether_hdr->ether_type = htons(ether_hdr->ether_type);
            send_buffer = (char *)rte_pktmbuf_append(send_mbuf, msg_len);
			rte_memcpy(send_buffer+sizeof(struct rte_ether_hdr), rte_pktmbuf_mtod(mbuf, uint8_t *), msg_len);
		}
		else
		{
		
	        //if(DIRECTION_FWD == router_header_tmp->D)
	        {
	            encode_inner_header(&inner_msg_header, data_len, mapping_id, g_st_app_conf.port_id[mapping_id]);
	            msg_len = ntohs(inner_msg_header.msg_len) + sizeof(MAC_FRAME_HEADER); 
				send_buffer = (char *)rte_pktmbuf_append(send_mbuf, msg_len);
				rte_memcpy(send_buffer, &inner_msg_header, sizeof(L2L1_MSG_HEADER));
				rte_memcpy(send_buffer + sizeof(L2L1_MSG_HEADER), rte_pktmbuf_mtod(mbuf, uint8_t *), data_len);
			}
			/*else
			{
			    router_table_id = decide_router_table(mbuf);
				router_header[router_table_id].S = router_header_tmp->S;
				router_header[router_table_id].T = router_header_tmp->T;
				router_header[router_table_id].CUR_ENTRY = router_header_tmp->CUR_ENTRY;
				rte_pktmbuf_adj(mbuf, router_header_size);
	            msg_len = g_st_app_conf.router_header_size[router_table_id] + mbuf->data_len;
				encode_inner_header(&inner_msg_header, msg_len, mapping_id, g_st_app_conf.port_id[mapping_id]);
				msg_len = ntohs(inner_msg_header.msg_len) + sizeof(MAC_FRAME_HEADER); 
				send_buffer = (char *)rte_pktmbuf_append(send_mbuf, msg_len);
				rte_memcpy(send_buffer, &inner_msg_header, sizeof(L2L1_MSG_HEADER));
			    rte_memcpy(send_buffer + sizeof(L2L1_MSG_HEADER), &router_header[router_table_id], g_st_app_conf.router_header_size[router_table_id]);
	            rte_memcpy(send_buffer + sizeof(L2L1_MSG_HEADER) + g_st_app_conf.router_header_size[router_table_id], rte_pktmbuf_mtod(mbuf, uint8_t *), mbuf->data_len);
			}*/
		}
		
		snd_pkt(send_buffer, msg_len,  g_st_app_conf.port_id[mapping_id]);
		outroute_bytes[mapping_id] += msg_len;
		#if 1
        send_buffer = send_buffer + 18 + 28 + 24;
	    memcpy(&seq_num, send_buffer, sizeof(uint32_t));
        seq_num = ntohl(seq_num);

		if(pre_seq_num == 0)
		{
            pre_seq_num = seq_num;
		}
		else
		{
            if((seq_num - pre_seq_num) > 1)
            {
                if(seq_num != 180879362)
                {
                    debug_log(1, test_file_record_1, "%lu\n", seq_num);
			    }
			}
		}
		
		if(seq_num != 180879362)
        {
		    pre_seq_num = seq_num;
		}

		send_buffer = send_buffer + 12;
	    memcpy(&unique_num, send_buffer, sizeof(uint16_t));
        unique_num = ntohs(unique_num);

		if(unique_num == 12851)
		{
            ut_statis_debug.fwd_pdu_num++;
			
		}
		
        
		#endif
		rte_pktmbuf_free(send_mbuf);
	}
	else
	{
	    LOG(LOG_ERR, LOG_MODULE_ENCAP, LOG_ERR,
            LOG_CONTENT("send_mbuf allocated failed"));
	}
	rte_pktmbuf_free(mbuf); 
	
}
#else
void snd_single_pdu(struct rte_mbuf* mbuf, uint8_t port_id)
{
    uint16_t data_len = 0;
	char *send_buffer;
    L2L1_MSG_HEADER inner_msg_header;
	uint16_t router_table_id;
	uint16_t msg_len;
    if(NULL != mbuf)
    {

        data_len = mbuf->data_len;
	    if(data_len >= PDU_BUFFER_SIZE)
	    {
			LOG(LOG_ERR, LOG_MODULE_ENCAP, LOG_ERR,
                LOG_CONTENT("buffer size less than pkt len"));
	        rte_pktmbuf_free(mbuf);
		    return;
        }
    }
	else
	{
		return;
	}

	struct rte_mbuf* send_mbuf = get_encap_mbuf(); 
	if(NULL != send_mbuf)
	{
        if(1 == port_id)
        {
        	Mbuf_Private_Header * mph = (Mbuf_Private_Header*)rte_mbuf_to_priv(mbuf);
        	struct rte_ether_hdr * ether_hdr = rte_pktmbuf_mtod(send_mbuf, struct rte_ether_hdr *);
			msg_len = mbuf->data_len + sizeof(struct rte_ether_hdr);
			/*ether_hdr->d_addr.addr_bytes[0] = 0xFF;  // 00:1B:21:E2:90:0E
			ether_hdr->d_addr.addr_bytes[1] = 0xFF;
			ether_hdr->d_addr.addr_bytes[2] = 0xFF;
			ether_hdr->d_addr.addr_bytes[3] = 0xFF;
			ether_hdr->d_addr.addr_bytes[4] = 0xFF;
			ether_hdr->d_addr.addr_bytes[5] = 0xFF;*/
			memcpy(&ether_hdr->d_addr, g_st_app_conf.local_mac[2], 6);
            memcpy(&ether_hdr->s_addr, g_st_app_conf.local_mac[port_id], 6);
			ether_hdr->ether_type =  htons(mph->protocol_type);
            send_buffer = (char *)rte_pktmbuf_append(send_mbuf, msg_len);
			rte_memcpy(send_buffer+sizeof(struct rte_ether_hdr), rte_pktmbuf_mtod(mbuf, uint8_t *), mbuf->data_len);
			snd_pkt(send_buffer, msg_len,  port_id);
		}
		rte_pktmbuf_free(send_mbuf);
	}
	else
	{
	    LOG(LOG_ERR, LOG_MODULE_ENCAP, LOG_ERR,
            LOG_CONTENT("send_mbuf allocated failed"));
	}
	rte_pktmbuf_free(mbuf); 
	
}
#endif


void snd_tun_pdu(char* buf, uint16_t msg_len)
{
	char *send_buffer;
    //L2L1_MSG_HEADER inner_msg_header;
    uint16_t pdu_buffer_offset = 0;
	uint32_t pri_proto_userid;

	struct rte_mbuf* send_mbuf = get_encap_mbuf(); 
	if(NULL != send_mbuf)
	{
	 
        uint16_t frame_size = g_st_app_conf.router_header_size[0]  + sizeof(uint32_t) + msg_len;
	    uint16_t mapping_id = get_mapping_index(g_st_app_conf.local_id, g_st_app_conf.local_id + 1, ntohs(router_header[0].segment[router_header[0].LAST_ENTRY]));
		send_buffer = (char *)rte_pktmbuf_append(send_mbuf, frame_size);
		router_header[0].D = DIRECTION_RTN;
		rte_memcpy(send_buffer, &router_header[0], g_st_app_conf.router_header_size[0]);
		pdu_buffer_offset +=  g_st_app_conf.router_header_size[0];
	    rte_memcpy(send_buffer + pdu_buffer_offset, &pri_proto_userid, sizeof(uint32_t));
		pdu_buffer_offset +=  sizeof(uint32_t);
        rte_memcpy(send_buffer + pdu_buffer_offset, buf, msg_len);
		if(false == enqueue_encap_ring(send_mbuf, mapping_id))
		{
			rte_pktmbuf_free(send_mbuf);
			printf("enqueue_encap_ring failed\n");
		}
	        
	}
	else
	{
        printf("send_mbuf allocated failed\n");
	}
	
}





/*void encapsulate_bbframe(struct rte_mbuf* mbuf, uint16_t port_id)
{
    //封包
    send_to(rte_pktmbuf_mtod(mbuf, uint8_t *), mbuf->data_len);
    return;
}*/





#ifdef TAG_ROUTE
void * encapsulate_thread (void * pv_arg)

{

    THREAD_HANDLE* pst_handle = (THREAD_HANDLE*)pv_arg;
    uint16_t us_serial_number = pst_handle->pParam;
    struct rte_mbuf *pkts_burst[MAX_PKT_BURST];
    int nb_rx, j;

    //ROUTERHEADER *router_header;
    int8_t  read_ok = -1;
    uint8_t loop_count[MAX_THREAD_NUM] = {0};
	uint8_t loop_count_1 = 0;
    printf("Hi, I'm thread encapsulate_thread,threadid:%d\n", us_serial_number);
	uint8_t        delay_time_mark_flag;
	uint64_t inrouter_time;
	uint64_t outrouter_time;
    do 
	{
        if(1 == g_st_app_conf.encap_thread_num) { // round robin
            for(int i = 0; i < g_st_app_conf.encap_ring_queue_num; i++) 
			{
                read_ok = 0;
				#if 1

                if((ISL_PACK == g_st_app_conf.port_to_pkt_type[i]) 
					|| (BBFRAME_PACK == g_st_app_conf.port_to_pkt_type[i])
					|| (ENUM_FWD_Tx_FWD_Data == g_st_app_conf.port_to_pkt_type[i])) 
				{
                    while(true) 
					{
                        read_ok = get_frag_pkt(i);
                        if(SUCCEED_FETCH_FRAGMENT == read_ok) 
						{
                            encapsulate_isl_bb(SUCCEED_FETCH_FRAGMENT, i);
                            continue;
                        } 
						else 
						{
                            break;
                        }
                    }
                } 

				#endif
				
                nb_rx = dequeue_encap_ring(pkts_burst, MAX_PKT_BURST, i);
                if(nb_rx != 0) 
				{ 
				    loop_count[i] = 0;
					loop_count_1 = 0;
					Mbuf_Private_Header * mph = (Mbuf_Private_Header*)rte_mbuf_to_priv(pkts_burst[0]);
	                if(1 == mph->delay_time_mark_flag)
		            {
		                inrouter_time = mph->inrouter_time;
						delay_time_mark_flag = 1;
						mph->delay_time_mark_flag = 0;
	                }
				    #if 0
					router_header = rte_pktmbuf_mtod(pkts_burst[j], ROUTERHEADER *);
	                ptmp = (uint32_t *)(router_header);
	                *ptmp = ntohl(*ptmp);

					if(DELAY_METRIC == router_header->RESERVE1)
					{
					    
                        continue;
					}
					#endif
                    if((ISL_PACK == g_st_app_conf.port_to_pkt_type[i]) 
						|| (BBFRAME_PACK == g_st_app_conf.port_to_pkt_type[i])
						|| (ENUM_FWD_Tx_FWD_Data == g_st_app_conf.port_to_pkt_type[i])) 
					{
                        for(j = 0; j < nb_rx; j++) {
                            ut_statis_debug.encap_pkt_num++;
                            read_ok = get_whole_pkt(pkts_burst[j], i);
                            encapsulate_isl_bb(read_ok, i);
                        }
                    } 
					else if((MULTI_FWD_PDU_PACK == g_st_app_conf.port_to_pkt_type[i]) || (MULTI_RTN_PDU_PACK == g_st_app_conf.port_to_pkt_type[i]))
					{
                        for(j = 0; j < nb_rx; j++) 
						{
                            encapsulate_pdu(pkts_burst[j], i);
                        }
                    }
					else if((FWD_TUNNEL_PDU == g_st_app_conf.port_to_pkt_type[i]) || (RTN_TUNNEL_PDU == g_st_app_conf.port_to_pkt_type[i]))
					{
                        for(j = 0; j < nb_rx; j++) 
						{
                            snd_single_pdu(pkts_burst[j], i);
                        }
                    }

	                if(1 == delay_time_mark_flag)
		            {
		                outrouter_time = get_cur_time_us();
						              //struct timeval tm_current = {0};
                  // gettimeofday(&tm_current, NULL);
				   //outrouter_time  = (uint64_t)((uint64_t)tm_current.tv_sec *USECONDS_PER_SECOND + tm_current.tv_usec);
						//printf("mph->outrouter_time:%lu, mapping id:%d\n",outrouter_time,i);
					    calc_delay_metric_time(inrouter_time, outrouter_time, i);
						delay_time_mark_flag = 0;
		            }

                }
				else
				{
				    loop_count[i]++;
					loop_count_1++;
					if(100 == loop_count_1)
					{
                        usleep(10);
						loop_count_1 = 0;
					}
					
					if(100 == loop_count[i])
					{
						loop_count[i] = 0;
					    if((ISL_PACK == g_st_app_conf.port_to_pkt_type[i]) 
					        || (BBFRAME_PACK == g_st_app_conf.port_to_pkt_type[i])
					        || (ENUM_FWD_Tx_FWD_Data == g_st_app_conf.port_to_pkt_type[i])) 
				        {
                            encapsulate_isl_bb(FAILED_FETCH_MODCOD, i);
                        } 
			     	    else if((MULTI_FWD_PDU_PACK == g_st_app_conf.port_to_pkt_type[i]) || (MULTI_FWD_PDU_PACK == g_st_app_conf.port_to_pkt_type[i])) 
				        {
				            encapsulate_pdu(NULL, i);//force send
                        }
					}
				}

	
            }
        } 
		else if(g_st_app_conf.encap_thread_num > 1) 
		{ //one on one
            nb_rx = dequeue_encap_ring(pkts_burst, MAX_PKT_BURST, us_serial_number);
            if(nb_rx != 0) 
			{
                if((ISL_PACK == g_st_app_conf.port_to_pkt_type[us_serial_number]) || (BBFRAME_PACK == g_st_app_conf.port_to_pkt_type[us_serial_number])) 
				{
                    for(j = 0; j < nb_rx; j++) 
					{
                        ut_statis_debug.encap_pkt_num++;
                        read_ok = get_whole_pkt(pkts_burst[j], us_serial_number);
                        encapsulate_isl_bb(read_ok, us_serial_number);
                    }
                }
				else
				{
				    for(j = 0; j < nb_rx; j++) 
					{
					    ut_statis_debug.encap_pkt_num++;
                        encapsulate_pdu(pkts_burst[j], us_serial_number);
				    }
                }
            }

            if((ISL_PACK == g_st_app_conf.port_to_pkt_type[us_serial_number]) || (BBFRAME_PACK == g_st_app_conf.port_to_pkt_type[us_serial_number])) 
			{
                while(true) 
				{
                    read_ok = get_frag_pkt(us_serial_number);
                    if(SUCCEED_FETCH_FRAGMENT == read_ok) 
					{
                        encapsulate_isl_bb(SUCCEED_FETCH_FRAGMENT, us_serial_number);
                        continue;
                    } 
					else 
					{
                        break;
                    }
                }

                if(nb_rx == 0) 
				{
                    encapsulate_isl_bb(FAILED_FETCH_MODCOD, us_serial_number);

                }
            } 
			else 
			{
                encapsulate_pdu(NULL, us_serial_number);//force send
            }
        }


    } while (TRUE);

    LOG(LOG_ERR, LOG_MODULE_MAIN, LOG_ERR,
        LOG_CONTENT("encapsulate_thread is over"));

    return NULL;
}


#else
void * encapsulate_thread (void * pv_arg)
{

    THREAD_HANDLE* pst_handle = (THREAD_HANDLE*)pv_arg;
    uint16_t us_serial_number = pst_handle->pParam;
    struct rte_mbuf *pkts_burst[MAX_PKT_BURST];
    int nb_rx, j;

    //ROUTERHEADER *router_header;
    int8_t  read_ok = -1;
    uint8_t loop_count[MAX_THREAD_NUM] = {0};
	uint8_t loop_count_1 = 0;
    printf("Hi, I'm thread encapsulate_thread,threadid:%d\n", us_serial_number);

    do 
	{
        if(1 == g_st_app_conf.encap_thread_num) { // round robin
            for(int i = 0; i < g_st_app_conf.encap_ring_queue_num; i++) 
			{
                read_ok = 0;
                if(1 == i) //pkt come from port 1
				{
                    while(true) 
					{
                        read_ok = get_frag_pkt(i);
                        if(SUCCEED_FETCH_FRAGMENT == read_ok) 
						{
                            encapsulate_isl_bb(SUCCEED_FETCH_FRAGMENT, i);
                            continue;
                        } 
						else 
						{
                            break;
                        }
                    }
                } 

                nb_rx = dequeue_encap_ring(pkts_burst, MAX_PKT_BURST, i);
                if(nb_rx != 0) 
				{ 
				    loop_count[i] = 0;
					loop_count_1 = 0;
					Mbuf_Private_Header * mph = (Mbuf_Private_Header*)rte_mbuf_to_priv(pkts_burst[0]);
                    if(1 == i) 
					{
                        for(j = 0; j < nb_rx; j++) {
                            ut_statis_debug.encap_pkt_num++;
                            read_ok = get_whole_pkt(pkts_burst[j], i);
                            encapsulate_isl_bb(read_ok, i);
                        }
                    } 
					else if(0 == i)//receive bbframe from port 0 and decapsulate to PDU to give to port 1
					{
                        for(j = 0; j < nb_rx; j++) 
						{
                            snd_single_pdu(pkts_burst[j], 1);
                        }
                    }
                }
				else
				{
				    loop_count[i]++;
					loop_count_1++;
					if(100 == loop_count_1)
					{
                        usleep(10);
						loop_count_1 = 0;
					}
					
					if(100 == loop_count[i])
					{
						loop_count[i] = 0;
					    if(1 == i) 
				        {
                            encapsulate_isl_bb(FAILED_FETCH_MODCOD, i);
                        } 
					}
				}

	
            }
        } 
		else if(g_st_app_conf.encap_thread_num > 1) 
		{ //one on one
            nb_rx = dequeue_encap_ring(pkts_burst, MAX_PKT_BURST, us_serial_number);
            if(nb_rx != 0) 
			{
                if((ISL_PACK == g_st_app_conf.port_to_pkt_type[us_serial_number]) || (BBFRAME_PACK == g_st_app_conf.port_to_pkt_type[us_serial_number])) 
				{
                    for(j = 0; j < nb_rx; j++) 
					{
                        ut_statis_debug.encap_pkt_num++;
                        read_ok = get_whole_pkt(pkts_burst[j], us_serial_number);
                        encapsulate_isl_bb(read_ok, us_serial_number);
                    }
                }
				else
				{
				    for(j = 0; j < nb_rx; j++) 
					{
					    ut_statis_debug.encap_pkt_num++;
                        encapsulate_pdu(pkts_burst[j], us_serial_number);
				    }
                }
            }

            if((ISL_PACK == g_st_app_conf.port_to_pkt_type[us_serial_number]) || (BBFRAME_PACK == g_st_app_conf.port_to_pkt_type[us_serial_number])) 
			{
                while(true) 
				{
                    read_ok = get_frag_pkt(us_serial_number);
                    if(SUCCEED_FETCH_FRAGMENT == read_ok) 
					{
                        encapsulate_isl_bb(SUCCEED_FETCH_FRAGMENT, us_serial_number);
                        continue;
                    } 
					else 
					{
                        break;
                    }
                }

                if(nb_rx == 0) 
				{
                    encapsulate_isl_bb(FAILED_FETCH_MODCOD, us_serial_number);

                }
            } 
			else 
			{
                encapsulate_pdu(NULL, us_serial_number);//force send
            }
        }


    } while (TRUE);

    LOG(LOG_ERR, LOG_MODULE_MAIN, LOG_ERR,
        LOG_CONTENT("encapsulate_thread is over"));

    return NULL;
}


#endif
