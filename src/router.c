#include "router.h"
#include "define_const.h"
#include "define_struct.h"
#include "general_module.h"
#include "sys_thread.h"
#include "global_var.h"
#include "stats.h"
#include "system_log.h"
#include "ring_queue.h"
#include "tun.h"
#include "common_dpdk.h"
#include "ether.h"
#include "receive.h"
#include "rcv_send.h"

ROUTERHEADER router_header[MAX_DEVICE_NUM];
const uint8_t ethzero[RTE_ETHER_ADDR_LEN] = {0, 0, 0, 0, 0, 0};
const uint8_t ethbroadcast[RTE_ETHER_ADDR_LEN] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

int arp_raw(uint16_t port_id, const uint8_t *ethsrc_addr,
        const uint8_t *ethdst_addr,
        const uint8_t *hwsrc_addr, const uint32_t src_router_id,
        const uint8_t *hwdst_addr, const uint32_t dst_router_id,
        const uint16_t opcode)
{
    struct rte_ether_hdr *eth_hdr;
    struct rte_arp_hdr *arp_hdr;
    struct rte_arp_router_id *arp_data;
    struct rte_mbuf *m;

    m = get_rcv_mbuf();	
    if(m == NULL){
         printf("Failed to alloc mbuf for arp request.\n");
         return -1;
    }

    arp_hdr = (struct rte_arp_hdr *)rte_pktmbuf_prepend(m, sizeof(struct rte_arp_hdr));
    eth_hdr = (struct rte_ether_hdr *)rte_pktmbuf_prepend(m, sizeof(struct rte_ether_hdr));

    memcpy(eth_hdr->s_addr.addr_bytes, ethsrc_addr, RTE_ETHER_ADDR_LEN);
    memcpy(eth_hdr->d_addr.addr_bytes, ethdst_addr, RTE_ETHER_ADDR_LEN);
    eth_hdr->ether_type = rte_cpu_to_be_16(RTE_ETHER_TYPE_ARP);

    arp_hdr->arp_hardware = rte_cpu_to_be_16(RTE_ARP_HRD_ETHER);
    arp_hdr->arp_protocol = rte_cpu_to_be_16(RTE_ETHER_TYPE_ROUTER_ID);
    /* set hwlen and protolen */
    arp_hdr->arp_hlen = RTE_ETHER_ADDR_LEN;
    arp_hdr->arp_plen = sizeof(uint32_t);

    arp_hdr->arp_opcode = rte_cpu_to_be_16(opcode);

    arp_data = &arp_hdr->arp_data;
    /* Write the ARP MAC-Addresses */

    memcpy(arp_data->arp_sha.addr_bytes, hwsrc_addr, RTE_ETHER_ADDR_LEN);
    memcpy(arp_data->arp_tha.addr_bytes, hwdst_addr, RTE_ETHER_ADDR_LEN);
    arp_data->arp_s_router_id = src_router_id;
    arp_data->arp_t_router_id = dst_router_id;

	snd_pkt(rte_pktmbuf_mtod(m, char *), m->data_len,  port_id);
	rte_pktmbuf_free(m);

    return 0;
}

int arp_request_dst(uint16_t id)
{
    int result = 0;

    result = arp_raw(g_st_app_conf.port_id[id], g_st_app_conf.local_mac[g_st_app_conf.port_id[id]], ethbroadcast,
                     g_st_app_conf.local_mac[g_st_app_conf.port_id[id]], rte_cpu_to_be_32(g_st_app_conf.local_id), ethzero,
                     rte_cpu_to_be_32(g_st_app_conf.next_hop_id[id]), RTE_ARP_OP_REQUEST);

    return result;
}

extern uint8_t mapping_table_entry_num;

