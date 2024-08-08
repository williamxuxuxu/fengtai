#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/un.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/stat.h> 
#include <arpa/inet.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include <linux/if_tun.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h> // for close

#include "tun.h"


struct VTnlInfo g_tnl;


int recvFromVtnl(char *buf, int len)
{
	int nread = read(g_tnl.tun_fd, buf, len);
	if (nread < 0) {
		perror("Reading from interface");
		return -1;
	}
	return nread;
}


void sendToVtnl(char *buf, int len)
{
	int ret = -1;

	if (NULL == buf) {
		return;
	}

	errno = 0;
	printf("send to vtnl\n");
	#if 0
	printf("send to vtnl [%s:%d]\n",inet_ntoa(g_tnl.tnlAddr.sin_addr),ntohs(g_tnl.tnlAddr.sin_port));
	ret = sendto(g_tnl.toVtnlsd, buf, len, 0, (struct sockaddr *)&(g_tnl.tnlAddr), sizeof(g_tnl.tnlAddr));
	if(ret == -1){
		printf("send to vtnl failed,ret = %d!,errorno is %d, err is %s \n",ret,errno,strerror(errno));
		return;
	}
	#endif
	ret = write(g_tnl.tun_fd, buf, len);
    (void)ret;
	return;
}

int setNonblocking(int fd, int enable)
{
  int flags;
  if ((flags = fcntl(fd, F_GETFL, 0)) < 0) {
    perror("");
    return -1;
  }
  // 开启或关闭
  if (enable == 0)  {
  	  flags &= ~O_NONBLOCK;
  }  else {
      flags |= O_NONBLOCK;
  }

  if (fcntl(fd, F_SETFL, flags) < 0) {
    perror("");
    return -1;
  }
  return 0;
}


int tun_alloc(void)
{
    struct ifreq ifr;
    int fd, err;
    const char *clonedev = "/dev/net/tun";

    if ((fd = open(clonedev, O_RDWR)) < 0) {
		printf("open fail\n");
        return fd;
    }

    memset(&ifr, 0, sizeof(ifr));
    ifr.ifr_flags = IFF_TUN | IFF_NO_PI;
	const char *dev = "tun100";
	strncpy(ifr.ifr_ifrn.ifrn_name, dev, IFNAMSIZ);

    if ((err = ioctl(fd, TUNSETIFF, (void *) &ifr)) < 0) {
        close(fd);
        return err;
    }
	
	setNonblocking(fd, 0);

	printf("Open tun/tap device: %s for reading...\n", ifr.ifr_name);

    return fd;
}


uint32_t get_tunnel_hdr(int pri, int proto, int user_id)
{
    return (pri << RTN_TUNNEL_PRI_SHIFT | proto << RTN_TUNNEL_PROTO_SHIFT | user_id);
}



int initVtnl(void)
{
	g_tnl.tnlAddr.sin_addr.s_addr = inet_addr("192.0.0.204");

	int port = atoi("2222");

	g_tnl.tnlAddr.sin_port = htons(port);//设置接收方端口号
	g_tnl.tnlAddr.sin_family  = AF_INET; //使用IPv4协议
	
    g_tnl.tun_fd = tun_alloc();
    if (g_tnl.tun_fd < 0) {
        perror("Allocating interface");
        exit(1);
    }

	return 0;
}

