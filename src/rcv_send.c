
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/ip.h>
#include <netinet/in.h>
#include <netinet/udp.h>
#include <memory.h>
#include <stdlib.h>
#include <linux/if_ether.h>
#include <linux/if_packet.h> // sockaddr_ll
#include <arpa/inet.h>
#include <netinet/if_ether.h>
#include <errno.h>
#include <netinet/ether.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <pthread.h>



#include "stats.h"
#include "common_dpdk.h"
#include "rcv_send.h"
#include "receive.h"
#include "global_var.h"


#define BUF_SIZE (8192)

extern char if_name[MAX_NETCARD_NUM][32];


struct fwd_fds {
    int from_sock;
    int to_sock;
    int index;
};
int stati_recv[2] = {0, 0};
int stati_send[2] = {0, 0};
int stati_error[2] = {0, 0};


static int set_promisc(char *ifname, int enable) 
{
        int sock, err;
        struct ifreq ifr;
        
        if ((sock = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_IP))) < 0)
        {
            printf("socket error\n");
            return -1;
        }
    
        memset(&ifr, 0, sizeof(ifr));
        strncpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name));
    
        // get the index number of the interface
        err = ioctl(sock, SIOCGIFINDEX, &ifr);
        if (err < 0) {
            printf("SIOCGIFINDEX error\n");
            close(sock);
            return -1;
        }
         if ((err = ioctl(sock, SIOCGIFFLAGS, &ifr))<0)
        {
            printf("SIOCGIFFLAGS error\n");
            close(sock);
            return-1;
        }
        if (enable) {
            ifr.ifr_flags |= IFF_PROMISC;
        }
        else {
            ifr.ifr_flags &= (~IFF_PROMISC);
        }
        if ((err = ioctl(sock, SIOCSIFFLAGS, &ifr)) < 0){
            printf("IFF_PROMISC error\n");
            close(sock);
            return -1;
        }
        printf("Set interface %s promisc %s\n", ifr.ifr_name, enable?"on":"off");
        close(sock);
        
        return 0;

}
/* return -1 when error */
static int open_socket(char *ifname)
{
		int sock, err, on = 1;
		struct ifreq ifr;
		struct sockaddr_ll addr;
		int nRcvBufferLen = 4*1024*1024;
		int nSndBufferLen = 4*1024*1024;
		int optval;
		unsigned int optlen = sizeof(unsigned int);
		//static uint8_t flag = 0;
		//if ((sock = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_IP))) < 0)
		if ((sock = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_ALL))) < 0)
		{
			printf("socket error\n");
			return -1;
		}
	
		// bind to device
		memset(&ifr, 0, sizeof(ifr));
		strncpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name));
	
		// get the index number of the interface
		err = ioctl(sock, SIOCGIFINDEX, &ifr);
		if (err < 0) {
			printf("SIOCGIFINDEX error\n");
			close(sock);
			return -1;
		}
	
		//printf("init: IFRIDX ifr_ifindex=%d\n",ifr.ifr_ifindex);
		//ifidx = ifr.ifr_ifindex;
	
		memset(&addr, 0, sizeof(struct sockaddr_ll));
		addr.sll_family = AF_PACKET; /*PF_PACKET*/
		addr.sll_protocol = htons(ETH_P_ALL);
		//addr.sll_protocol = htons(ether_type);
		addr.sll_ifindex = ifr.ifr_ifindex;
	
		err = bind(sock,(struct sockaddr *) &addr,
				   sizeof(struct sockaddr_ll));
		if (err < 0) {
			printf("bind error\n");
			close(sock);
			return -1;
		}
	#if 0
		err = setsockopt(sock, SOL_PACKET, PACKET_IGNORE_OUTGOING, &on, sizeof(int));
		if(err != 0)
		{
			printf("setsockopt PACKET_IGNORE_OUTGOING error, %s\n", strerror(errno));
		}
	#endif
	//	  err = getsockopt(sock, SOL_SOCKET, SO_SNDBUF, &optval, &optlen);
	//	  if(err != 0)
	//	  {
	//		  printf("getsockopt SO_SNDBUF error, %s\n", strerror(errno));
	//	  }
	//	  else {
	//		  printf("SO_SNDBUF optval = %d\n", optval);
	//	  }
	//	  err = getsockopt(sock, SOL_SOCKET, SO_RCVBUF, &optval, &optlen);
	//	  if(err != 0)
	//	  {
	//		  printf("getsockopt SO_RCVBUF error, %s\n", strerror(errno));
	//	  }
	//	  else {
	//		  printf("SO_RCVBUF optval = %d\n", optval);
	//	  }
		
		err = setsockopt(sock, SOL_SOCKET, SO_SNDBUF, (char*)&nSndBufferLen, sizeof(int));
		if(err != 0)
		{
			printf("setsockopt SO_SNDBUF error, %s\n", strerror(errno));
		}
		err = setsockopt(sock, SOL_SOCKET, SO_RCVBUF, (char*)&nRcvBufferLen, sizeof(int));
		if(err != 0)
		{
			printf("setsockopt SO_RCVBUF error, %s\n", strerror(errno));
		}
		err = getsockopt(sock, SOL_SOCKET, SO_SNDBUF, &optval, &optlen);
		if(err != 0)
		{
			printf("getsockopt SO_SNDBUF error, %s\n", strerror(errno));
		}
		else {
			printf("SO_SNDBUF optval = %d\n", optval);
		}
		err = getsockopt(sock, SOL_SOCKET, SO_RCVBUF, &optval, &optlen);
		if(err != 0)
		{
			printf("getsockopt SO_RCVBUF error, %s\n", strerror(errno));
		}
		else {
			printf("SO_RCVBUF optval = %d\n", optval);
		}
		//set nonblocking
		#if 0
		if(0 == flag)
		{
		    err = fcntl(sock, F_SETFL, fcntl(sock, F_GETFD, 0)|O_NONBLOCK);
		    if (err != 0) {
			    printf("fcntl O_NONBLOCK error, %s\n", strerror(errno));	
		    } 
			flag++;
		}
		#endif
	
		if ((err = ioctl(sock, SIOCGIFFLAGS, &ifr))<0)
		{
			printf("SIOCGIFFLAGS error\n");
			close(sock);
			return-1;
		}
			
		if(ifr.ifr_flags & IFF_RUNNING) {
			printf("eth %s link up\n", ifname);
		}
		else {
			printf("eth %s link down\n", ifname);
		}
		return sock;
}