static void arp_recv(uint16_t port_id, struct rte_mbuf *m)
{
    uint16_t router_id;
	uint8_t mapping_table_index = 0;

    struct rte_ether_hdr *eth_hdr;
    struct rte_arp_hdr *arp_hdr;
    struct rte_arp_router_id *arp_data;


    eth_hdr = rte_pktmbuf_mtod(m, struct rte_ether_hdr *);
    arp_hdr = rte_pktmbuf_mtod_offset(m, struct rte_arp_hdr *, sizeof(struct rte_ether_hdr));
    arp_data = &arp_hdr->arp_data;
    router_id = rte_be_to_cpu_32(arp_data->arp_t_router_id);

    if(router_id != g_st_app_conf.local_id) {
        printf("arp_recv: find interface entry failed! tip=%d\n", router_id);
        rte_pktmbuf_free(m);
        return;
    }

    /* RFC 826 "Packet Reception": */
    if ((arp_hdr->arp_hardware != rte_be_to_cpu_16(RTE_ARP_HRD_ETHER)) ||
        (arp_hdr->arp_hlen != MAC_ADDR_BYTE_LEN) ||
        (arp_hdr->arp_plen != sizeof(uint32_t)) ||
        (arp_hdr->arp_protocol != rte_be_to_cpu_16(RTE_ETHER_TYPE_ROUTER_ID)))  {
        // RTE_LOG(WARNING, ARP, "etharp_input: packet dropped, wrong hw type, hwlen, proto, protolen or ethernet type (%u u% %u %u)\n");

        rte_pktmbuf_free(m);
        return;
    }



    /* We only do ARP reply when:
     * 1. tip is me.
     */
    if (arp_hdr->arp_opcode == rte_be_to_cpu_16(RTE_ARP_OP_REQUEST) )
	{

        eth_hdr->d_addr = eth_hdr->s_addr;
        memcpy(eth_hdr->s_addr.addr_bytes, g_st_app_conf.local_mac[port_id], MAC_ADDR_BYTE_LEN);
        arp_hdr->arp_opcode = rte_cpu_to_be_16(RTE_ARP_OP_REPLY);
        uint32_t t_router_id = arp_data->arp_t_router_id;
        arp_data->arp_t_router_id = arp_data->arp_s_router_id;
        arp_data->arp_s_router_id = t_router_id;

        arp_data->arp_tha = arp_data->arp_sha;
        memcpy(arp_data->arp_sha.addr_bytes, g_st_app_conf.local_mac[port_id], MAC_ADDR_BYTE_LEN);

       
		snd_pkt(rte_pktmbuf_mtod(m, char *), m->data_len,  port_id);

    }
	else
	{
	    uint32_t s_router_id = rte_be_to_cpu_32(arp_data->arp_s_router_id);
	    for(mapping_table_index=0; mapping_table_index<mapping_table_entry_num; mapping_table_index++)
	    {
	        if(s_router_id == g_st_app_conf.next_hop_id[mapping_table_index])
	        {
	            break;
			}
		}
		
       	memcpy(g_st_app_conf.dst_mac[mapping_table_index], &eth_hdr->s_addr, 6);
	    printf("dst mac:%d, mac: %02x:%02x:%02x:%02x:%02x:%02x\n", mapping_table_index,g_st_app_conf.dst_mac[mapping_table_index][0], g_st_app_conf.dst_mac[mapping_table_index][1], g_st_app_conf.dst_mac[mapping_table_index][2], g_st_app_conf.dst_mac[mapping_table_index][3], g_st_app_conf.dst_mac[mapping_table_index][4], g_st_app_conf.dst_mac[mapping_table_index][5]);
	
	    g_st_app_conf.dst_mac_flag[mapping_table_index] = 3;
	}
	
    rte_pktmbuf_free(m);
}


int arp_in (uint16_t port_id, struct rte_mbuf *mbuf)
{
    // struct rte_ether_hdr *eth_hdr;
    struct rte_arp_hdr *arp_hdr;
    //struct rte_arp_ipv4 *arp_data;
    //struct rte_arp_hdr *arp_pkt;
//  struct ether_hdr *eth;

//  eth = rte_pktmbuf_mtod(mbuf, struct ether_hdr *);
    arp_hdr  = (struct rte_arp_hdr*) (rte_pktmbuf_mtod(mbuf, unsigned char *) + sizeof(struct rte_ether_hdr));



    //dump_arp_pkt(arp_hdr);
//   printf("arp packet with opcode %u %u\n", arp_pkt->opcode, ntohs(arp_pkt->opcode));
    switch(ntohs(arp_hdr->arp_opcode)) {
        case RTE_ARP_OP_REQUEST ://
            arp_recv(port_id, mbuf);
            //write_pcap_file(1,rte_pktmbuf_mtod(mbuf, uint8_t *), mbuf->data_len);
            break;

        case RTE_ARP_OP_REPLY ://
            arp_recv(port_id, mbuf);
            break;
    }
    return 0;
}


