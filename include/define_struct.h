#if !defined(_DEFINE_STRUCT_H_)
#define _DEFINE_STRUCT_H_

#include <pthread.h>
#include <semaphore.h>
#include <sys/time.h>
#include <asm/byteorder.h>


#include "define_const.h"
#include "circular_queue.h"

#pragma pack(1)

#undef  __MUTEX__
#define __MUTEX__
//#define __SPINLOCK__
#ifdef __MUTEX__
	#ifndef  lock_t
		#define	lock_t 	pthread_mutex_t			//typedef pthread_mutex_t		LOCK;
	#endif	
#else
	#ifdef __SPINLOCK__
		#ifndef  lock_t
			#define	lock_t 	pthread_spinlock_t	//typedef pthread_spinlock_t	LOCK;	//
		#endif	
	#endif
#endif

typedef struct _EN_PDU_NODE 				EN_PDU_NODE;				// PDU属性：描述优先级队列和modcod队列的结点PDU
typedef struct _ONE_PDU 	  			ONE_PDU;				// 单个PDU的属性：描述GSE包里的PDU
typedef struct _ENCODE_INFO 			ENCODE_INFO;  			// PDU所使用的编码信息
typedef struct _OPT_EXT_HEADER_INFO		OPT_EXT_HEADER_INFO;	// 可选扩展头信息
typedef struct _MAN_EXT_HEADER_INFO		MAN_EXT_HEADER_INFO;	// 强制扩展头信息


typedef struct _PDU_GROUP		PDU_GROUP;		// 封装到同一个GSE包里的所有PDU的信息
// typedef struct _READY_GSE		READY_GSE;		// 
typedef struct _ONE_GSE			ONE_GSE;		// 单个GSE的组成信息
typedef struct _GSE_GROUP		GSE_GROUP;		// 封装入同一个BBFrame的所有GSE包的信息

typedef struct _L2L1_HEADER L2L1_HEADER;		// L2传递给L1信息的报文头


typedef struct _CIRCLE_ARRAY
{
	double*  value;				// 保存最近 SMOOTH_CYCLE_MAX_NUM 个 10ms/20MS 的数据的数组
	int8_t   head_pos;			// 指向 value 数组最老的数据的位置
	int8_t   tail_pos;			// 指向 value 数组最新的数据的位置
	uint8_t  array_max_size;	// value 数组的最大长度
	uint8_t  valid_max_number;	// value 数组中有效数据的最大个数，该值 <= array_max_size
	uint8_t  data_number;		// value 数组中已保存数据的数目，该值 <= array_max_size
	uint8_t  valid_number;		// value 数组中有效数据的个数，该值 <= valid_max_number

}CIRCLE_ARRAY;


/****************************************
* priority队列和MODCOD队列存储的PDU的相关信息  
****************************************/
struct _EN_PDU_NODE
{
	// 加入优先级队列前需要赋值的字段
	uint16_t pdu_size;				// 能够封装进GSE包的PDU字节数（如果PDU被分片，这是其中一个分片包的字节数）
	uint8_t* pPDU_start;			// PDU在存储区中的起始地址，如果是PDU的分片，表示该分片的起始地址
	uint8_t  mac_length;			// MAC地址长度。 0～6Bytes  1～3Bytes
	uint8_t  MAC_address[6];		// 目的MAC地址
	uint8_t  MODCOD;				// PDU所使用的MODCOD
	uint8_t  vip_flag;		 		// 1～用户是VIP用户，0～用户是普通用户
	uint8_t  mobility;              // 1 this vip user will run away
	uint32_t userID;				// PDU所属的用户
	uint8_t  priority;				// PDU的优先级
	uint16_t protocoltype;
	uint8_t  priority_group;		// 优先级组的序号：0～fast 1～RT  2～NRT。根据priority字段确定
	uint8_t* pPDU_address;			// 完整的PDU在存储区中的起始地址
	void *mbuf_address;             // DPDK mbuf 首地址

	// 统计时延参数
	uint64_t enq_time;			// PDU进入优先级队列的时刻
	uint64_t deq_time;			// PDU写入codeblock buffer的时刻
	uint64_t deq_prio_time;		// PDU从优先级队列迁出的时刻
	uint64_t deq_modcod_time;	// PDU从modcod队列迁出的时刻
	uint64_t read_time;			// PDU从待封装缓冲区读出的时刻
	
	// 分片时的参数
	uint8_t  fragment_flag;			// PDU是否分片标志		0～不分片  1～分片
	uint8_t  fragment_ID;
	uint8_t  fragment_type;			// PDU分片类型，fragment_flag=1时有效。1～start 2～middle	3～end
	uint8_t  fragment_seq;			// PDU分片的顺序号
	uint16_t pdu_total_length;		// 完整PDU的字节数（如果PDU被分片pdu_total_length>pdu_size，否则pdu_total_length==pdu_size）
	uint32_t crc32;					// 分片PDU的CRC32校验码，只有fragment_flag==1时有效

};


