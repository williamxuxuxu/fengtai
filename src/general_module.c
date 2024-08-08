#include <net/if.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>


#include "general_module.h"
#include "global_var.h"
#include "receive.h"

#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wunused-value"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"



unsigned char CRC8_table[256];
unsigned int  CRC32_table[256];
App_Conf    g_st_app_conf       = {0};

uint8_t dst_mac[6];
extern char if_name[MAX_NETCARD_NUM][32];


char local_nic_name[20];
extern uint8_t local_air_mac[6];
static uint16_t valid_payload[32] = { 0, 
									2001 - 10, 2676 - 10, 3216 - 10, 4026 - 10, 4836 - 10, 5380 - 10, 6051 - 10, 6456 - 10, 6730 - 10, 7184 - 10, 7274 - 10, 
									4836 - 10, 5380 - 10, 6051 - 10, 6730 - 10, 7184 - 10, 7274 - 10, 
									5380 - 10, 6051 - 10, 6456 - 10, 6730 - 10, 7184 - 10, 7274 - 10, 
									6051 - 10, 6456 - 10, 6730 - 10, 7184 - 10, 7274 - 10,
							  	   	0, 0, 0 };

#define MCS_CRC_8_POLY           0xD5
// X8+X7+X6+X4+X2+1

#define MCS_CRC_32_POLY          0x4C11DB7
// x32 + x26 + x23 + x22 + x16 + x12 + x11 + x10 + x8 + x7 + x5 + x4 + x2 + x + 1

#define MCS_CRC_TBL_COLUMN_NUM  ((uint16_t) 256)

static uint8_t	 crc8_table [MCS_CRC_TBL_COLUMN_NUM];
static uint32_t  crc32_table[MCS_CRC_TBL_COLUMN_NUM];

static bool crc8_tbl_has_init  = false;
static bool crc32_tbl_has_init = false;

/*
 * Func:   Reverse bits of an input (e.g. 1000b -> 0001b).
 * Input:  data to reverse, bit width to care.
 * Output: None
 * Return: Bits reversed data.
 */
static uint32_t mcs_bit_rev(uint32_t data, uint32_t bit_width)
{
	uint32_t bit_idx;
	uint32_t ret = 0;

	for(bit_idx=0;bit_idx<bit_width;bit_idx++) {
		if(data & 0x01) {
			ret |= 1<<(bit_width-bit_idx-1);
		}
		data>>=1;
	}
	return ret;
}

/*
 * Func:   Init CRC8_DVB_S2 table.
 *		   Poly   Init	 RefIn	 RefOut   XorOut
 *		   0xD5   0x00	 false	 false	  0x00
 * Input:  Polyomial.
 * Output: None
 * Return: None
 */
static void mcs_init_crc8_table(uint32_t poly)
{
	if(crc8_tbl_has_init == true) {
		return;
	}
	
	uint16_t col_idx;
	uint32_t value;
	
	uint32_t bit = 0x80;
	uint8_t  bit_idx;
	uint8_t  one_byte_bit_num = 8;
	
	for(col_idx=0; col_idx<MCS_CRC_TBL_COLUMN_NUM; col_idx++) {
		value = col_idx;
		for(bit_idx=0; bit_idx<one_byte_bit_num; bit_idx++) {
			if(value & bit) {
				value = (value << 1) ^ poly;
			}
			else {
				value = (value << 1);
			}
		}
	
		crc8_table[col_idx] = value;
	}
	
	crc8_tbl_has_init = true;
}

/*
 * Func:   Init crc-32 table.
 * Input:  Polyomial.
 * Output: None
 * Return: None
 */
static void mcs_init_crc32_table(uint32_t poly)
{
	if(crc32_tbl_has_init == true) {
		return;
	}

	uint32_t col_idx;
	int j;
	uint32_t crc;
	
	poly = mcs_bit_rev(poly, 32);

	for(col_idx=0; col_idx<MCS_CRC_TBL_COLUMN_NUM; col_idx++) {
		crc = col_idx;
		for (j=0; j<8; j++) {
			crc = (crc & 1 ) ? (poly^(crc>>1)) : (crc>>1);
		}

		crc32_table[col_idx] = crc;
	}
	
	crc32_tbl_has_init = true;
}

