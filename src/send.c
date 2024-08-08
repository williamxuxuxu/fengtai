#include <stdio.h>
#include <malloc.h>
#include <error.h>
#include <sys/ioctl.h>


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

//raw socket
#include <linux/if_packet.h>
#include <linux/if.h>

#include <sys/ioctl.h>

/*

char local_nic_name[20];
uint8_t src_mac[6];		// 本机MAC地址
uint8_t ether_header[14];
uint8_t dst_mac[6] = {0x68, 0x05, 0xCA, 0x88, 0x87, 0xF6};	// 目的MAC地址

void* send_by_raw_socket(struct rte_mbuf* mbuf)
{
    int ret = 0;
    int sock;	// RAW socket id
    struct sockaddr_ll dst_addr;
    struct ifreq ifr;
    struct rte_ether_hdr *eth_hdr;

	prctl(PR_SET_NAME, "send_by_raw_socket");


  	if ((sock = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_ALL))) < 0)
	{
		perror("socket");
		exit(1);
	}

   	memset(&ifr, 0, sizeof (ifr));
    strcpy (ifr.ifr_name, (const char*)local_nic_name);


	// 获取接口网卡MAC地址
    if (ioctl (sock, SIOCGIFHWADDR, &ifr) < 0) 
	{
        perror ("ioctl() failed to get source MAC address!");
        return 0;//(EXIT_FAILURE);
    }
	memcpy(src_mac, ifr.ifr_hwaddr.sa_data, 6);

	// 获取接口网卡索引号
    if (ioctl (sock, SIOCGIFINDEX, &ifr) < 0) 
	{
        perror ("ioctl() failed to get interface index");
        return 0;//(EXIT_FAILURE);
    }

    memset(&dst_addr, 0, sizeof(dst_addr));
	dst_addr.sll_family   = htons(PF_PACKET);
	dst_addr.sll_protocol = htons(ETH_P_ALL);
	dst_addr.sll_ifindex  = ifr.ifr_ifindex;
	dst_addr.sll_hatype	  = ARPHRD_ETHER;
	dst_addr.sll_pkttype  = PACKET_HOST;
	dst_addr.sll_halen	  = ETH_ALEN;
	memcpy(dst_addr.sll_addr, src_mac, ETH_ALEN);

    memcpy (ether_header, dst_mac, 6);
	
    memcpy (ether_header + 6, src_mac, 6);
    ether_header[12] = (ETH_P_DEAN & 0xFF00) >> 8;
    ether_header[13] = ETH_P_DEAN & 0x00FF;
    struct ether_hdr *eth_hdr;

    rte_memcpy((char *)eth_hdr, ether_header, 14);

	ret = sendto(sock, rte_pktmbuf_mtod(mbuf, uint8_t *), mbuf->data_len, 0, (struct sockaddr*)&dst_addr, sizeof(dst_addr));
	if(ret>0)
	{
		ut_statis_debug.send_count++;
		// printf("send_bytes:%d sequence_num:%d\n", ret, pItem->sequence_num);
	}
	else
	{
		printf("sendto fail\n");
	}
					

	return 0;
}*/

//raw socket


//PCIE
/*void send_pkt_to_specail_port(struct rte_mbuf* mbuf, uint16_t port_id)
{
    return;
}*/

//PCIE





