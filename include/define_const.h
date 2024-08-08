#if !defined(_DEFINE_CONST_H_)
#define _DEFINE_CONST_H_

#include <stddef.h>
//#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>



#pragma pack(1)

//#define  _DEBUG_

/********************************************************************************/
/* �����������Ͷ����� stdint.h ֮��, ������ͷ�ļ���Ҫ����Щ����ɾ��			*/
//typedef unsigned char 		uint8_t;
//typedef unsigned short 		uint16_t;
//typedef unsigned int 		uint32_t;
//typedef unsigned long long	uint64_t;
//typedef __int8_t  int8_t;
//typedef __int16_t int16_t;
//typedef __int32_t int32_t;
//typedef __int64_t int64_t;
/********************************************************************************/

#ifndef max
#define max(a,b)    (((a) > (b)) ? (a) : (b))
#endif

#ifndef min
#define min(a,b)    (((a) < (b)) ? (a) : (b))
#endif

#define RECEIVE_BUFFER_SIZE		1000*1024*1024


#define PRIORITY_FAST		0
#define PRIORITY_RT			1
#define PRIORITY_NRT		2
#define NRT_PRIORITY_LEVEL	3

#define PRIORITY_Q_NUM		(2 + NRT_PRIORITY_LEVEL)	// 优先级队列的个数
#define PRIORITY_GRP_NUM	3							// 优先级队列的组数：fast、RT、NRT共3组

#define ENCAP_Q_NUM			16		// 待封装缓存区的个数 fast、data

#define TRUE  1
#define FALSE 0
#define SUCCESS	(0)
#define ERROR	(-1) 
#define false 0
#define true  1


#define POINT_ERROR		-128		// 指针错误：空指针
#define SIZE_ERROR		-1			// 长度错误：长度与预期不符

#define GSE_PARAM_ERROR		-100		// GSE封装参数冲突

#define CONCAT_ERROR	-1			// 串接参数发生错误
#define CONCAT_UNALLOWED 0			// 串接不被允许

// 前后PDU的目的MAC相同，frame最大长度 <= GSE最大长度：新取PDU串接到原有的GSE包中后，形成的情况
#define CONCAT_EXIST_ALLOW			1	// GSE包还允许串接PDU
#define CONCAT_EXIST_FIT_FRAME		2	// GSE包刚好填满一个frame
#define CONCAT_EXIST_OVER_FRAME		3	// GSE包总长度超过frame最大长度

// 前后PDU的目的MAC相同，frame最大长度 > GSE最大长度：新取PDU串接到原有的GSE包中后，形成的情况
// 1. 新取PDU串接进已有GSE包后，gse包长度 <= GSE最大长度
// #define CONCAT_EXIST_ALLOW		1
// 2. 新取PDU串接进已有GSE包后，gse包长度 > GSE最大长度
#define CONCAT_NEW_ALLOW			4	// 新取PDU新建GSE包：包长度 < frame可用长度；允许再取一个PDU进行串接判断
#define CONCAT_NEW_FIT_FRAME		5	// 新取PDU新建GSE包：包长度 = frame可用长度；将全部的PDU_GROUP各自进行GSE封装，并封装进一个frame
#define CONCAT_NEW_OVER_FRAME		6	// 新取PDU新建GSE包：包长度 > frame可用长度；将全部的PDU_GROUP（最新PDU的首个分片）各自进行GSE封装，并封装进一个frame

// 前后PDU的目的MAC不同，frame最大长度 无关 GSE最大长度：新取PDU新建GSE包，形成的情况
#define CONCAT_MAC_DIFFERENCE_ALLOW			7	// 新取PDU新建GSE包：包长度 < frame可用长度；允许再取一个PDU进行串接判断
#define CONCAT_MAC_DIFFERENCE_FIT_FRAME		8	// 新取PDU新建GSE包：包长度 = frame可用长度；将全部的PDU_GROUP各自进行GSE封装，并封装进一个frame
#define CONCAT_MAC_DIFFERENCE_OVER_FRAME	9	// 新取PDU新建GSE包：包长度 > frame可用长度；将全部的PDU_GROUP（最新PDU的首个分片）各自进行GSE封装，并封装进一个frame

#define PDU_MAX_NUM		180			// 一个GSE包里最多封装的PDU数目
#define GSE_MAX_NUM		1000		// 一个frame里最多封装的GSE包数目
#define GSE_MAX_SIZE	4095		// 一个GSE数据包的最大字节数

#define FRAGMENT_MAX_NUM	256		// PDU分片的最大个数

#define SUCCEED_FETCH_FRAGMENT	1	// 成功读取PDU的分片
#define	SUCCEED_FETCH_MODCOD	2	// 成功读取完整的PDU
#define FAILED_FETCH_MODCOD     3