/*
 * Func:   Get CRC8 by lookup table.
 * Input:  data, data length
 * Output: None
 * Return: CRC8 if success, 0 otherwise.
 */
uint8_t mcs_get_crc8(uint8_t *data, uint32_t len)
{
	if(data== NULL || len == 0) {
		return 0;
	}
	
	mcs_init_crc8_table(MCS_CRC_8_POLY);
	
	uint32_t crc8 = 0x00;
	while(len--) {
		crc8 = crc8_table[crc8 ^ *data++];
	}
	
	return crc8;
}

/*
 * Func:   Get several segments data's CRC32 by lookup table. 
 *		   It's caller's duty to:
 *			  1. MUST input pre_crc=0xFFFFFFFF when first segment
 *			  2. MUST calc ultimately crc32 by return_crc32 ^ 0xFFFFFFFF when last segment.
 * Input:  data, data length, previous segment crc.
 * Output: None
 * Return: Segment CRC32 if success, 0 otherwise.
 */
uint32_t mcs_get_crc32(uint8_t *seg_data, uint32_t seg_len, uint32_t pre_seg_crc)
{
	if(seg_data== NULL || seg_len == 0) {
		return 0;
	}

	mcs_init_crc32_table(MCS_CRC_32_POLY);

	uint8_t  index;

	while (0 != seg_len--) {
		index = (uint8_t)(pre_seg_crc ^ (*seg_data++));
		pre_seg_crc = crc32_table[index] ^ (pre_seg_crc >> 8);
	}

	return pre_seg_crc;
}





static int convert_ipv4_val(const char *val, uint32_t *ip)
{

	if (inet_pton(AF_INET, val, ip) != 1)
		return -EINVAL;

	return 0;
}

 uint64_t get_current_time_us(void)
{
	uint64_t time_us;
	struct timeval time;
	gettimeofday(&time, NULL);
	time_us = time.tv_sec * 1000000 + time.tv_usec;
									
    return time_us;
}

inline uint64_t get_current_time_s(void)
{
	uint64_t time_s;
	struct timeval time;
	gettimeofday(&time, NULL);
	time_s = time.tv_sec;
									
    return time_s;
}


void crc8_init_table(unsigned int poly)
{
	int i;
	int j;
	unsigned char crc;
	
	poly = bit_rev(poly,8);

	for(i=0; i<256; i++)
	{
		crc = i;
		for (j=0; j<8; j++)
		{
			crc = (crc & 1 ) ? (poly^(crc>>1)) : (crc>>1);
		}

		CRC8_table[i] = crc;
	}
}

/********************************************************************************
* 功能：查表法计算CRC8校验码
* 参数：
 [in]	pdata：PDU数据的首地址
 [in]	len：PDU数据字节数
* 返回值：
 CRC8校验码
* 方法说明：
 生成多项式 X8+X7+X6+X4+X2+1
********************************************************************************/
uint8_t crc8_lookup_table(uint8_t *pdata, uint32_t len)
{
	uint32_t CRC8 = 0;
	uint8_t  index;
	int i = 0;

	while (0 != len--)
	{
		index = CRC8;
		CRC8 <<= 8;
		CRC8 ^= CRC8_table[index ^ (*pdata++)];
	}

	return CRC8;
}


/********************************************************************************
* 功能：计算CRC8校验码
* 参数：
 [in]	pdata：PDU数据的首地址
 [in]	len：PDU数据字节数
* 返回值：
 CRC8校验码
* 方法说明：
 生成多项式 X8+X7+X6+X4+X2+1
********************************************************************************/
uint8_t calc_crc8(uint8_t *pdata, uint32_t len)
{
	uint32_t Gx = 0xC5;	// 生成多项式
	uint32_t CRC8 = 0;
	int i = 0;

	while (0 != len--)
	{
		CRC8 ^= (*pdata++);

		for (i = 0; i < 8; i++)
		{
			if (CRC8 & 0x80)
			{
				CRC8 <<= 1;
				CRC8 ^= Gx;
			}
			else
			{
				CRC8 <<= 1;
			}
		}
	}

	return CRC8;
}


