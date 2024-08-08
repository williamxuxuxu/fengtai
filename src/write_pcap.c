

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <sys/queue.h>
#include <stdarg.h>
#include <errno.h>
#include <getopt.h>
#include <time.h>
#include <sys/time.h>

#include <netinet/in.h>
#include <linux/if.h>
#include <linux/if_tun.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <signal.h>

#if 0
#define PCAP_MAGIC 0xa1b2c3d4

typedef struct _Pcaprec_Hdr_s 
{ 
    uint32_t ts_sec;         /* timestamp seconds */
    uint32_t ts_usec;        /* timestamp microseconds */
    uint32_t incl_len;       /* number of octets of packet saved in file */
    uint32_t orig_len;       /* actual length of packet */
}Pcaprec_Hdr_s;

/* "libpcap" file header (minus magic number). */
typedef struct _Pcap_Hdr 
{
    uint16_t    version_major;    /* major version number */
    uint16_t    version_minor;    /* minor version number */
    int32_t    thiszone;    /* GMT to local correction */
    uint32_t    sigfigs;    /* accuracy of timestamps */
    uint32_t    snaplen;    /* max length of captured packets, in octets */
    uint32_t    network;    /* data link type */
}Pcap_Hdr;

__thread uint32_t gtui_time_last = 0;
__thread FILE* gtf_file_handle = NULL;
__thread uint32_t   gtui_pkt_counter = 0;
__thread uint8_t  gtauc_file_name[128] = {0};

int write_pcap_file(uint8_t id,uint8_t *pc_data, uint16_t us_len)
{
    uint32_t    ui_magic = PCAP_MAGIC;
    time_t timep;
    Pcap_Hdr       st_pcap_hdr = {0};
    Pcaprec_Hdr_s  st_pcaprec_hdr = {0};
    struct timeval curr_time = {0};
    struct tm      pst_tm;


    if ((NULL == pc_data) || (us_len < 1))
    {
        return -1;
    }

    if(gtf_file_handle == NULL)
    {
        time(&timep);
        gmtime_r(&timep, &pst_tm);

        memset(gtauc_file_name, 0x00, sizeof(gtauc_file_name));
        snprintf(gtauc_file_name, sizeof(gtauc_file_name) - 1, "./%04d%02d%02d%02d%02d%02d_%03d.pcap",
                    1900 + pst_tm.tm_year, 1 + pst_tm.tm_mon, pst_tm.tm_mday,
                    pst_tm.tm_hour, pst_tm.tm_min, pst_tm.tm_sec,
                    id);


        gtf_file_handle = fopen(gtauc_file_name, "a+");
        if (NULL == gtf_file_handle)
        {
            return -1;
        }
        gtui_pkt_counter = 0 ;

        if (1 != fwrite(&ui_magic,sizeof(unsigned int), 1, gtf_file_handle))
        {
            fclose(gtf_file_handle);
            gtf_file_handle = NULL;
            return -1;
        }

        st_pcap_hdr.version_major   = 2;
        st_pcap_hdr.version_minor   = 4;
        st_pcap_hdr.thiszone        = 0;
        st_pcap_hdr.sigfigs         = 0;
        st_pcap_hdr.snaplen         = 65535;    //maximum number of bytes perpacket that will be captured
        st_pcap_hdr.network         = 1;        //Ethernet, and Linux loopback devices

        if (1 != fwrite(&st_pcap_hdr, sizeof(st_pcap_hdr), 1, gtf_file_handle))
        {
            fclose(gtf_file_handle);
            gtf_file_handle = NULL;
            return -1;
        }

    }

    fseek(gtf_file_handle, 0, SEEK_END);

    gettimeofday(&curr_time,NULL);

    st_pcaprec_hdr.ts_sec   = curr_time.tv_sec;
    st_pcaprec_hdr.ts_usec  = curr_time.tv_usec;
    st_pcaprec_hdr.incl_len = us_len;
    st_pcaprec_hdr.orig_len = us_len;
    if (1 != fwrite(&st_pcaprec_hdr, sizeof(st_pcaprec_hdr), 1, gtf_file_handle))
    {
        fclose(gtf_file_handle);
        gtf_file_handle = NULL;
        return -1;
    }

    if (1 != fwrite(pc_data, us_len, 1, gtf_file_handle))
    {
        fclose(gtf_file_handle);
        gtf_file_handle = NULL;
        return -1;
    }

    fflush(gtf_file_handle);

    gtui_pkt_counter++;
    if(gtui_pkt_counter >= 6000)
    {
       fclose(gtf_file_handle);
       gtf_file_handle = NULL;
       gtui_pkt_counter = 0 ;
    }

    return 0;
}

#endif

