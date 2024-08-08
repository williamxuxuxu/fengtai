#include "router_libcap.h"

#include <stdio.h>
#include <inttypes.h>
#include <stdbool.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>

// includes for socket
#include <arpa/inet.h>
#include <linux/if_ether.h>
#include <linux/if.h>
#include <linux/if_packet.h> // sockaddr_ll
#include <sys/ioctl.h>
#include <rte_memcpy.h>

// includes for adaptation layer
#include <rte_mbuf.h>


#include <pcap.h>

#include "receive.h"
#include "stats.h"
#include "global_var.h"



pcap_t *pcap_fd[MAX_NETCARD_NUM];
char if_name[MAX_NETCARD_NUM][32] = {0};


#define FWD_PROTOCOL 9101
#define RTN_PROTOCOL 9103

#if 0
static pcap_t * reate_pcap_fd(char *card_name, uint8_t block_flag)
{
	char errbuf[PCAP_ERRBUF_SIZE] = {0};
	pcap_t * pcap = NULL;
	int i_ret;

	// Open tap on Device
	pcap = pcap_open_live(card_name, 65535, 0, 1, errbuf); // All-size, disable-promiscuous
	if(pcap == NULL) {
		printf("pcap_open_live() failed\n");
		return NULL;
	}
	#if 1

	struct bpf_program bpf_program;
	char filter[128] = "ether proto 0x9103 or (ether proto 0x9101 and greater 100) or ether proto 0x0800 or ether proto 0x0806";
#if 1
	// Set filter
	if(pcap_compile(pcap, &bpf_program, filter, 1, PCAP_NETMASK_UNKNOWN) != 0) { // failed
		printf("pcap_compile() failed\n");
		pcap_close(pcap);
		return NULL;
	}

	// Enable filter
	if(pcap_setfilter(pcap, &bpf_program) != 0) { // failed
		printf("pcap_setfilter() failed\n");
		pcap_close(pcap);
		return NULL;
	}
#endif
	// Packets ingress
	if(pcap_setdirection(pcap, PCAP_D_IN) != 0) { // failed
		printf("pcap_setdirection() failed\n");
		pcap_close(pcap);
		return NULL;
	}

	if(block_flag == 1)
	{
	    i_ret = pcap_getnonblock(pcap,errbuf);
        if (-1 == i_ret)
        {
            return NULL;
        }
        else if (0 == i_ret)
        {
            // set in capture mode:0 block,1 non_block
            i_ret = pcap_setnonblock(pcap,1,errbuf);
            if (-1 == i_ret)
            {
                return NULL;
            }
        }
	}
	#endif

	return pcap;
}

#else
static pcap_t *  reate_pcap_fd(char *card_name, uint8_t block_flag)
{
    char ebuf[PCAP_ERRBUF_SIZE] = {0};
	pcap_t * pd = NULL;
	int i_ret;
	int status = 0;
	int snaplen = 65535;
	static uint8_t iface = 0;
    pd = pcap_create(card_name, ebuf);
		if (pd == NULL)
			printf("%s: pcap_create failed: %s\n", card_name, ebuf);
		status = pcap_set_snaplen(pd, snaplen);
		if (status != 0)
			printf("%s: pcap_set_snaplen failed: %s\n",
			    card_name, pcap_statustostr(status));

		status = pcap_set_timeout(pd, 1);
		if (status != 0)
			printf("%s: pcap_set_timeout failed: %s\n",
			    card_name, pcap_statustostr(status));

		status = pcap_set_buffer_size(pd, 200*1024*1024);
		if (status != 0)
			printf("%s: pcap_set_buffer_size failed: %s\n",
			    card_name, pcap_statustostr(status));
		struct bpf_program bpf_program;
	char filter[128] = "ether proto 0x9101 and greater 6000";

	status = pcap_activate(pd);
	if (status < 0) {
		/*
		 * pcap_activate() failed.
		 */
		printf("%s: %s\n(%s)", card_name,
		    pcap_statustostr(status), pcap_geterr(pd));
	} else if (status > 0) {
		/*
		 * pcap_activate() succeeded, but it's warning us
		 * of a problem it had.
		 */
		printf("%s: %s\n(%s)\n", card_name,
		    pcap_statustostr(status), pcap_geterr(pd));
	} else
		printf("%s opened successfully\n", card_name);

		#if 1
	if(0 == iface)
	{
	// Set filter
	if(pcap_compile(pd, &bpf_program, filter, 1, PCAP_NETMASK_UNKNOWN) != 0) { // failed
		printf("pcap_compile() failed\n");
		pcap_close(pd);
		return NULL;
	}

	// Enable filter
	if(pcap_setfilter(pd, &bpf_program) != 0) { // failed
		printf("pcap_setfilter() failed\n");
		pcap_close(pd);
		return NULL;
	}
	iface++;
	}
#endif
	// Packets ingress
	if(pcap_setdirection(pd, PCAP_D_IN) != 0) { // failed
		printf("pcap_setdirection() failed\n");
		pcap_close(pd);
		return NULL;
	}
	if(block_flag == 1)
	{
	    i_ret = pcap_getnonblock(pd,ebuf);
        if (-1 == i_ret)
        {
            return NULL;
        }
        else if (0 == i_ret)
        {
            // set in capture mode:0 block,1 non_block
            i_ret = pcap_setnonblock(pd,1,ebuf);
            if (-1 == i_ret)
            {
                return NULL;
            }
        }
	}

	return pd;

}
#endif


void init_libpcap(void)
{
    for(uint8_t i=0; i<g_st_app_conf.netcard_num-1; i++)
    {
       pcap_fd[i] = reate_pcap_fd(if_name[i], 0);
	}
}


struct rte_mbuf* read_pcap(uint8_t index)
{
	 struct rte_mbuf* mbuf;
	 struct pcap_pkthdr pcap_hdr;
     const uint8_t* data = pcap_next(pcap_fd[index], &pcap_hdr);
	 uint16_t len;
     if(data != NULL) 
	 { 
	 	
         //write_pcap_file(10,data, pcap_hdr.caplen);
			
	     #if 0
		  	     if(index == 0)
	     {
            len = (uint16_t)pcap_hdr.caplen;
			if(1512 == len)
			{
                ut_statis_debug.fwd_pdu_num++;
			}
			else
			{
                return NULL;
			}
		 }
		 #endif
	     mbuf = get_rcv_mbuf();	 
		 if(NULL != mbuf)
	     { 
			uint8_t* start = (uint8_t*)rte_pktmbuf_append(mbuf, (uint16_t)pcap_hdr.caplen);
			if(start == NULL) 
			{  
			    rte_pktmbuf_free(mbuf);
			    return NULL;
		    }
			rte_memcpy(start, data, (size_t)pcap_hdr.caplen);
		 }
	}
	else
	{
		return NULL;
	}

	 
	 return mbuf;
}


void show_pcap_state(void)
{
    struct pcap_stat ps;

	for(uint8_t i=0; i<g_st_app_conf.netcard_num-1; i++)
    {
       	pcap_stats(pcap_fd[i], &ps);
	    printf("%d ps_recv, %d ps_drop, %d ps_ifdrop\n",
			ps.ps_recv, ps.ps_drop, ps.ps_ifdrop);
	}
}