/****************************************
* 单个PDU的属性
****************************************/
struct _ONE_PDU
{
	uint32_t pdu_size;			// PDU封装在一个GSE包里的字节数
	uint8_t *pStart;			// PDU存储区的指针，如果是PDU的分片，表示该分片的起始地址

	uint8_t  priority;			// PDU的优先级
	uint64_t enq_time;			// PDU进入优先级队列的时刻
	uint64_t deq_time;			// PDU写入codeblock buffer的时刻
	uint64_t deq_prio_time;		// PDU从优先级队列迁出的时刻
	uint64_t deq_modcod_time;	// PDU从modcod队列迁出的时刻
	uint64_t read_time;			// PDU从待封装缓冲区读出的时刻

	uint8_t* pPDU_address;		// 完整的PDU在存储区中的起始地址
	void *mbuf_address;         // DPDK mbuf 首地址

	uint8_t  vip_flag;		 		// 1～用户是VIP用户，0～用户是普通用户
	uint8_t  mobility;              // 1 this vip user will run away

} ;

/****************************************
* PDU所使用的编码信息
****************************************/
struct _ENCODE_INFO
{
	uint8_t modcod;					// MODCOD序号：protocol_std=DVB-S2时 MODCOD取0～31；protocol_std=DVB-S2X时 MODCOD取64～127
	uint8_t code_type		: 2;	// 编码类型：来自配置文件？protocol_std=DVB-S2时有效  0～标准	1～短	2～中等
	uint8_t pilot_insertion	: 1;	// 插入导频：来自配置文件？	0～不插入		 1～插入
	uint8_t reserve			: 5;
	uint8_t VL_SNR_set;
} ;

struct _OPT_EXT_HEADER_INFO
{
	int header_num;									// 扩展头数目
	uint16_t header_type[EXT_HEADER_MAX_NUM];		// 扩展头的类型
	uint16_t header_size[EXT_HEADER_MAX_NUM];		// 扩展头的长度
	uint8_t  header_value[EXT_HEADER_MAX_NUM][20];	// 扩展头的值
	uint16_t next_type[EXT_HEADER_MAX_NUM];			// 下一个扩展头/PDU的类型

};

struct _MAN_EXT_HEADER_INFO
{
	int header_num;									// 扩展头数目
	int value_address[EXT_HEADER_MAX_NUM];			// 扩展头的内容是使用header_value里的值，还是使用地址header_address指向的数据  非0：header_value的值，0：header_address指向的数据
	uint16_t header_type[EXT_HEADER_MAX_NUM];		// 扩展头的类型
	uint16_t header_size[EXT_HEADER_MAX_NUM];		// 扩展头的长度
	uint16_t header_value[EXT_HEADER_MAX_NUM][10];	// 扩展头的值
	uint8_t *header_address[EXT_HEADER_MAX_NUM];	// 指向实际数据的指针
	uint16_t next_type[EXT_HEADER_MAX_NUM];			// 下一个扩展头/PDU的类型

};

/************************************************************************/
/*            GSE封装器与dequeuing modcod调度器的接口数据 		          */
/* 明确给出：
		一个frame里包含几个GSE包
		这个frame使用什么MODCOD
		一个GSE包里包含几个PDU，以及每一个PDU的属性信息
*  1个完整的PDU
   N个完整的PDU
   1个PDU的分片：start、middle、end
************************************************************************/

/************************************************************************
* 封装到一个GSE包里的所有PDU的信息
* 一个 PDU_GROUP 组成一个 GSE 包
* 一个 PDU_GROUP 里的所有 PDU 拥有相同的目的MAC地址 
************************************************************************/
struct _PDU_GROUP
{
	uint8_t	 MAC_address[6];			// PDU的目的MAC地址
	uint8_t  priority_group;			// modcod queue group 的序号：0～fast 1～RT 2～NRT
	uint16_t pdu_num;					// 一个GSE包所含PDU的个数，fragment_flag=1时，PDU_num=1

	uint16_t protocol_type;				// 第一个PDU的协议类型/扩展头类型
	uint16_t PDU_total_length;			// 该组中所有PDU或PDU分片包的总长度，只有1个PDU时=pdu_size，有N个PDU时=所有PDU_size总和 +（N*2）字节

	uint8_t  fragment_flag;				// PDU是否分片标志		0～不分片  1～分片
	uint8_t  fragment_type;				// PDU分片类型，fragment_flag=1时有效。1～start 2～middle	3～end
	uint8_t  fragment_ID;				// PDU的分片ID：只有fragment_flag=1时有效
	uint16_t fragmented_PDU_length;		// 被分片的PDU的完整长度，fragment_flag=1时有效。
	uint32_t crc32;						// 分片PDU的CRC32校验码，只有fragment_flag==1时有效

	ONE_PDU	 one_PDU[PDU_MAX_NUM];	// 一个GSE包里所有PDU的信息