unsigned int bit_rev(unsigned int input, int bw)
{
	int i;
	unsigned int var = 0;

	for(i=0;i<bw;i++)
	{
		if(input & 0x01)
		{
			var |= 1<<(bw-1-i);
		}
		input>>=1;
	}
	return var;
}


void crc32_init_table(unsigned int poly)
{
	int i;
	int j;
	unsigned int crc;
	
	poly = bit_rev(poly,32);

	for(i=0; i<256; i++)
	{
		crc = i;
		for (j=0; j<8; j++)
		{
			crc = (crc & 1 ) ? (poly^(crc>>1)) : (crc>>1);
		}

		CRC32_table[i] = crc;
	}
}




/********************************************************************************
* 功能： 查表法计算CRC32校验码
 生成多项式 x32 + x26 + x23 + x22 + x16 + x12 + x11 + x10 + x8 + x7 + x5 + x4 + x2 + x + 1
* 参数：
 [in]	pdata：PDU数据的首地址
 [in]	len：PDU数据字节数
* 返回值：	CRC32校验码
********************************************************************************/
uint32_t crc32_lookup_table(uint8_t *pdata, uint32_t len)
{
	uint32_t CRC32 = 0xFFFFFFFF;
	uint8_t  index;
	int i = 0;

	while (0 != len--)
	{
		index = (uint8_t)(CRC32 ^ (*pdata++));
		CRC32 = CRC32_table[index] ^ (CRC32 >> 8);
	}

	return (CRC32 ^ 0xFFFFFFFF);
}


/********************************************************************************
* 功能：   计算CRC32校验码
 生成多项式 x32 + x26 + x23 + x22 + x16 + x12 + x11 + x10 + x8 + x7 + x5 + x4 + x2 + x + 1
* 参数：
 [in]	pdata：PDU数据的首地址
 [in]	len：PDU数据字节数
* 返回值：	CRC32校验码
********************************************************************************/
uint32_t calc_crc32(uint8_t *pdata, uint32_t len)
{
	uint32_t Gx = 0x04C11DB7;	// 生成多项式
	uint32_t CRC32 = 0;
	int i = 0;

	while (0 != len--)
	{
		CRC32 ^= ((*pdata++) << 24);

		for (i = 0; i < 8; i++)
		{
			if (CRC32 & 0x80000000)
			{
				CRC32 <<= 1;
				CRC32 ^= Gx;
			}
			else
			{
				CRC32 <<= 1;
			}
		}
	}

	return CRC32;
}


/********************************************************************************
* 功能：计算时间差
* 参数：
		 [in]	time0 ：起始时间
		 [in]	time1 ：终止时间
* 返回值：
		 时间差，单位us
********************************************************************************/
inline int64_t calc_time_diff(struct timeval time0, struct timeval time1)
{
	int64_t t0, t1;
	int64_t delay;

	t0 = time0.tv_sec * 1000000 + time0.tv_usec;
	t1 = time1.tv_sec * 1000000 + time1.tv_usec;

	delay = t1 - t0;
	return delay;
}


/********************************************************************************
* 功能：根据MODCOD值查询对应的frame有效数据长度
* 参数：
		 [in]	modcod：modcod值
* 返回值：
		 frame有效数据长度
********************************************************************************/
inline uint16_t map_modcod_payload(uint8_t modcod)
{
	return valid_payload[modcod];
}


// #define  __USE_DPDK__
inline uint64_t get_current_time(void)
{
	uint64_t time_s;


	struct timeval time;
	gettimeofday(&time, NULL);
	time_s = time.tv_sec;



	return time_s;
}



