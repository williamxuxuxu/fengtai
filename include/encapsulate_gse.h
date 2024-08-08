#if !defined(_ENCAPSULATE_GSE_H_)
#define _ENCAPSULATE_GSE_H_


#include "define_const.h"
#include "define_struct.h"
#include "common_dpdk.h"

#define TEST_ENCAPSULATE_GSE		0xB000

#ifndef NULL
#define NULL 0
#endif


#ifdef __cplusplus 
extern "C" {
#endif



uint8_t decide_label(uint8_t mac_length1, uint8_t mac_address1[], uint8_t mac_length2, uint8_t mac_address2[], uint8_t null_frame, uint8_t* label_type);
void   update_last_label(uint8_t mac_length1, uint8_t mac_address1[], uint8_t *mac_length2, uint8_t mac_address2[]);
int8_t add_to_pdu_group(const EN_PDU_NODE *pPDU_Node, PDU_GROUP *pPDU_group, uint8_t label_type, uint8_t label_size);
// int8_t add_opt_extension_header(OPT_EXT_HEADER_INFO *pheader_info, uint16_t header_type, uint16_t value_size, uint8_t *pheader_value, uint16_t pdu_type);
// int8_t add_man_extension_header(MAN_EXT_HEADER_INFO *pheader_info, uint16_t header_type, uint16_t value_size, uint8_t *pheader_value, uint16_t pdu_type, int8_t value_address);
int8_t add_opt_extension_header(OPT_EXT_HEADER_INFO *pheader_info, OPT_EXT_HEADER_INFO *src_header, uint16_t pdu_type);
int8_t add_man_extension_header(MAN_EXT_HEADER_INFO *pheader_info, MAN_EXT_HEADER_INFO *src_header, uint16_t pdu_type);



void init_frame_payload(FRAME_PAYLOAD *ptr_frame_payload, uint8_t modcod);
void recalc_frame_payload(FRAME_PAYLOAD *ptr_frame_payload, uint8_t modcod);
void update_frame_payload(FRAME_PAYLOAD *ptr_frame_payload, uint16_t delta_length);
void update_pdu_group(EN_PDU_NODE *pPDU_Node, PDU_GROUP *pPDU_group, uint8_t label_type, uint8_t label_size, uint16_t mapping_id);
void update_gse_group(FRAME_PAYLOAD *pframe_payload, ENCAP_PARAM *pEncap_param, uint8_t bResetSRc);

uint32_t calc_data_length(PDU_GROUP *pPDU_group);
uint32_t calc_ext_header_length(PDU_GROUP *pPDU_group);

void reset_pdu_group(PDU_GROUP *pPDU_group);
void reset_gse_group(GSE_GROUP *pGSE_group);

GSE_FIXED_HEADER package_gse_fixed_header(int8_t encapsulation_type, uint8_t label_type, uint8_t fragment_type, uint16_t data_length, GSE_VARIABLE_HEADER_SIZE *pvariable_header_size);
GSE_VARIABLE_HEADER_SIZE calc_gse_variable_header_size(int8_t encapsulation_type, uint8_t fragment_type, uint16_t label_type, uint16_t data_length);
uint16_t calc_gse_length(GSE_VARIABLE_HEADER_SIZE variable_header_size, uint16_t data_length);
int8_t   package_gse_variable_header(GSE_VARIABLE_HEADER_SIZE variable_header_size, PDU_GROUP *pPDU_group, uint8_t *pvariable_header);
int8_t   get_encapsulation_type(uint16_t pdu_num, uint8_t fragment_flag);
uint16_t calc_datafield_length(PDU_GROUP *pPDU_group);

uint8_t  get_broadcast_type(const uint8_t MAC_address[]);
int8_t   encapsulate_gse(PDU_GROUP *pPDU_group, GSE_GROUP *pGSE_group, uint8_t bResetSrc);



#ifdef __cplusplus 
}
#endif

#endif