	OPT_EXT_HEADER_INFO opt_ext_header_info;	// 一个GSE包里的optional扩展头信息
	MAN_EXT_HEADER_INFO man_ext_header_info;	// 一个GSE包里的mandatory扩展头信息

	uint8_t modcod;
	uint8_t label_type;
	uint8_t label_size;
	uint16_t gse_header_length;		// 一个PDU_GROUP组成的GSE包头总长度
	uint16_t ext_header_length;		// 一个PDU_GROUP组成的GSE包扩展头总长度
	uint16_t gse_data_length;		// 一个PDU_GROUP组成的GSE包的数据域长度（包括扩展头和数据）
	uint16_t gse_total_length;		// 一个PDU_GROUP组成的GSE包的总长度

};

/************************************************************************
*  一个 READY_GSE 包含 group_num 个 PDU_GROUP
*  一个 PDU_GROUP 是一个 GSE 包的数据字段
*  一个 PDU_GROUP 包含 pdu_num 个 PDU
************************************************************************/
// struct _READY_GSE
// {
// 	uint8_t  	priority_group;		// modcod queue group 的序号：0～fast 1～RT 2～NRT
// 	uint16_t	group_num;			// 一个BBFrame所含的GSE包个数 N
// 	ENCODE_INFO	encode_info;		// PDU所使用的编码信息：这些GSE包使用相同的MODCOD
// 	uint16_t 	all_gse_length;		// 全部GSE包的总长度
// 	uint64_t 	frame_create_time;	// frame填入第一个GSE包的时间
// 	PDU_GROUP*	pPDU_group;			// pPDU_group 指示GSE1的信息、pPDU_group+1 指示GSE2的信息、......、pPDU_group+N-1指示GSE(N-1)的信息
// };
/*              GSE封装器与dequeueing modcod调度器的接口数据                 */
/************************************************************************/


/************************************************************************/
/*                 GSE封装器与frame封装器的接口数据                     */

/****************************************
* 单个GSE的组成信息 
****************************************/
struct _ONE_GSE
{
	uint8_t  GSE_header[13];	// GSE报文头（最大 13 bytes）
	uint8_t  GSE_header_size;	// GSE报文头长度 Bytes
	uint16_t pdu_num;			// 一个GSE包里的PDU个数

	uint8_t  fragment_flag;		// 标识该GSE包里的PDU是否分片，当fragment_flag==1时，必须有PDU_num==1。
	uint8_t  fragment_type;		// 标识该GSE包里的PDU的分片类型：start、middle、end
	uint8_t  fragment_ID;
	uint32_t crc32;				// 分片PDU的crc校验码，只在fragment_flag==1且fragment_type==end时有效

	ONE_PDU  one_PDU[PDU_MAX_NUM];	// 一个GSE包里所有PDU的信息
	
	OPT_EXT_HEADER_INFO opt_ext_header_info;	// option扩展头信息
	MAN_EXT_HEADER_INFO man_ext_header_info;	// mandatory扩展头信息

};

/****************************************
* 封装入同一个BBFrame的所有GSE包的信息
****************************************/
struct _GSE_GROUP
{
	// change:2020.05.26，将READY_GSE的部分字段迁移到PDU_GROUP
	ENCODE_INFO	encode_info;		// PDU所使用的编码信息：这些GSE包使用相同的MODCOD
	uint64_t 	frame_create_time;	// frame填入第一个GSE包的时间
	// uint16_t all_gse_length;		// 全部GSE包的总长度
	// uint16_t	group_num;			// 一个BBFrame所含的GSE包个数 N

	uint8_t  priority_group;
	uint32_t total_length;			// frame里所有GSE包的总长度
	uint8_t  gse_num;				// BBFrame中所含GSE的个数
	
	ONE_GSE* pOneGSE;		// 保存所有GSE信息存储区的首地址，可以在初始化时开辟一个大小为800的存储区
};
/*                 GSE封装器与frame封装器的接口数据                     */
/************************************************************************/

/************************************************************************/
/* 									frame封装器与物理层的接口数据                       */

/* Code Block Buffer里存储的数据按照 L2_L1_HEADER + BBFrame 的形式逐条存储 */
/****************************************
* L2传递给L1信息的报文头 
****************************************/
struct _L2L1_HEADER
{
    uint32_t timestamp_in_seconds;
	uint32_t timestamp_in_nanoseconds;
	uint16_t reserve;
	uint16_t frame_size;
	uint32_t sequence_num;
	uint32_t reserve1;
	uint32_t reserve2;
	uint32_t reserve3;
	uint32_t reserve4;


};

