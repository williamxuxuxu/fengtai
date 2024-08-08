#include "global_var.h"
#include "stats.h"
#include "common_dpdk.h"
#include "ring_queue.h"
struct rte_ring* rcv_ring[MAX_DEVICE_NUM];         	 
struct rte_ring* encap_ring[MAX_DEVICE_NUM];        // return  ring  for gse output
struct rte_ring* snd_bb_ring[MAX_DEVICE_NUM];        // return  ring  for gse output

#define RING_SIZE         8192U * 64


void create_ring(void)
{
    uint32_t i;
	char name[32] = {0};
    for(i=0; i<g_st_app_conf.rcv_ring_queue_num; i++)
    {
	    sprintf(name, "rcv_ring[%d]", i);
	    rcv_ring[i] = rte_ring_create(name, RING_SIZE, 0, 0);
	    if(rcv_ring[i] == NULL) 
		{
		    printf("Can not create ring fwd_gse_out[%d]\n", i);
		}
	}

	for(i=0; i<g_st_app_conf.encap_ring_queue_num; i++)
    {
	    sprintf(name, "encap_ring[%d]", i);
	    encap_ring[i] = rte_ring_create(name, RING_SIZE, 0, 0);
	    if(encap_ring[i] == NULL) 
		{
		    printf("Can not create ring fwd_gse_out[%d]\n", i);
	    }
	}

	for(i=0; i<g_st_app_conf.encap_ring_queue_num; i++)
    {
	    sprintf(name, "snd_bbframe_ring[%d]", i);
	    snd_bb_ring[i] = rte_ring_create(name, RING_SIZE, 0, 0);
	    if(snd_bb_ring[i] == NULL) 
		{
		    printf("Can not create ring fwd_gse_out[%d]\n", i);
	    }
	}
}

int enqueue_snd_bb_ring( struct rte_mbuf* mbuf, uint16_t ring_id)
{
	if( mbuf == NULL )
		return false;
	
	int ret = false;
	
	// Enqueue
	ret = rte_ring_enqueue(snd_bb_ring[ring_id], mbuf) == 0 ? true : false;
	if(ret)
	{
		ut_statis_debug.pkt_enqueue_snd_bb_ring_num[ring_id]++;
	}
	else
	{
        ut_statis_debug.drop_pkt_enqueue_snd_bb_ring_num[ring_id]++;
	}
		
	
	return ret;
}


int dequeue_snd_bb_ring(struct rte_mbuf** buf, uint32_t max_num, uint16_t ring_id)
{
	if(buf == NULL || max_num == 0 )
		return false;
	uint32_t pkt_dequeue = 0;
	
	pkt_dequeue =  rte_ring_dequeue_burst(snd_bb_ring[ring_id], (void **)buf, max_num, NULL);
	ut_statis_debug.pkt_dequeue_snd_bb_ring_num[ring_id] += pkt_dequeue;
	
	return pkt_dequeue;
}


int snd_bb_ring_is_null(uint16_t ring_id)
{
	if(rte_ring_empty(snd_bb_ring[ring_id]) == 1) 
	{ 
	    return 1;
	}
	else
	{
        return 0;
	}

}

int enqueue_encap_ring( struct rte_mbuf* mbuf, uint16_t ring_id)
{
	if( mbuf == NULL )
		return false;
	
	int ret = false;
	
	// Enqueue
	ret = rte_ring_enqueue(encap_ring[ring_id], mbuf) == 0 ? true : false;
	if(ret)
	{
		ut_statis_debug.pkt_enqueue_encap_ring_num[ring_id]++;
	}
	else
	{
        ut_statis_debug.drop_pkt_enqueue_encap_ring_num[ring_id]++;
	}
		
	
	return ret;
}

int dequeue_encap_ring(struct rte_mbuf** buf, uint32_t max_num, uint16_t ring_id)
{
	if(buf == NULL || max_num == 0 )
		return false;
	uint32_t pkt_dequeue = 0;
	
	pkt_dequeue =  rte_ring_dequeue_burst(encap_ring[ring_id], (void **)buf, max_num, NULL);
	ut_statis_debug.pkt_dequeue_encap_ring_num[ring_id] += pkt_dequeue;
	
	return pkt_dequeue;
}



int enqueue_rcv_ring( struct rte_mbuf* mbuf, uint16_t ring_id)
{
	if( mbuf == NULL )
		return false;
	
	int ret = false;
	
	// Enqueue
	ret = rte_ring_enqueue(rcv_ring[ring_id], mbuf) == 0 ? true : false;
	if(ret)
	{
		ut_statis_debug.pkt_enqueue_rcv_ring_num[ring_id]++;
	}
	else
	{
        ut_statis_debug.drop_pkt_enqueue_rcv_ring_num[ring_id]++;
	}
		
	
	return ret;
}

int dequeue_rcv_ring(struct rte_mbuf** buf, uint32_t max_num, uint16_t ring_id)
{
	if(buf == NULL || max_num == 0 )
		return false;
	uint32_t pkt_dequeue = 0;
	
	pkt_dequeue =  rte_ring_dequeue_burst(rcv_ring[ring_id], (void **)buf, max_num, NULL);
	ut_statis_debug.pkt_dequeue_rcv_ring_num[ring_id] += pkt_dequeue;
	
	return pkt_dequeue;
}