void acess_circle_array(CIRCLE_ARRAY* array, double data)
{
	// 把当前数据值存入循环数组
	if(array->data_number >= array->array_max_size)	// 数组已存满
	{
		*(array->value+array->head_pos) = data;	// 用当前得到的最新值替换掉数组中最老的数据
		array->tail_pos = array->head_pos;	// 更新最新数据的位置
	
		// 更新数组中最老的数据的位置
		if( array->head_pos < (array->array_max_size-1) )
		{
			++(array->head_pos);
		}
		else
		{
			array->head_pos = 0;
		}

		if( array->valid_number < array->valid_max_number)
		{
			++(array->valid_number);
		}
	}
	else	// 数组未存满
	{
		// 更新最新数据的位置
		array->tail_pos = array->head_pos + array->data_number;	
		if(array->tail_pos > (array->array_max_size - 1))
		{
			array->tail_pos -= array->array_max_size;
		}

		*(array->value+array->tail_pos) = data;

		++(array->data_number);

		if( array->valid_number < array->valid_max_number)
		{
			++(array->valid_number);
		}
	}
}


uint8_t is_little_endian(void)
{
	union w
	{
		int  a;
		char b;
	} c;

	c.a = 1;
	return (c.b == 1);
}


void StrToHex(char* str, unsigned char* Hex)
{
	uint8_t i=0, cnt=0, data[20], j;
	char *p = str;

	while(*p != '\0')
	{
		if(*p>='0' && *p <='9')
		{
			data[cnt] = *p - '0';
		}
		else if(*p>='A' && *p <='Z')
		{
			data[cnt] = *p - 55;
		}
		else if(*p>='a' && *p <='z')
		{
			data[cnt] = *p - 87;
		}
		p++;
		cnt++;
	}

	for(i=0, j=0; i<cnt-1; i+=2, j++)
	{
		Hex[j] = data[i] << 4 | data[i+1];
	}
}


void get_mac_addr(void)
{
	struct ifreq ifreq;
	int sock = 0;
	sock = socket(AF_INET,SOCK_STREAM,0);
	if(sock < 0)
	{
		perror("error sock");
		return;
	}

	for(uint8_t j=0; j<g_st_app_conf.netcard_num; j++)
	{
	    strcpy(ifreq.ifr_name,if_name[j]);
	    if(ioctl(sock,SIOCGIFHWADDR,&ifreq) < 0)
        {
            perror("error ioctl");
            return;
        }
		memcpy((char *)g_st_app_conf.local_mac[j], ifreq.ifr_hwaddr.sa_data,6);
		printf("eth: %s, mac: %02x:%02x:%02x:%02x:%02x:%02x\n", if_name[j], g_st_app_conf.local_mac[j][0], g_st_app_conf.local_mac[j][1], g_st_app_conf.local_mac[j][2], g_st_app_conf.local_mac[0][3], g_st_app_conf.local_mac[j][4], g_st_app_conf.local_mac[j][5]);
		
	}

}
#if 0

int main(int argc, char *argv[])
{
            struct ifreq ifreq;
                int sock = 0;
                    char mac[32] = "";

                        if(argc < 2){
                                        printf("Usage: ./program_name NIC_name");
                                                return 1;
                                                    }

                            sock = socket(AF_INET,SOCK_STREAM,0);
                                if(sock < 0)
                                            {
                                                            perror("error sock");
                                                                    return 2;
                                                                        }

                                    strcpy(ifreq.ifr_name,argv[1]);
                                        if(ioctl(sock,SIOCGIFHWADDR,&ifreq) < 0)
                                                    {
                                                                    perror("error ioctl");
                                                                            return 3;
                                                                                }

                                            int i = 0;
                                                for(i = 0; i < 6; i++){
                                                                sprintf(mac+3*i, "%02X:", (unsigned char)ifreq.ifr_hwaddr.sa_data[i]);
                                                                    }
                                                    mac[strlen(mac) - 1] = 0;
                                                        printf("MAC: %s\n", mac);

                                                            return 0;
}