typedef struct _TX_FWD_DATA
{
    uint32_t request_id;
#if defined(__LITTLE_ENDIAN_BITFIELD)
	uint8_t base_singal_version:4;
    uint8_t base_band_signal_type:4;
#elif defined (__BIG_ENDIAN_BITFIELD)
	uint8_t base_band_signal_type:4;
	uint8_t base_singal_version:4;
#else
#error  "Please fix <asm/byteorder.h>"
#endif
	uint8_t scramble_index;	
	uint8_t psl_code;
	uint8_t vl_snr_set;
	
#if defined(__LITTLE_ENDIAN_BITFIELD)
	uint8_t reserve:4;
    uint8_t priority:4;
#elif defined (__BIG_ENDIAN_BITFIELD)
	uint8_t priority:4;
	uint8_t reserve:4;
#else
#error  "Please fix <asm/byteorder.h>"
#endif
	uint8_t  Reserve_bits;
	uint16_t KBCH_Length;
	uint32_t Super_Frame_Number;
	uint32_t Timestamp_in_seconds;
	uint32_t Timestamp_in_nanosecond;
	uint8_t  Reserve_bytes[8];

}__attribute__((packed))TX_FWD_DATA;

typedef struct _FWD_Tx_Indication
{
	uint32_t Indication_Mask;
	uint32_t Indication_ID;
	
	// Indication_Mask bit 31
	uint8_t  MAC_Address[6];
	uint16_t MODEM_Status;

	// Indication_Mask bit 30
	uint32_t Available_Buffer;
	uint32_t Sending_Rate;

	// Indication_Mask bit 29
	uint32_t Frame_Number;
	uint32_t SFTP_Delay;

	// Indication_Mask bit 28
	uint32_t Alarm_Type;
	uint32_t Alarm_ID;
	
	uint8_t Reserve_bytes[8];
} __attribute__((packed)) FWD_Tx_Indication;



/* 									frame封装器与物理层的接口数据                       */
/************************************************************************/

/************************************************************************/
/* 濞磋偐濞�閿熻姤甯炲▓鎴濃槈閸喍绱栧ù锝忔嫹                                        */

/************************************************************************/

typedef struct
{
	uint8_t  bAllow;				// 本队列是否可以进行传输  0～否   1～是
	uint8_t  priority_group;// 本队列所属的MODCOD队列集合	0～fast	 1～RT	2～NRT
	uint8_t  MODCOD;				// 本队列所采用的MODCOD值
	uint16_t pdu_num;				// 本队列里可以进行传输的PDU个数

} MODCOD_Q_INFO;





/*	PDU分片的信息	*/
typedef struct _FRAGMENT_INFO
{
	uint8_t  FragmentID;		// 使用的FragmentID
	uint8_t  total_num;			// PDU被分片的总个数，包括 Start-packet、middle-Packet、End-Packet
	uint8_t  transfered_num;	// 已经传输的分片个数
	uint16_t middle_size;		// PDU的MODCOD对应的frame净载荷长度
	uint16_t start_size;		// Start-packet的长度
	uint16_t end_size;			// End-packet的长度（不包括crc校验码）
	uint32_t CRC32;				// 对整个PDU（可以把扩展头和数据PDU合并看作一个PDU）的CRC32校验码
	
} FRAGMENT_INFO;

typedef struct _FRAGMENT_BUFFER
{
	uint8_t used;						// 判断FragmentID是否已被标识为已分配使用
	FRAGMENT_INFO fragment_info;
	EN_PDU_NODE pdu_node;					// 从MODCOD队列读取的一个PDU的信息

} FRAGMENT_BUFFER;

typedef struct _FRAME_PAYLOAD
{
	uint16_t used_payload;		// frame里已经填入的字节数
	uint16_t usable_payload;	// frame里还可用的字节数
	uint16_t max_payload;		// frame最大可填入的字节数
	uint8_t  frame_modcod;		// frame所使用的modcod值

} FRAME_PAYLOAD;

typedef struct _PADDING_INFO
{
	uint16_t used_size;		// 一个frame被填充的字节数
	uint16_t frame_size;	// 一个frame的字节数
	uint64_t total_padding;	// N个frame累计被padding的字节数
	uint64_t total_frame;	// N个frame的总字节数

} PADDING_INFO;

/* 封装需要的参数  */
typedef struct _ENCAP_PARAM
{
	// 需要初始化的参数
	PDU_GROUP *pPDU_Group;				// usingPDU先放入PDU_GROUP中，一个PDU_GROUP可以包含若干个PDU
	GSE_GROUP *pGSE_Group;				// 把READY_GSE里的PDU_GROUP封装成一个GSE包，加到GSE_GROUP中，一个GSE_GROUP可以包含若干个READY_GSE，这些GSE包采用相同的物理层参数

	// 封装时得到的参数
	EN_PDU_NODE using_pdu;					// 正在进行处理的PDU，从待封装缓存区中取出的PDU放在using_pdu中，以后对using_pdu进行操作
																		
	// 分片参数
	FRAGMENT_BUFFER *pfragment_buffer;  // 用来保存using_pdu进行fragment的结果
	CIRCULAR_QUEUE fragmentID_queue;	// 用来保存已经分配的 fragmentID

} ENCAP_PARAM;

