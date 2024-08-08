#if !defined(_ENCAPSULATE_FRAME_H_)
#define _ENCAPSULATE_FRAME_H_

#include "define_struct.h"
#include "define_const.h"
#include "general_module.h"





#ifdef __cplusplus 
extern "C" {
#endif


#pragma pack(1)

#define FRAME_HEADER_SIZE  ((sizeof(ISLHEADER)) > (sizeof(BBHEADER)) ? (sizeof(ISLHEADER)) : (sizeof(BBHEADER)))
void init_BBHeader(BBHEADER *pBBHeader, TX_FWD_DATA *pL2L1_header);
void init_ISLHeader(ISLHEADER *pISLHeader, TX_FWD_DATA *pL2L1_header);

uint16_t isl_padding(uint16_t islframe_size, ISLHEADER *pISLHeader);

int8_t 	 package_BBHeader(BBHEADER initialBBHeader, uint16_t bbframe_size, uint16_t data_size, BBHEADER *pBBHeader);
uint16_t padding(uint16_t bbframe_size, BBHEADER *pBBHeader);
int8_t 	 package_L2L1_Header(uint16_t frame_max_payload, ENCODE_INFO encode_info, TX_FWD_DATA *pL2L1_header);
uint8_t  map_PLScode(ENCODE_INFO encode_info);
int8_t encapsulate_frame(GSE_GROUP GSE_group, uint16_t frame_max_payload, uint16_t data_size, ENCODE_INFO encode_info, FRAME_PARAM *pFrame_param, uint16_t mapping_id);

void encode_inner_header(L2L1_MSG_HEADER *inner_msg_header,  uint16_t frame_size, uint8_t mapping_id, uint8_t direction);

uint32_t write_codeblock(L2L1_MSG_HEADER *inner_msg_header, TX_FWD_DATA *pL2L1_header, uint8_t *frame_header, uint16_t frame_header_size, GSE_GROUP* pGSE_group, uint16_t padding_bytes, uint16_t mapping_id);

int8_t package_ISLHeader(ISLHEADER initialISLHeader, uint16_t islframe_size, uint16_t data_size, ISLHEADER *pISLHeader);
void * snd_bbframe_thread (void * pv_arg);
uint8_t mcs_get_pls_code(uint8_t modcod, bool normal_code_block_size);


#ifdef __cplusplus 
}
#endif

#pragma pack()

#endif