uint16_t get_mapping_index(uint16_t cur_id, uint16_t last_id, uint16_t next_hop_id)
{

    (void)last_id;

	if(cur_id == g_st_app_conf.local_id)
	{
       /* if(cur_id == last_id)
        {
            printf("manager or control data\n");
			return 255;
		}
		else*/
		{
		    uint16_t i = 0;

			for(i=0; i<MAX_DEVICE_NUM; i++)
			{
                if(next_hop_id == g_st_app_conf.next_hop_id[i])
                {

					return i;
				}
			}
		}
	}
	
	LOG(LOG_ERR, LOG_MODULE_ROUTER, LOG_ERR,
	        LOG_CONTENT("router failed"));

	return 0;
	
}

void init_router_header(Pack_Direction direction)
{
	for(uint8_t i = 0; i < g_st_app_conf.router_table_num; i++)
	{
        router_header[i].CUR_ENTRY = 	g_st_app_conf.cur_entry;
	    router_header[i].LAST_ENTRY = g_st_app_conf.router_table_entry_num[i] - 1;
	    router_header[i].D = direction;
	    router_header[i].M = 0;
	    router_header[i].TTL = g_st_app_conf.router_table_entry_num[i] + 1;

	    /*uint32_t *ptmp;
	    ptmp = (uint32_t*)router_header;
	    *ptmp = htonl(*ptmp);*/
	
	    for(uint8_t j = 0; j < g_st_app_conf.router_table_entry_num[i]; j++)
	    {
            router_header[i].segment[j] = htons(g_st_app_conf.router_table[i][j]);
	    }

	    g_st_app_conf.router_header_size[i] = ROUTER_HEADER_LEN + sizeof(uint16_t)*g_st_app_conf.router_table_entry_num[i];
	}
}

uint16_t get_dst_id(struct rte_mbuf* pdu_mbuf)
{
    ROUTERHEADER *router_header;
    router_header = rte_pktmbuf_mtod(pdu_mbuf, ROUTERHEADER *);
	for(uint8_t i = 0; i<= router_header->LAST_ENTRY; i++)
	{
        router_header->segment[i] = ntohs(router_header->segment[i]);
    }

	return router_header->segment[router_header->LAST_ENTRY];
}

uint16_t decide_router_table(struct rte_mbuf* pdu_mbuf)
{
    uint16_t dst_id;
	uint8_t last_entry;
	dst_id = get_dst_id(pdu_mbuf);
	for(uint8_t i = 0; i<g_st_app_conf.router_table_num; i++)
	{
	    last_entry = g_st_app_conf.router_table_entry_num[i] - 1;
        if(dst_id == g_st_app_conf.router_table[i][last_entry])
        {
            return i;
		}
		
	}

	LOG(LOG_ERR, LOG_MODULE_DEFAULT, LOG_ERR,
	        LOG_CONTENT("decide_router_table: don't find corresponding router table"));

	return 0;
}