/****************************************
* BBHeader
****************************************/
typedef struct _BBHEADER
{

#if defined(__LITTLE_ENDIAN_BITFIELD)
	// Matype1
	uint8_t RO			:2; // Transmission Roll-off factor. 00 = 0.35	01 = 0.25	10 = 0.20  11 = reserved
	uint8_t NPD 		:1; // Null-packet deletion active/not active.	1 = active	0 = not-active
	uint8_t ISSYI		:1; // (Input Stream Synchronization Indicator): If ISSYI = 1, the ISSY field is inserted after UPs.  1 = active  0 = not-active
	uint8_t CCM_ACM 	:1; // Constant Coding and Modulation or Adaptive Coding and Modulation (VCM is signalled as ACM).	  1 = CCM  0 = ACM
	uint8_t SIS_MIS 	:1; // Single Input Stream or Multiple Input Stream. 1 = single  0 = multiple
	uint8_t TS_GS		:2; // Transport Stream Input or Generic Stream Input (packetized or continuous).
							// 11 = Transport	00 = Generic Packetized    01 = Generic continuous	 10 = reserved
#elif defined (__BIG_ENDIAN_BITFIELD)
	// Matype1
	uint8_t TS_GS		:2; // Transport Stream Input or Generic Stream Input (packetized or continuous).
							// 11 = Transport	00 = Generic Packetized    01 = Generic continuous	 10 = reserved
	uint8_t SIS_MIS 	:1; // Single Input Stream or Multiple Input Stream. 1 = single  0 = multiple
	uint8_t CCM_ACM 	:1; // Constant Coding and Modulation or Adaptive Coding and Modulation (VCM is signalled as ACM).	  1 = CCM  0 = ACM
	uint8_t ISSYI		:1; // (Input Stream Synchronization Indicator): If ISSYI = 1, the ISSY field is inserted after UPs.  1 = active  0 = not-active
	uint8_t NPD 		:1; // Null-packet deletion active/not active.	1 = active	0 = not-active
	uint8_t RO			:2; // Transmission Roll-off factor. 00 = 0.35	01 = 0.25	10 = 0.20  11 = reserved
#else
#error  "Please fix <asm/byteorder.h>"
#endif
	// Matype2
	uint8_t Matype2;		// If SIS_MIS = 0, then Matype2 = Input Stream Identifier (ISI); else reserved.
	uint16_t UPL;			// User Packet Length in bits, in the range 0 to 65535. 0000(HEX) = continuous stream.
	uint16_t DFL;			// Data Field Length in bits, in the range 0 to 58112.
	uint8_t  SYNC;			// for packetized Transport or Generic Streams: copy of the User Packet Sync byte;
							// SYNC = 00(HEX) when the input Generic packetized stream did not contain a sync-byte
							// (therefore the receiver, after CRC-8 decoding, shall remove the CRC-8 field without reinserting the Sync-byte).
	uint16_t SYNCD; 		// for packetized Transport or Generic Streams: distance in bits from the beginning of the DATA FIELD and the first User Packet
							// from this frame (first bit of the CRC-8). SYNCD = 65535 means that no User Packet starts in the DATA FIELD;
	uint8_t  CRC8;			// error detection code applied to the first 9 bytes of the BBHEADER.
} __attribute__((packed)) BBHEADER;

/****************************************
* ISLHeader
****************************************/
typedef struct _ISLHEADER
{
    uint8_t SYN;
#if defined(__LITTLE_ENDIAN_BITFIELD)
	uint8_t D			:1; 
	uint8_t M 		    :1; 
	uint8_t E 	        :1; 
	uint8_t RE      	:5; 
#elif defined (__BIG_ENDIAN_BITFIELD)
    uint8_t RE			:5; 
    uint8_t E			:1; 
	uint8_t M 		    :1; 
	uint8_t D			:1; 
#else
#error  "Please fix <asm/byteorder.h>"
#endif

	uint8_t  Link_Control;		
	uint16_t Frame_Sequence;			
	uint16_t DFL;
	uint32_t CRC32;
} __attribute__((packed)) ISLHEADER;


/* 封装frame时参数  */
typedef struct _FRAME_PARAM
{
	BBHEADER initialBBHeader;		// 初始化的BBHEADER值
	ISLHEADER initialISLHeader;		// 初始化的BBHEADER值
	TX_FWD_DATA L2L1_header;		// L2L1通信的协议报文头
	uint32_t padding_bytes;
	uint8_t pack_type;
	uint16_t gse_offset;
	uint16_t header_size;//ISL or BBFRAME header size.
	int16_t  frame_size;
	uint64_t dummy_count;
	uint64_t frame_count;
} FRAME_PARAM;