void get_mac_addr(void)
{
	int sock, if_count, i;
	struct ifconf ifc;
	struct ifreq ifr[10];
	unsigned char mac[6];

	memset(&ifc, 0, sizeof(struct ifconf));

	sock = socket(AF_INET, SOCK_STREAM, 0);

	ifc.ifc_len = 10 * sizeof(struct ifreq);
	ifc.ifc_buf = (char *)ifr;
	//获取所有网卡信息
	ioctl(sock, SIOCGIFCONF, (char *)&ifc);

	if_count = ifc.ifc_len / (sizeof(struct ifreq));
	for (i = 0; i < if_count; i++) 
	{		
		if (ioctl(sock, SIOCGIFHWADDR, &ifr[i]) == 0)
		{  

		    for(uint8_t j=0; j<g_st_app_conf.netcard_num; j++)
		    {
                if(strncmp(if_name[j], ifr[i].ifr_name,strlen(if_name[j])) == 0)
			    {
	                memcpy((char *)g_st_app_conf.local_mac[0], ifr[i].ifr_hwaddr.sa_data,6);
					
					printf("eth: %s, mac: %02x:%02x:%02x:%02x:%02x:%02x\n", ifr[i].ifr_name, g_st_app_conf.local_mac[0][0], g_st_app_conf.local_mac[0][1], g_st_app_conf.local_mac[0][2], g_st_app_conf.local_mac[0][3], g_st_app_conf.local_mac[0][4], g_st_app_conf.local_mac[0][5]);
					break;
				}
			}
		} 
	}
	return 0;
}
#endif 

static uint8_t read_router_string(char string[])
{
	char delim[] = ",:-\t ";
	char *p;
	uint16_t local_id;

	static uint8_t i = 0;
	uint8_t j = 0;

	char *s = strdup(string);
	printf("%2d, ",i);
	p = strsep(&s, delim);
	local_id = atoi(p);
	if(local_id != g_st_app_conf.local_id)
	{
        return 0;
	}
	else
	{
		g_st_app_conf.router_table[i][j] = atoi(p);
		j++;
	}
	
    while( (p = strsep(&s, delim)) != NULL )
    {
        g_st_app_conf.router_table[i][j] = atoi(p);
        j++;
		
    }
	g_st_app_conf.router_table_entry_num[i] = j;

	
	printf("\n");
	i++;
	g_st_app_conf.router_table_num = i;
	return 0;

}



uint8_t read_string(char string[], char data[20])
{
    char delim[] = ":-\t";
    char *p, text[20];
    uint8_t i, type = 0;
	string[strlen(string)-1]=0;

    char *s = strdup(string);

    p = strsep(&s, delim);
    if(0==strcmp(p, "if_name"))
    {   
        uint8_t j = 0;
        while( (p = strsep(&s, delim)) != NULL )
        {
            strcpy(data, p);
		
			// 只取字符串最前面的连续的字母和数字
			p = data;
			i = 0;
			while((*p >= '0' && *p <= '9') ||
				  (*p >= 'a' && *p <= 'z') ||
				  (*p >= 'A' && *p <= 'Z') )
			{
				p++;
				i++;
			}
			data[i] = '\0';
			type = 1;
			strcpy(if_name[j], data);
			j++;
        }

		g_st_app_conf.netcard_num = j;
       
    }
    else if(0==strcmp(p, "decap_thread_num"))
    {
        p = strsep(&s, delim);
		g_st_app_conf.decap_thread_num = atoi(p);
    }	
    else if(0==strcmp(p, "recv_thread_num"))
    {
        p = strsep(&s, delim);
		g_st_app_conf.recv_thread_num = atoi(p);
    }	
	else if(0==strcmp(p, "encap_thread_num"))
    {
        p = strsep(&s, delim);
		g_st_app_conf.encap_thread_num = atoi(p);
    }	
	else if(0==strcmp(p, "check_frag_timeout_period"))
    {
		p = strsep(&s, delim);
		g_st_app_conf.check_frag_timeout_period = atoi(p);
    }	
	else if(0==strcmp(p, "rcv_ring_queue_num"))
    {
		p = strsep(&s, delim);
		g_st_app_conf.rcv_ring_queue_num = atoi(p);
    }	
	else if(0==strcmp(p, "encap_ring_queue_num"))
    {
		p = strsep(&s, delim);
		g_st_app_conf.encap_ring_queue_num = atoi(p);
    }	
	else if(0==strcmp(p, "pcie_flag"))
    {
		p = strsep(&s, delim);
		g_st_app_conf.pcie_flag = atoi(p);
    }
	else if(0==strcmp(p, "padding_threshold"))
    {
		p = strsep(&s, delim);
		g_st_app_conf.padding_threshold = atoi(p);
    }	
	else if(0==strcmp(p, "modcode"))
    {
		p = strsep(&s, delim);
		g_st_app_conf.modcode = atoi(p);
    }
	else if(0 == strcmp(p, "dst_id"))
	{
        i = 0;
        while( (p = strsep(&s, delim)) != NULL )
        {
            g_st_app_conf.dst_id[i] = atoi(p);
            i++;
        }
	}

	else if(0 == strcmp(p, "dst_ip"))
	{
        i = 0;
		uint32_t ip;
		
        while( (p = strsep(&s, delim)) != NULL )
        {   
            convert_ipv4_val(p, &ip);
            g_st_app_conf.dst_ip[i] = rte_be_to_cpu_32(ip);
            i++;
        }
	}	
	else if(0==strcmp(p, "router_id"))
    {
		p = strsep(&s, delim);
		g_st_app_conf.router_id = atoi(p);
    }
	else if(0==strcmp(p, "cur_entry"))
    {
		p = strsep(&s, delim);
		g_st_app_conf.cur_entry = atoi(p);
    }
	else if(0==strcmp(p, "local_id"))
    {
		p = strsep(&s, delim);
		g_st_app_conf.local_id = atoi(p);
    }



	if( type > 2 && type < 8)
	{
		strcpy(data, s);
		int len = strlen(data);
		if(0x0a == data[len-1])	// 去掉末尾的换行符
		{
			data[len-1] = '\0';
		}
    }

	return type;
}

