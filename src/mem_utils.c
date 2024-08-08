#include "mem_utils.h"
void free_p_memory(void ** pp)
{
	void ** p = *pp;
	if (p == NULL)
	{
	    return;
	}

	free(p);
	*pp = NULL;
}

//  free dobule pointer without memory block nubmer
void free_pp_mem(void *** ppp)
{
	void ** pp = *ppp;
	if (pp == NULL)
	{
	    return;
	}

	free(pp);
	*ppp = NULL;
}


/*
void free_pp_memory(void*** ppp, size_t pSize){
	void** pp=*ppp;
	if(pp==NULL)
		return;
	S32 nNode=sizeof(pp)/pSize;
	S32 i = 0;
	for(i=0;i<nNode;i++)
		free_p_memory(&pp[i]);
	free(pp);
	*ppp=NULL;
}
*/

void free_pp_memory(void*** ppp, int nNum){

	void** pp=*ppp;
	
	if(pp==NULL)
		return;
	int i = 0;
	for(i=0;i<nNum;i++)
	{
		free_p_memory(&pp[i]);
	}
	
	free(pp);
	*ppp=NULL;
}


void* alloc_memory(size_t nSize){
	void* p;
	if(nSize <= 0)
		return NULL;
	p = (void*)malloc(nSize);
	if(p==NULL)
		return NULL;
	memset(p, 0, nSize);
	return p;
}

void* realloc_memory(void** pOrg, size_t nOrgSize, size_t nDestSize){
	void* p;
	p = (void*)malloc(nDestSize);
	if(p==NULL)
		return NULL;
	memset(p, 0, nDestSize);
	//  Copy the old data
	memcpy(p, *pOrg, nOrgSize);
	//  Release the old
	free(*pOrg);
	*pOrg = p;
	return p;
}