#define MODCOD_VALID_MIN	 1		// 实际有效的MODCOD最小值
#define MODCOD_VALID_MAX	28		// 实际有效的MODCOD最大值
#define MODCOD_MAX_NUM	    32		// 系统支持的最大MODCOD阶数
#define USER_MAX_NUM	100*1024	// 系统支持的最大用户数
#define EXT_HEADER_MAX_NUM	10		// 一个GSE包支持的扩展头最大数目
#define FAST_MODCOD			MODCOD_VALID_MIN + 3

#define GSE_HEADER_LENGTH_INTEGRITY_PDU1	10	// 不分片的PDU组成的GSE包头长度（指定目的地）
#define GSE_HEADER_LENGTH_INTEGRITY_PDU2	4	// 不分片的PDU组成的GSE包头长度（广播）
#define GSE_HEADER_LENGTH_FRAGMENT_PDU_S1	13  // 分片的PDU的start-packet组成的GSE包头长度（目的地6Bytes）
#define GSE_HEADER_LENGTH_FRAGMENT_PDU_S2	7   // 分片的PDU的start-packet组成的GSE包头长度（广播）
#define GSE_HEADER_LENGTH_FRAGMENT_PDU_S3	10  // 分片的PDU的start-packet组成的GSE包头长度（目的地3Bytes）
#define GSE_HEADER_LENGTH_FRAGMENT_PDU_M1	11	// 分片的PDU的middle-packet组成的GSE包头长度（在同一个frame中与前一个目的MAC地址不同的情况）
#define GSE_HEADER_LENGTH_FRAGMENT_PDU_E1	11	// 分片的PDU的end-packet组成的GSE包头长度（在同一个frame中与前一个目的MAC地址不同的情况）
#define GSE_HEADER_LENGTH_FRAGMENT_PDU_M2	5	// 分片的PDU的middle-packet组成的GSE包头长度（在同一个frame中与前一个目的MAC地址相同的情况）
#define GSE_HEADER_LENGTH_FRAGMENT_PDU_E2	5	// 分片的PDU的end-packet组成的GSE包头长度（在同一个frame中与前一个目的MAC地址相同的情况）


#define RTE_PKTMBUF_PRIVATE_SIZE 128   // private_size = cache_line_size * n, cache_line_size set 64 by default



enum MODCOD_Q_GROUP_CODE
{
	Q_GRP_FAST = 0,	// FAST的MODCOD队列集合
	Q_GRP_RT,		// RT的MODCOD队列集合
	Q_GRP_NRT,		// NRT的MODCOD队列集合
};


/* GSE封装形式：
 1个完整的PDU
 N个完整的PDU串接
 1个PDU的分片
 */
enum ENCAPSULATION_TYPE_CODE
{
	ENCAPSULATION_TYPE_WHOLE = 0,	// GSE数据包是一个完整的PDU
	ENCAPSULATION_TYPE_CONCAT,		// GSE数据包是由多个完整PDU串接
	ENCAPSULATION_TYPE_FRAG,		// GSE数据包是PDU分片
};

/* PDU的分片的类型：start、middle、end	*/
enum FRAGMENT_TYPE_CODE
{
	FRAGMENT_TYPE_S = 0,	// GSE数据包是第一个分片
	FRAGMENT_TYPE_M,		// GSE数据包中间分片
	FRAGMENT_TYPE_E,		// GSE数据包最后一个分片
};

/* GSE的目的地址类型 */
enum LABEL_TYPE_CODE
{
	LABEL_TYPE_6B = 0,		// Label字段是6 Bytes的MAC地址
	LABEL_TYPE_3B,			// Label字段是3 Bytes的系统自定义方式
	LABEL_TYPE_BROADCAST,	// 不包含Label字段。用于广播。
	LABEL_TYPE_REUSE		// 不包含Label字段。重用最近一个收到的Label
};

/* GSE的是否是广播数据 */
enum BOARDCAST_TYPE_CODE
{
	BROADCAST_TYPE_NO = 0,		// GSE数据包不是广播数据
	BROADCAST_TYPE_YES		// GSE数据包是广播数据
};

/* 扩展头类型值的定义 */
#define PROTOCOL_IPV6		0x86DD
#define PROTOCOL_IPV4		0x0800
#define PROTOCOL_CTRL		0x00C9

#define DEFAULT_NEXT_TYPE 	0x86DD		// 默认的下一组数据类型值
#define	PDU_CONCAT			0x0003		// PDU串接扩展头

#define FRAG_SEQUENCE		0x0202		// 分片序列码扩展头

#define MANDATORY_EXT_HEADER	0
#define OPTIONAL_EXT_HEADER		1


#define FIFO_ITEM_SIZE		7373							// CodeBlock FIFO的每一条item的字节数
#define PDU_BUFFER_SIZE     8192 - 256
#define MAX_THREAD_NUM      16