uint8_t mapping_table_entry_num = 0;
static uint8_t read_mapping_tbl_string(char string[])
{
    char delim[] = ",:-\t ";
    char *p, text[20];
	char data[20];
	uint16_t local_id;

	static uint8_t i = 0;
	

    char *s = strdup(string);//out_id
	printf("%2d, ",i);

	p = strsep(&s, delim);
	if(NULL != p)
	{
		local_id = atoi(p);
		if(local_id != g_st_app_conf.local_id)
		{
            return 0;
		}
		printf("%d:, ",g_st_app_conf.local_id);
	}

	p = strsep(&s, delim);
	if(NULL != p)
	{
		g_st_app_conf.next_hop_id[i] = atoi(p);
		printf("%d:, ",g_st_app_conf.next_hop_id[i]);
	}

	p = strsep(&s, delim);
	if(NULL != p)
	{
	     for(uint8_t j=0; j<g_st_app_conf.netcard_num; j++)
	     {
             if(0 == strncmp(p, if_name[j], strlen(if_name[j])))
             {
                 g_st_app_conf.port_id[i] = j;
		         printf("%d:, ",j);
				 break;
			 }
		 }
	}

    if(g_st_app_conf.next_hop_id[i] == g_st_app_conf.local_id)
	{
        memcpy((char *)g_st_app_conf.dst_mac[i], (char *)g_st_app_conf.local_mac[g_st_app_conf.port_id[i]], 6);
		g_st_app_conf.dst_mac_flag[i] = 3;
	    printf("dst mac:%d, mac: %02x:%02x:%02x:%02x:%02x:%02x\n", i,g_st_app_conf.dst_mac[i][0], g_st_app_conf.dst_mac[i][1], g_st_app_conf.dst_mac[i][2], g_st_app_conf.dst_mac[i][3], g_st_app_conf.dst_mac[i][4], g_st_app_conf.dst_mac[i][5]);
	}

	p = strsep(&s, delim);//eth type
	if(NULL != p)
	{
         g_st_app_conf.protocol_type[i] = strtol(p, NULL, 16);
		 printf("%s:, ",p);
	}

    p = strsep(&s, delim);//pkt type
	if(NULL != p)
	{
        if(0 == strcmp(p, "isl"))
        {
             g_st_app_conf.port_to_pkt_type[i] = ISL_PACK;
		}
		else if(0 == strcmp(p, "bb"))
        {
             g_st_app_conf.port_to_pkt_type[i] = BBFRAME_PACK;
		}
		else if(0 == strcmp(p, "m_fwd_pdu"))
        {
             g_st_app_conf.port_to_pkt_type[i] = MULTI_FWD_PDU_PACK;
		}
		else if(0 == strcmp(p, "m_rtn_pdu"))
        {
             g_st_app_conf.port_to_pkt_type[i] = MULTI_FWD_PDU_PACK;
		}
		else if(0 == strcmp(p, "fwd_pdu"))
        {
             g_st_app_conf.port_to_pkt_type[i] = FWD_TUNNEL_PDU;
		}
		else if(0 == strcmp(p, "rtn_pdu"))
        {
             g_st_app_conf.port_to_pkt_type[i] = RTN_TUNNEL_PDU;
		}
		else if(0 == strcmp(p, "fwd_bb"))
        {
             g_st_app_conf.port_to_pkt_type[i] = ENUM_FWD_Tx_FWD_Data;
		}
		printf("%s:, ",p);
	}

    //dst mac
	uint8_t j=0;
	j = 0;
    while( (p = strsep(&s, delim)) != NULL )
    {
        strcpy(&text[j], p);
        j += strlen(p);
		if(j == 12)
		{
            break;
		}
    }

	StrToHex(text, (uint8_t*)data);
	memcpy((char *)g_st_app_conf.dst_mac[i], data, 6);
	
	printf("\n");
    i++;
	mapping_table_entry_num = i;
	return 0;

}