/* 进行封装需要的参数 */
typedef struct _ENCAP_INFO
{
	int group_id;				// modcod queue group 的序号：0～fast 1～RT 2～NRT
	int timeout_period;			// 超时周期
	ENCAP_PARAM encap_param[ENCAP_Q_NUM];	// 进行GSE和frame封装时使用的参数。有2组，分别对应fast数据和普通数据 
	FRAME_PARAM frame_param[ENCAP_Q_NUM];	// frame封装时使用的参数。有2组，分别对应fast数据和普通数据 
	PADDING_INFO padding_info[ENCAP_Q_NUM];	// debug参数


} ENCAP_INFO;


/****************************************
* L2L1_MSG_HEADER
****************************************/
typedef struct _L2L1_MSG_HEADER
{
	uint8_t m_cDstMacAddress[6];  // destination address
	uint8_t m_cSrcMacAddress[6];  // source address
	uint16_t m_cType;             // high layer protocol 0x0800:IP protocol，0x0806: arp
	
	uint16_t mac_rev;
	
#if defined(__LITTLE_ENDIAN_BITFIELD)
	uint8_t  V          :4;
	uint8_t  R          :4;					
#elif defined (__BIG_ENDIAN_BITFIELD)
	uint8_t  R		    :4; 							
	uint8_t  V 	        :4; 
#else
#error  "Please fix <asm/byteorder.h>"
#endif
    uint8_t msg_type;
	uint16_t msg_len;
	uint32_t reserve;
} __attribute__((packed)) L2L1_MSG_HEADER;


// GSE包头固定部分
typedef struct
{
#if defined(__LITTLE_ENDIAN_BITFIELD)

    uint16_t  GSE_Length        : 12;   // GSE Packet(不含S、E、GSE_LENTH 3个字段) 的总长度
    uint16_t  Label_Type		:  2;   // 00:Label是6字节MAC地址
										// 01:Label是3字节MAC地址
										// 10:广播。该Packet没有Label字段，所有接收者都会处理这个Packet
										//    在非广播系统中也可以使用这个值，这是Layer2不做任何处理，留给IP层处理
										// 11:重用Label字段。该Packet没有Label字段
										//    接收方继续使用最近一次接收到的label字段值，不能在frame的第一个Packet使用这个值；前一个packet的label_type也不能为10
	uint16_t End_Indicator 		:1;  	// 1:閻炴稏鍔庨妵姘交濞嗗酣鍤婫SE Packet闁哄嫷鍨辨竟娆愭姜閻ｅ本鐣盤DU闁告帒妫涙晶鏍儍閸曨剚浠橀柛姘凹缁斿瓨绋夐敓锟�
	uint16_t Start_Indicator 	:1;   	// 1:閻炴稏鍔庨妵姘交濞嗗酣鍤婫SE Packet闁哄嫷鍨辨竟娆愭姜閻ｅ本鐣盤DU闁告帒妫涙晶鏍儍閸曨収鍎戝☉鎿勬嫹濞戞搫鎷�
#elif defined (__BIG_ENDIAN_BITFIELD)
    uint16_t Start_Indicator 	:1; 
    uint16_t End_Indicator 		:1; 
	uint16_t  Label_Type		:  2; 
	uint16_t  GSE_Length        : 12;

#else
#error  "Please fix <asm/byteorder.h>"
#endif


} __attribute__((packed)) GSE_FIXED_HEADER;

// GSE包头可变部分
typedef struct
{
	uint8_t fragment_ID_size;
	uint8_t total_length_size;
	uint8_t protocol_type_size;
	uint8_t label_size;

} GSE_VARIABLE_HEADER_SIZE;


// GSE包头可变部分计算过程中的临时存储区
typedef struct
{
	uint8_t  fragment_ID;
	uint16_t total_length;
	uint16_t protocol_type;
	uint8_t  label[6];

} GSE_VARIABLE_HEADER_VALUE;

// GSE包头可变部分计算过程中的临时存储区
typedef struct
{
	GSE_VARIABLE_HEADER_SIZE  variable_header_size;
	GSE_VARIABLE_HEADER_VALUE variable_header_value;

} GSE_VARIABLE_HEADER;


typedef struct _MSG_BANDWIDTH
{
    long msg_type; 		//大于0
    char msg_text[512];

}MSG_BANDWIDTH;


// 一个100ms的记录项
typedef struct _DELAY_RECORD
{
  	// 每个查询周期的记录值，  K
	uint32_t prio_avg[PRIORITY_Q_NUM];
	uint32_t prio_N  [PRIORITY_Q_NUM];	// 每一个10ms内的，单个优先级的PDU的个数：10个10ms
	uint32_t total_avg;					// 普通用户NRT所有优先级的统计数据：M个10ms，保存计算得出的平均时延
	uint32_t all_total_avg;				// 普通用户和VIP用户NRT所有优先级的统计数据：M个10ms，保存计算得出的平均时延

	// 一个100ms周期的记录值
	uint32_t stat_prio_avg[PRIORITY_Q_NUM];				// 普通用户NRT单个优先级的统计数据：K个（100ms的平均时延）
	uint32_t stat_total_avg;							// 普通用户NRT所有优先级的统计数据（100ms的平均时延）
	uint32_t stat_all_total_avg;						// 普通用户和VIP用户NRT所有优先级的统计数据（100ms的平均时延）

}DELAY_RECORD;