#define MAX_VALUE_64BIT 	(2<<64 - 1)

#define CHECK_CYCLE			10000 	// us 查询周期  10ms
#define CHECK_CYCLE_NUM		10		// 查询周期个数  10个10ms = 100ms
#define STAT_CYCLE_NUM		10*60   // 统计周期个数  10*60个100ms = 1s*60	

#define SMOOTH_CYCLE_MAX_NUM	20	// 每 SMOOTH_CYCLE_NUM 个估计带宽值，进行一次平滑，SMOOTH_CYCLE_NUM个数的最大值
#define SMOOTH_CYCLE_NUM		5	// 每 SMOOTH_CYCLE_NUM 个估计带宽值，进行一次平滑


#define BW_SAVE_CYC_NUM	 	10 * 1000 / 10	// 10s时段内10ms周期的个数 

		   
#define TEST_REC_STATISTICS_RESULT	3678
// #define TEST_FRAME_CONTENT			0xA001
// #define TEST_QUEUE_SIZE				0xA002

#define DELAY_METRIC 3
#define USECONDS_PER_SECOND 1000000ULL


//copy from ut
#define POINT_ERROR		-128		
#define SIZE_ERROR		  -1	

#define MANDATORY_EXT_HEADER	0
#define OPTIONAL_EXT_HEADER		1

#define WHOLE_PDU			3		// 完整的PDU
#define FRAG_START			2		// PDU的第一个分片
#define FRAG_MIDDLE			0		// PDU的中间分片
#define FRAG_END			1		// PDU的最后一个分片

#define EXT_HEADER_MAX_NUM 	10
#define FRAG_MAX_NUM		6		// 一个PDU的最大分片个数
#define MAX_FRAG_ID			256		// 能缓存的分片的PDU的最大数目

#define FRAG_MAX_LENGTH		1518	// PDU分片的最大字节数

//copy from ut code begin
/* 扩展头类型值的定义 */
#define PROTOCOL_IPV6		0x86DD
#define PROTOCOL_IPV4		0x0800
#define PROTOCOL_CTRL		0x00C9		// 控制信息


#define DEFAULT_NEXT_HEADER_TYPE 0x86DD

#define TEST_SNDU		0x0000
#define BRIDGED_SNDU	0x0001
#define	PDU_CONCAT		0x0003
#define	GSE_LLC			0x0087


#define TIME_STEMP		0x0301
#define FRAG_SEQUENCE	0x0202


#define TRUE  1
#define FALSE 0


#define RECV_PKT_MAX_NUM  256*1024	


#define FRAG_BUFFER_NUM 2



//copy from ut

#define MAX_DEVICE_NUM 8

typedef enum _Pack_Type
{
    INVALID_PACK = 0,
    FWD_TUNNEL_PDU = 1,
    RTN_TUNNEL_PDU =2,
	ISL_PACK = 3,
	BBFRAME_PACK = 4,
	DELAY_METRIC_FRAME = 6,
	MISSING_PDU = 7,
	MULTI_FWD_PDU_PACK = 17,	
	MULTI_RTN_PDU_PACK = 18,	
	TYPE_END
} Pack_Type;


typedef enum _FWD_L2_L1_MSG_Type
{
	ENUM_FWD_Tx_Indication     = 0x01,
	ENUM_FWD_Tx_Configure,
	ENUM_FWD_Tx_Reset,
	ENUM_FWD_Tx_Testing,
	ENUM_FWD_Tx_Confirm,
	ENUM_FWD_Tx_FWD_Data,
	ENUM_FWD_Request_SFTP,
	ENUM_FWD_Rx_Indication     = 0x11,
	ENUM_FWD_Rx_Configure,
	ENUM_FWD_Rx_Reset,
	ENUM_FWD_Rx_Testing,
	ENUM_FWD_Rx_Confirm,
	ENUM_FWD_Rx_FWD_Data
}FWD_L2_L1_MSG_Type;

typedef enum _RTN_L2_L1_MSG_Type
{
	ENUM_RTN_Rx_Indication     = 0x01,
	ENUM_RTN_Rx_Configure,
	ENUM_RTN_Rx_Reset,
	ENUM_RTN_Rx_Testing,
	ENUM_RTN_Rx_Confirm,
	ENUM_RTN_Rx_RTN_Data,
	ENUM_RTN_RCM_Request_Time,
	ENUM_RTN_Request_BAP,
	ENUM_RTN_Tx_Indication     = 0x11,
	ENUM_RTN_Tx_Configure,
	ENUM_RTN_Tx_Reset,
	ENUM_RTN_Tx_Testing,
	ENUM_RTN_Tx_Confirm,
	ENUM_RTN_Tx_RTN_Data,
	ENUM_RTN_UT_Request_Time
}RTN_L2_L1_MSG_Type;




#pragma pack()

#endif