#if 0
int decide_router_table_by_dst_ip(struct rte_mbuf* pdu_mbuf)
{
    uint16_t dst_id;
	uint8_t last_entry;
	struct ipv4_hdr *ip_hdr;
	uint32_t dst_ip;
	
	ip_hdr = rte_pktmbuf_mtod_offset(pdu_mbuf, struct ipv4_hdr *, sizeof(struct rte_ether_hdr));
    dst_ip = ntohl(ip_hdr->dst_addr);
	for(uint8_t j = 0; j < MAX_DEVICE_NUM; j++)
	{
        if(dst_ip == g_st_app_conf.dst_ip[j])
        {
            dst_id = g_st_app_conf.dst_id[j];
            for(uint8_t i = 0; i<g_st_app_conf.router_table_num; i++)
			{
			    last_entry = g_st_app_conf.router_table_entry_num[i] - 1;
		        if(dst_id == g_st_app_conf.router_table[i][last_entry])
		        {
		            return i;
				}
				
			}
		}
	}




	//printf("don't find dst ip\n");

return -1;
}
#else
int decide_router_table_by_dst_id(void)
{
    uint16_t dst_id;
	uint8_t last_entry;
	
    dst_id = g_st_app_conf.dst_id[0];
    for(uint8_t i = 0; i<g_st_app_conf.router_table_num; i++)
	{
	    last_entry = g_st_app_conf.router_table_entry_num[i] - 1;
        if(dst_id == g_st_app_conf.router_table[i][last_entry])
        {
            return i;
		}
		
	}
		
	




	//printf("don't find dst ip\n");

return -1;
}
#endif



void router_func(struct rte_mbuf* pdu_mbuf)
{
    uint16_t mapping_index;//will have a mapping between queue id and device id
	//uint32_t *ptmp;
	ROUTERHEADER *router_header;


	router_header = rte_pktmbuf_mtod(pdu_mbuf, ROUTERHEADER *);
	/*ptmp = (uint32_t *)(router_header);
	*ptmp = ntohl(*ptmp);*/
	//printf("router");
	for(uint8_t i = 0; i<= router_header->LAST_ENTRY; i++)
	{
        router_header->segment[i] = ntohs(router_header->segment[i]);
		//printf(":%d",router_header->segment[i]);
    }
	//printf("\n");
 
    //uint16_t router_header_segment_size = sizeof(uint16_t)*(router_header->LAST_ENTRY + 1);
	//uint16_t router_header_size = ROUTER_HEADER_LEN + sizeof(uint16_t)*(router_header->LAST_ENTRY + 1);

	uint16_t cur_id = router_header->segment[router_header->CUR_ENTRY];
	uint16_t last_id = router_header->segment[router_header->LAST_ENTRY];
	uint16_t next_hop_id = 0;
	if(router_header->CUR_ENTRY == router_header->LAST_ENTRY)
	{
        next_hop_id = router_header->segment[router_header->CUR_ENTRY];
	}
	else
	{
        next_hop_id = router_header->segment[router_header->CUR_ENTRY + 1];
	}

	//printf("cur_id:%d,last_id:%d,next_hop_id:%d\n",cur_id,last_id,next_hop_id);
	
    mapping_index = get_mapping_index(cur_id, last_id, next_hop_id);

    if(255 == mapping_index)
	{

	    uint16_t router_header_size = ROUTER_HEADER_LEN + sizeof(uint16_t)*(router_header->LAST_ENTRY + 1);
		uint16_t header_len = router_header_size + sizeof(struct fwd_fixed_tunnel);
		rte_pktmbuf_adj(pdu_mbuf, header_len);
	    sendToVtnl(rte_pktmbuf_mtod(pdu_mbuf, char *), pdu_mbuf->data_len);
        rte_pktmbuf_free(pdu_mbuf);
		return;
	}

    router_header->CUR_ENTRY = router_header->CUR_ENTRY + 1;
	

	for(uint8_t i = 0; i<= router_header->LAST_ENTRY; i++)
	{
        router_header->segment[i] = htons(router_header->segment[i]);
    }

	/*ptmp = (uint32_t *)(router_header);
     *ptmp = htonl(*ptmp);*/


	if(false == enqueue_encap_ring(pdu_mbuf, mapping_index))
	{
		rte_pktmbuf_free(pdu_mbuf);
		LOG(LOG_ERR, LOG_MODULE_ROUTER, LOG_ERR,
	        LOG_CONTENT("enqueue_encap_ring failed"));
	}



    //rte_pktmbuf_adj(pdu_mbuf, router_header_size);
	//write_pcap_file(1,rte_pktmbuf_mtod(pdu_mbuf, uint8_t *), pdu_mbuf->data_len);

	
	
	return;
}

