
#if !defined(_ENCAPSULATE_H_)
#define _ENCAPSULATE_H_
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

#define ERROR_FRAGMENT_ID_INVALID	-1		// fragmentID 不是合法值
#define ERROR_FRAGMENT_SIZE			-2		// fragment 的长度错误
#define ERROR_FRAGMENT_ID_USED		-3		// fragmentID 在用

void * encapsulate_thread (void * pv_arg);
int16_t fetch_pdu_fragment(FRAGMENT_BUFFER *fragment_buffer, CIRCULAR_QUEUE *pfragmentID_queue, EN_PDU_NODE *pPDU_Node);
int8_t read_enapsulate_buffer(EN_PDU_NODE *pdu_node, struct rte_mbuf* mbuf);
uint8_t make_frame(ENCAP_PARAM *encap_param, FRAME_PAYLOAD *frame_payload, FRAME_PARAM *frame_param, uint16_t port_id);


int8_t force_send(ENCAP_PARAM* ptr_encap_param, FRAME_PAYLOAD *pframe_payload, FRAME_PARAM* pFrameParam, uint8_t insert_dummy, uint16_t port_id);
void encapsulate_pdu(struct rte_mbuf* mbuf, uint8_t mapping_id);


void encapsulate_isl_bb(int8_t      read_ok, uint16_t port_id);
void fragment_process(ENCAP_PARAM* ptr_encap_param, FRAME_PAYLOAD* ptr_frame_payload, FRAME_PARAM* pFrameParam, uint8_t label_type, uint8_t label_size, uint8_t* ptr_null_frame, uint16_t mapping_id);

uint8_t padding_rule(uint16_t frame_usable_payload, uint16_t padding_threshold);
uint8_t get_min_avail_modcod(uint8_t modcod, uint8_t taget_modcod, uint32_t gse_total_length);



struct rte_mbuf *get_encap_mbuf(void);

void init_encap_mem(void);

uint8_t get_frag_pkt(uint16_t mapping_id);
uint8_t get_whole_pkt(struct rte_mbuf* mbuf, uint16_t mapping_id);
void calc_delay_metric_time(uint64_t inrouter_time, uint64_t outrouter_time, uint8_t mapping_id);
void snd_tun_pdu(char* buf, uint16_t msg_len);
void snd_single_pdu(struct rte_mbuf* mbuf, uint8_t mapping_id);







#endif