// 一个100ms的记录项
typedef struct _DELAY_RECORD_VIP
{
  	// 每个查询10ms周期的记录值     
	uint32_t avg_VIP;		// 所有VIP用户的统计数据：M个10ms，保存计算得出的平均时延
	uint32_t avg_MOB;		// 所有mobility标识用户的统计数据：M个10ms，保存计算得出的平均时延

	// 单个统计100ms周期的记录值
	uint32_t stat_avg_VIP;					// 所有VIP用户的统计数据
	uint32_t stat_avg_MOB;					// 所有mobility标识用户的统计数据

}DELAY_RECORD_VIP;


// 由10ms计算得到的平均时延，在100ms时平均得到的时延
typedef struct _DELAY_STAT_COMMON
{
	uint32_t prio_avg[PRIORITY_Q_NUM];	// 100ms周期单个优先级的PDU平均时延
	uint32_t total_avg;					// 100ms周期所有优先级的PDU平均时延

}DELAY_STAT_COMMON;

// 由10ms计算得到的平均时延，在100ms时平均得到的时延
typedef struct _DELAY_STAT_VIP
{
	uint32_t avg_VIP;	// 100ms周期所有VIP用户的PDU平均时延
	uint32_t avg_MOB;	// 100ms周期所有所有Mobility标识用户的PDU平均时延

}DELAY_STAT_VIP;


typedef struct _DELAY_CHECK
{
	// 针对每一个查询周期，对每一个优先级进行统计
	int32_t  min[PRIORITY_Q_NUM];	// 单个优先级的时延最小值
	uint32_t max[PRIORITY_Q_NUM];	// 单个优先级的时延最大值
	uint32_t avg[PRIORITY_Q_NUM];	// 单个优先级的PDU平均时延
	uint64_t sum[PRIORITY_Q_NUM];	// 单个优先级的每个PDU的时延总和
	uint64_t N[PRIORITY_Q_NUM];		// 单个优先级的PDU的总个数

	// 针对每一个查询周期，对所有优先级的统计
	uint32_t total_avg;				// 针对普通用户NRT所有优先级的平均时延
	uint64_t total_sum;				// 针对普通用户NRT所有优先级的每个PDU的时延总和
	uint64_t total_N;				// 针对普通用户NRT所有优先级的PDU的总个数

	uint32_t all_total_avg;			// 包括普通用户和VIP用户，NRT所有优先级的平均时延
	
}DELAY_CHECK;


typedef struct _DELAY_CHECK_VIP
{
	// 针对每一个查询周期，对所有VIP用户的统计
	uint32_t avg_VIP;				// 针对所有VIP用户的平均时延
	uint64_t sum_VIP;				// 针对所有VIP用户的每个PDU的时延总和
	uint64_t num_VIP;				// 所有VIP用户的PDU的总个数

	// 针对每一个查询周期，对所有Mobility标识用户的统计
	uint32_t avg_MOB;				// 针对所有Mobility标识用户的平均时延
	uint64_t sum_MOB;				// 针对所有Mobility标识用户的每个PDU的时延总和
	uint64_t num_MOB;				// 所有Mobility标识用户的PDU的总个数

}DELAY_CHECK_VIP;

typedef struct _DELAY_SMOOTH_ARRAY
{
	uint64_t delay_array[CHECK_CYCLE_NUM];	// 保存最近10个10ms的数据的数组
	uint8_t  head_pos;						// 指向delay_array数组最老的数据的位置
	uint8_t  tail_pos;						// 指向delay_array数组最新的数据的位置
	uint8_t  valid_number;					// delay_array数组中有效数据的数目

}DELAY_SMOOTH_ARRAY;

typedef struct _DELAY_PARAM
{
	DELAY_CHECK delay_check;

	// 按100ms为一个查询周期的测量值
	uint64_t stat_delay_sum[PRIORITY_Q_NUM];	// 100ms周期PDU时延总和（单个优先级）
	uint64_t stat_N[PRIORITY_Q_NUM];			// 100ms周期PDU个数总和（单个优先级）
	uint64_t stat_total_delay_sum;				// 100ms周期所有优先级的PDU时延总和
	uint64_t stat_total_N;						// 100ms周期所有优先级的PDU个数总和
	
	DELAY_STAT_COMMON delay_stat_common;		// 统计周期的平均时延统计结果(按10ms为一个查询周期，100ms为一个统计周期)
	DELAY_RECORD delay_record;					// 记录的时延测量参量

	uint32_t all_total_avg;						// 包括普通用户和VIP用户，NRT所有优先级的平均时延

	uint8_t  forbid_update;		// 

	DELAY_SMOOTH_ARRAY common_array;				// 保存最近10个10ms的时延数据的数组(所有优先级)
	DELAY_SMOOTH_ARRAY prio_array[PRIORITY_Q_NUM];	// 保存最近10个10ms的时延数据的数组(单个优先级)
	DELAY_SMOOTH_ARRAY vip_array;					// 保存最近10个10ms的时延数据的数组(vip的NRT优先级)
	DELAY_SMOOTH_ARRAY all_array;					// 保存最近10个10ms的时延数据的数组(普通和vip的NRT优先级)

}DELAY_PARAM;


