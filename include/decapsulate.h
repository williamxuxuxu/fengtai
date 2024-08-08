
#if !defined(_DECAPSULATE_H_)
#define _DECAPSULATE_H_

#include "stats.h"
#include "define_const.h"
#include "define_struct.h"

#include "list_template.h"


#include <string.h>
#include <stdio.h>

#ifndef max
#define max(a,b)    (((a) > (b)) ? (a) : (b))
#endif


#ifdef __cplusplus 
extern "C" {
#endif

#define PDU_TYPE_NUM    3   // GSE里封装的PDU的类型个数
#define IP_PACKET       0   // GSE里封装的PDU的类型:IP包
#define CTRLMSG_FWD     1   // GSE里封装的PDU的类型:前向控制信息
#define CTRLMSG_RTN     2   // GSE里封装的PDU的类型:反向控制信息



/* 扩展头类型值的定义 */
#define PROTOCOL_IPV6		0x86DD
#define PROTOCOL_IPV4		0x0800
#define PROTOCOL_ARP        0x0806
#define PROTOCOL_CTRL		0x00C9		// 控制信息
#define  MAC_ADDR_BYTE_LEN          6
#define DECAP_MBUF_SIZE     8192


#define INDC_AVAIL_BUF_SENDING_RATE_MASK  (1U << 30)


void init_frag_info(FRAG_INFO frag_info[][MAX_FRAG_ID][FRAG_BUFFER_NUM]);

void reset_frag_info(FRAG_INFO* ptr_frag_info);

uint8_t start_end_indicator(GSE_FIXED_HEADER header);

void decapsulate(void);
int16_t read_codeblock(uint8_t* pBuffer, uint32_t offset, uint8_t* ptr_read_end);
FRAME_NODE* read_reccive_list(void);

int16_t get_L2L1_Header(uint8_t* pCodeBlock, L2L1_HEADER* pL2L1_header);
int16_t get_frame(uint8_t* pBuffer, uint16_t offset, uint16_t frame_size);

int16_t decapsulate_gse_header(uint8_t* pGSE, uint16_t* pProtocol_type, GSE_FIXED_HEADER* pfixed_header, GSE_VARIABLE_HEADER* pvariable_header);
int16_t get_extension_header_info(uint8_t* pCodeBlock, uint16_t cur_header_type, uint16_t* next_type);
uint8_t get_extension_header_type(uint16_t header_type);
int16_t get_pdu_concat_info(uint16_t* next_header_type, uint8_t* pheader);
void	get_BBHeader(uint8_t* pBBFrame, BBHEADER* pBBHeader);

int8_t  regular_ext_header_type(uint16_t header_type);
void 	get_concat_pdu(uint8_t* ptr_datafield, uint16_t datafield_length);
int get_single_pdu(uint8_t* ptr_datafield, uint16_t datafield_length, uint16_t protocol_type, uint16_t port_id, uint64_t inrouter_time);


int8_t start_reassemble_pdu(uint8_t*  ptr_datafield,  
							GSE_VARIABLE_HEADER var_header, 
							uint16_t  timestamp_seq, 
							uint16_t  header_length, 
							uint16_t  frag_len, 
							uint16_t  protocol_type, 
							FRAG_INFO frag_info[][MAX_FRAG_ID][FRAG_BUFFER_NUM],
							int8_t*   buffer_index,
							uint8_t   port_id);

int8_t keep_reassemble_pdu( uint8_t   pdu_frag_type, 
							uint8_t*  ptr_datafield, 
							uint8_t   frag_ID, 
							uint16_t  timestamp_seq, 
							uint16_t  frag_len, 
							FRAG_INFO frag_info[][MAX_FRAG_ID][FRAG_BUFFER_NUM],
							int8_t*   buffer_index,
							uint8_t   port_id);

int8_t finish_reassemble_pdu(FRAG_INFO* frag_info, uint16_t port_id);

int8_t identify_isl(ISLHEADER* pISLHeader);

int8_t  identify_gs(BBHEADER* pBBHeader);
int8_t  filter_address(GSE_FIXED_HEADER fixed_header, GSE_VARIABLE_HEADER var_header, uint8_t local_air_mac[], uint8_t bfirst_gse, PREVIOUS_MAC_INFO* ptr_previous_mac_info);

void process_pdu_concat(void);          // 空函数
void process_fragment_sequence(void);   // 空函数

void free_mbuf(struct rte_mbuf* mbuf);  
// void free_mbuf(FRAME_NODE* frame_node);

uint8_t calc_CRC8(uint8_t *pdata, uint32_t len);


// 将解封装后的PDU从接收mbuf转移到数据mbuf
// PDU的协议类型、目的IP、包长度
void move_pdu(uint8_t* dst_addr, uint8_t* src_addr, uint16_t data_length, uint16_t protocol_type);

void fragment_timeout(FRAG_INFO frag_info[][MAX_FRAG_ID][FRAG_BUFFER_NUM]);

void * decapsulate_thread (void * pv_arg);
void init_decap_mem(void);

void decapsulate_pdu(struct rte_mbuf* pdu_mbuf, 	FRAME_PARAM *frame_para, uint16_t queue_id);



void decapsulate_gse(struct rte_mbuf* frame_mbuf, 	FRAME_PARAM *frame_para, uint8_t queue_id);

void dispatch_decap_pkt(struct rte_mbuf* mbuf, uint16_t queue_id);
int process_frame_header(struct rte_mbuf *frame_mbuf, FRAME_PARAM *frame_para, uint16_t queue_id);
void decode_delay_pdu(struct rte_mbuf* pdu_mbuf);


int process_frame_header_test(struct rte_mbuf *frame_mbuf, FRAME_PARAM *frame_para);







#ifdef __cplusplus 
}
#endif



#endif