static int close_socket(int sock)
{
    close(sock);
    return 0;
}


int fds[MAX_NETCARD_NUM];

void init_rcv_snd(void)
{

    for(uint8_t i=0; i<g_st_app_conf.netcard_num-1; i++)
    {
        fds[i] = open_socket(if_name[i]);
	    if (fds[i] == -1) {
	        perror("open_socket error\n");
			close_socket(fds[i]);
	        exit(-1);
	    }
	}


//	int cflags = fcntl(sock0,F_GETFL,0);

//    fcntl(sock0,F_SETFL, cflags|O_NONBLOCK);

//	int cflags2 = fcntl(sock1,F_GETFL,0);

 //   fcntl(sock1,F_SETFL, cflags2|O_NONBLOCK);
 
    for(uint8_t i=0; i<g_st_app_conf.netcard_num-1; i++)
    {
       set_promisc(if_name[i], 1);
	}

//	set_promisc(if_name[1], 1);

}

#if 0
struct rte_mbuf* rcv_pkts(uint8_t sock_id)
{
     ssize_t pkt_len;
	 char *src_pkt = NULL;
	 struct rte_mbuf* mbuf;

	// #define BUF_SIZE (8000)
	// uint8_t pkt[BUF_SIZE];
	 struct sockaddr_ll client;
	 socklen_t  addr_length;
	 addr_length = sizeof( struct sockaddr_ll);
	 mbuf = get_rcv_mbuf();
	 if(NULL != mbuf)
	 { 
	   
	    pkt_len = recvfrom(fds[sock_id].from_sock, rte_pktmbuf_mtod(mbuf, char *), BUF_SIZE, 0, (struct sockaddr *)&client, (socklen_t*)&addr_length);
	     
		if(PACKET_OUTGOING == client.sll_pkttype)
        {
            ut_statis_debug.test_pkt_num[sock_id]++;
            rte_pktmbuf_free(mbuf);
			return NULL;
        }
		
		if(pkt_len > 0)
		{
	        src_pkt = (char *)rte_pktmbuf_append(mbuf, pkt_len);
		}
		else
		{
            rte_pktmbuf_free(mbuf);
			(void)src_pkt;
			return NULL;
		}
	  
	 }
	 
	 return mbuf;
}
#endif

void snd_pkt(char *pkt, uint32_t pkt_len, uint8_t sock_id)
{
	uint32_t send_len;

	while (1)
	{
	    send_len = send(fds[sock_id], pkt, pkt_len, 0);
	    if (send_len != pkt_len)
	    {
		    printf("recv_len %d send_len %d port id %d, error:%s\n", pkt_len, send_len, sock_id, strerror(errno));
		    ut_statis_debug.resent_pkt_num++;
			usleep(10);
			continue;
        }
	    else 
	    {
	        ut_statis_debug.sent_pkt_num[sock_id]++;
			break;
        }
	}

}