typedef struct _DELAY_PARAM_VIP
{
	DELAY_CHECK_VIP delay_check;

	// 按100ms为一个查询周期的测量值
	uint64_t stat_delay_sum_VIP;	// 100ms周期所有VIP用户的PDU时延总和
	uint64_t stat_num_VIP;			// 100ms周期所有VIP用户的PDU个数总和
	uint64_t stat_delay_sum_MOB;	// 100ms周期所有Mobility标识用户的PDU时延总和
	uint64_t stat_num_MOB;			// 100ms周期所有Mobility标识用户的PDU个数总和

	// 按10ms为一个查询周期，100ms为一个统计周期，统计周期的平均时延统计值
	DELAY_STAT_VIP delay_stat;

	// 记录的时延测量参量
	DELAY_RECORD_VIP delay_record;

}DELAY_PARAM_VIP;



//copy from ut


// 完整的PDU的缓存节点
typedef struct _DE_PDU_NODE
{
	uint16_t pdu_length;	// PDU长度
	uint8_t* ptr_pdu;		// 存储完整的PDU

}DE_PDU_NODE;


// 分片的PDU信息
typedef struct _FRAG_INFO
{
	uint16_t time_stamp;				// 属于同一fragment_ID的PDU的时戳
	uint16_t pdu_type;					// 属于同一fragment_ID的PDU的协议类型 IP包/前向控制消息/反向控制消息
	uint16_t max_frag_seq;				// 属于同一fragment_ID的PDU的发送序号最大值，发送序号从0开始
	uint16_t pdu_recv_len;				// 属于同一fragment_ID的PDU的已接收分片的总长度
	uint16_t frag_recv_num;				// 属于同一fragment_ID的PDU的已接收分片的总个数，frag_recv_num==(max_frag_seq+1)时表示接收完成整个PDU
	uint8_t  recv_end;					// 接收到同一fragment_ID的最后一个PDU分片标志
	uint64_t t0_us;						// 属于同一fragment_ID的PDU的第一个被接收的分片的接收时间
	uint64_t t1_us;						// 属于同一fragment_ID的PDU的最后一个被接收的分片的接收时间
	int8_t   timeout_flag;				// 属于同一fragment_ID的PDU的分片接收超时标志
	uint16_t frame_seq_num;				// 调试用
	uint16_t frag_len[FRAG_MAX_NUM];	// 属于同一fragment_ID的PDU的每一个分片的长度
	uint8_t* data_addr[FRAG_MAX_NUM];	// 属于同一fragment_ID的PDU的每一个分片的暂存区，接收到frag_seq的PDU分片暂存在data_addr[frag_seq]
	uint16_t protocol_type;				// PDU的协议类型 IPv4/IPv6 etc.
	DE_PDU_NODE pdu_node;
	uint32_t crc;

}FRAG_INFO;


// 收到的mac地址信息
typedef struct _PREVIOUS_MAC_INFO
{
	uint8_t label_type;
	uint8_t mac_length;			// mac地址长度
	uint8_t mac_address[6];		// mac地址

}PREVIOUS_MAC_INFO;

// FRAME NODE的缓存节点
typedef struct _FRAME_NODE
{
	uint8_t* frame_addr;		// frame的首地址
	struct rte_mbuf* mbuf;		// 存储frame的mbuf地址

}FRAME_NODE;

// 解封装参数
typedef struct _DECAP_PARAM
{
	FRAG_INFO frag_info[MAX_FRAG_ID][FRAG_BUFFER_NUM];

}DECAP_PARAM;



typedef struct _PDU_ATTRI
{
	uint16_t type;		// 消息类型
	uint16_t length;	// 消息长度 bytes
	uint64_t t;			// 重组耗时
	
}PDU_ATTRI;






typedef struct _MBUF_NODE
{
	struct rte_mempool* pool;	// mbuf所在的内存池指针
	
}MBUF_NODE;


typedef struct _Mbuf_Private_Header
{
	uint32_t       direction;   // 0- fwd;  1- rtn;  other- unknown;  big_end, for ACL
    uint32_t       protocol_type;
	uint64_t       inrouter_time;
	uint64_t       outrouter_time;
	uint8_t        delay_time_mark_flag;


} __attribute__((packed)) Mbuf_Private_Header;



//copy from ut


#pragma pack()

#endif