void read_mapping_tbl_config(const char file_name[], char *dir)
{
    uint8_t loop_flag = 0;
    char buffer[1024]   = {0};
    memcpy(buffer, dir, strlen(dir));
	strcat(buffer, file_name);

    FILE* pFile = fopen(buffer, "r");
	if(pFile == NULL) {
		printf("Open file: %s failed, at %s line %d.\n", file_name, __FILE__, __LINE__);
		exit(-1);
	}
	
    char text[100];
	uint8_t type;

    while( NULL != fgets(text, 100, pFile) )
    {
        if(0 == loop_flag)
        {
            loop_flag = 1;
			continue;
		}
        type = read_mapping_tbl_string(text);
    }

    fclose(pFile);

}








/********************************************************************************
* 功能：判断两个MAC地址是否相同
* 参数：
 [in]	MAC1：要进行比较的MAC地址1
 [in]	MAC2：要进行比较的MAC地址2
 [in]	len： MAC地址字节数
* 返回值：
 1～相同		0～不同
********************************************************************************/
uint8_t equal_mac(const uint8_t MAC1[6], const uint8_t MAC2[6], int len)
{
	uint8_t ret = MAC_SAME;
	int i;

	for (i = 0; i < len; ++i)		// 比较当前PDU是否和之前的PDU的目的MAC地址相同
	{
		if (MAC1[i] != MAC2[i])
		{
			ret = MAC_DIFFERENCE;	// 目的MAC地址不同，返回错误码
			break;
		}
	}

	return ret;
}


void read_nic_config(const char file_name[], char *dir)
{
    char buffer[1024]   = {0};
    memcpy(buffer, dir, strlen(dir));
	strcat(buffer, file_name);

    FILE* pFile = fopen(buffer, "r");
	if(pFile == NULL) {
		printf("Open file: %s failed, at %s line %d.\n", file_name, __FILE__, __LINE__);
		exit(-1);
	}
	
    char text[100], data[20];
	uint8_t type;

    while( NULL != fgets(text, 100, pFile) )
    {
        type = read_string(text, data);
    }

    fclose(pFile);

	get_mac_addr();
}



void read_router_tbl_config(const char file_name[], char *dir)
{
    char buffer[1024]   = {0};
    memcpy(buffer, dir, strlen(dir));
	strcat(buffer, file_name);

    FILE* pFile = fopen(buffer, "r");
	if(pFile == NULL) {
		printf("Open file: %s failed, at %s line %d.\n", file_name, __FILE__, __LINE__);
		exit(-1);
	}
	
    char text[100];
	uint8_t type;

    while( NULL != fgets(text, 100, pFile) )
    {
        type = read_router_string(text);
    }

    fclose(pFile);

}






