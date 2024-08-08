// list_template.cpp : Defines the entry point for the console application.
//

#include "list_template.h"
#include <stdio.h>
#include <stdlib.h>

/********************************************************************************
* 功能：   创建链表，生成链表的头节点（该节点为空节点，不存储数据，只作为链表存在的标志）
* 参数：   无
* 返回值：	指向新建链表的第一个节点Node0的指针
********************************************************************************/
LINKLIST* Init_List(void)
{
	LINKLIST *pList = (LINKLIST*)malloc(sizeof(LINKLIST));

	pList->head = (LINKNODE*)malloc(sizeof(LINKNODE));
	pList->head->pre = NULL;
	pList->head->next = NULL;
	pList->tail = NULL;
	pList->size = 0;
	pList->data_bytes = 0;

	return pList;
}

/********************************************************************************
* 功能：   在链表pos的后一个位置插入新增的Node
* 参数：
 pList:	指向链表的指针
 pos:	指明在pos的后一个位置插入Node,pos>=0
 data:	要插入的数据指针
* 返回值：	0～成功  小于0～错误码
********************************************************************************/
char Insert_Node(LINKLIST *pList, unsigned int pos, void *data)
{
	LINKNODE *pNewNode = NULL;
	LINKNODE *pCurrent = NULL;

	unsigned int i = 0;

	if (pList == NULL || data == NULL)
		return -1;

	if (pos > pList->size)       // 不合法位置，插入到尾部。
	{
		pos = pList->size;
	}

	// 生成一个孤立的新node
	pNewNode = (LINKNODE*) malloc(sizeof(LINKNODE));
	pNewNode->data = data;
	pNewNode->pre = NULL;
	pNewNode->next = NULL;

	// 把遍历的起点设为头节点，找到node(pos)
	pCurrent = pList->head;

	for (i = 0; i < pos; i++)
	{
		pCurrent = pCurrent->next;
	}

	// 将node(pos+1)链接到新node
	pNewNode->next = pCurrent->next;
	pCurrent->next->pre = pNewNode;

	// 将node(pos)链接到新node
	pNewNode->pre = pCurrent;
	pCurrent->next = pNewNode;

	pList->size++;

	if(pos == pList->size)
	{
		pList->tail = pNewNode;
	}

	return 0;
}

/********************************************************************************
* 功能：   在链表pos的后一个位置插入存在的Node
* 参数：
 pList:	指向链表的指针
 pos:	指明在pos的后一个位置插入Node，pos=0表示在当前node1之前插入
 node:	要插入的节点指针
* 返回值：	0～成功  小于0～错误码
********************************************************************************/
char Insert_Exist_Node(LINKLIST *pList, unsigned int pos, LINKNODE *node)
{
	LINKNODE *pNewNode = node;
	LINKNODE *pCurrent = NULL;

	unsigned int i = 0;

	if(pList == NULL || node == NULL)
		return -1;

	if(pos > pList->size)       // 不合法位置，插入到尾部。
	{
		pos = pList->size;
	}

	// 把遍历的起点设为头节点，找到node(pos)
	pCurrent = pList->head;

	for(i = 0; i < pos; i++)
	{
		pCurrent = pCurrent->next;
	}

	// 将node(pos+1)链接到新node
	pNewNode->next = pCurrent->next;
	if (pos < pList->size)
		pCurrent->next->pre = pNewNode;

	// 将node(pos)链接到新node
	pNewNode->pre = pCurrent;
	pCurrent->next = pNewNode;

	pList->size++;

	if(pos == pList->size)
	{
		pList->tail = pNewNode;
	}
	return 0;
}

/********************************************************************************
* 功能：   链表移除pos位置的Node（pos>0），但不释放这个node
* 参数：
 [in]	pList:	指向链表的指针
 [in]	pos:	要移除的Node位置
 * 返回值：	要移除的Node的指针
********************************************************************************/
LINKNODE* Remove_Node_pos(LINKLIST *pList, unsigned int pos)
{
	LINKNODE *pDel = NULL;
	LINKNODE *pCurrent = NULL;
	LINKNODE *pPrevious = NULL;

	unsigned int i = 0;

	if (NULL == pList || pos < 1 || pos > pList->size)
		return NULL;

	pCurrent = pList->head;
	pPrevious = pList->head;

	// 从头节点开始遍历，找到pCurrent=node(pos)，pPrevious=node(pos-1)
	for (i = 0; i < pos; i++)
	{
		pPrevious = pCurrent;
		pCurrent = pCurrent->next;
	}

	pDel = pCurrent;

	// node(pos-1)链接到原来的node(pos+1)
	pPrevious->next = pCurrent->next;
	if (NULL != pCurrent->next)
	{
		pCurrent->next->pre = pPrevious;
	}
	else
	{
		pList->tail = (pPrevious == pList->head) ? NULL : pPrevious; 
//		pList->tail = NULL;
	}
	pList->size--;
	pList->data_bytes -= pDel->data_bytes;

	return pDel;
}

/********************************************************************************
* 功能：   从链表中移除指定地址的节点，但不释放这个node
* 参数：
 [in]		pList:	指向链表的指针
 [inout]	pNode:	要移除的Node的指针
* 返回值：	0～成功  小于0～错误码
********************************************************************************/
char Remove_Node_point(LINKLIST *pList, LINKNODE *pNode)
{
	LINKNODE *pNext = NULL;
	LINKNODE *pPre = NULL;

	if (NULL == pList || NULL == pNode)
		return -1;

	pNext = pNode->next;
	pPre = pNode->pre;

	// node(pos-1)链接到原来的node(pos+1)
	if(pNext)
	{
		pNext->pre = pPre;
	}
	else		// 要出队的节点是最后一个
	{
		pPre->next = NULL;
		pList->tail = pPre;
	}

	if(pNode != pList->head)	// pNode不是头节点
	{
		pPre->next = pNext;
	}
	else		// pNode是头节点
	{
		pList->head = pNext;
	}

	pList->size--;
	pList->data_bytes -= pNode->data_bytes;
	pNode->next = NULL;

	return 0;
}

char Delete_Node_pos(LINKLIST *pList, unsigned int pos, uint8_t del_data)
{
	char ret = -1;
	LINKNODE *pDel = Remove_Node_pos(pList, pos);
	
	if (pDel)
	{
		if(del_data)
		{
			free(pDel->data);
			pDel->data = NULL;
		}

		free(pDel);
		pDel = NULL;

		ret = 0;
	}

	return ret;
}

char Delete_Node_point(LINKLIST *pList, LINKNODE *pNode)
{
	char ret = Remove_Node_point(pList, pNode);
	if (0 == ret)
		free(pNode);

	return ret;
}


int64_t Get_List_Byte(LINKLIST *pList)
{
	if (pList == NULL)
		return -1;

	return pList->data_bytes;
}


/********************************************************************************
* 功能：  获取链表大小
* 参数：
 	 	 pList:	指向链表的指针
* 返回值：链表的节点个数;-1表示错误
********************************************************************************/
int Get_List_Size(LINKLIST *pList)
{
	if (pList == NULL)
		return -1;

	return pList->size;
}

/********************************************************************************
* 功能：  获取链表中第pos个node的数据
* 参数：
		 pList:	指向链表的指针
		 pos:	要查询的Node位置
* 返回值：链表中第pos个node的指针
********************************************************************************/
void* Get_Node(LINKLIST *pList, unsigned int pos)
{
	LINKNODE *pCurrent = NULL;
	unsigned int i = 0;

	if (pList == NULL)
		return NULL;

	pCurrent = pList->head;

	for (i = 0; i < pos && NULL!=pCurrent; i++)
	{
		pCurrent = pCurrent->next;
	}

	return pCurrent;
}


void* Get_Header_Data(LINKLIST *pList, LINKNODE** pNode)
{
	LINKNODE *pCurrent = NULL;

	if (pList == NULL)
		return NULL;

	pCurrent = pList->head->next;
	*pNode = pCurrent;

	if(!pCurrent)
		return 0;
	else
		return (void*)(pCurrent->data);
}


/********************************************************************************
* 功能：  释放链表的内存
* 参数：
 	 	 pList:	指向链表的指针
* 返回值：无
********************************************************************************/
void Free_List(LINKLIST *pList, uint8_t del_data)
{
	LINKNODE *pCurrent = NULL;
	LINKNODE *pNext = NULL;

	if (pList == NULL)
		return;

	// free non-head(s)
	if(pList->head != NULL) {
		pCurrent = pList->head->next;

		while (pCurrent) {
			pNext = pCurrent->next;       // 缓存下一个结点

			if(del_data && pCurrent->data)
			{
				free(pCurrent->data);
			}	
			free(pCurrent);
			pCurrent = pNext;
		}
	}

	// free head
	if(pList->head)
	{
		free(pList->head);
	}

	free(pList);
}
void Clear_List(LINKLIST *pList)
{
	LINKNODE *pCurrent = NULL;
	LINKNODE *pNext = NULL;

	if (pList == NULL || pList->head == NULL)
		return;

	pCurrent = pList->head->next;

	while (pCurrent)
	{
		pNext = pCurrent->next;       // 缓存下一个结点
		//free(pCurrent->data);
		free(pCurrent);
		pCurrent = pNext;
	}

    pList->data_bytes = 0;
	pList->size = 0;
    pList->head->next = NULL;
    pList->head->pre = NULL;
    pList->tail = NULL;
}

/********************************************************************************
* 功能：  遍历链表的内存
* 参数：
 	 	 pList:	指向链表的指针
* 返回值：无
********************************************************************************/
int Traversal_List(LINKLIST *pList)
{
	LINKNODE *pCurrent;
	unsigned short *data = 0;
	int size = 0;

	(void)data;

	pCurrent = pList->head;
	if(!pList || !pList->head)
	{
		return POINT_ERROR;
	}

	pCurrent = pCurrent->next;
	while (pCurrent)
	{
		size++;
		data = (unsigned short*) (pCurrent->data);
		pCurrent = pCurrent->next;
		printf("data:%p value:%d\n", data, *data);
	}

	return size-1;
}

/********************************************************************************
* 功能：  链表中查询满足数据条件的node
* 参数：
 	 	 pList:	指向链表的指针
 	 	 data:	要检索的数据
* 返回值：>0～成功，检索到的node在链表中的位置  0～没找到满足条件的node  -1～错误码
********************************************************************************/
int Find_List(LINKLIST *pList, void *data)
{
	LINKNODE *pCurrent = NULL;
	int i = 0;
	uint8_t OK = FALSE;

	if (pList == NULL || data == NULL)
		return -1;

	pCurrent = pList->head;

	// 遍历查找
	while (pCurrent)
	{
		i++;
		if (pCurrent->data == data)
		{
			OK = TRUE;
			break;
		}
		pCurrent = pCurrent->next;
	}

	if (!OK)
		i = 0;

	return i;
}


int Push_List(LINKLIST *pList, LINKNODE* pNode)
{
	int ret = -1;

	if(NULL != pList)
	{
		if(NULL == pList->tail)		// 链表为空，只有（表示链表存在的）头节点
		{
			pList->head->next = pNode;
			pNode->pre = pList->head;
		}
		else
		{
			pNode->pre = pList->tail;
			pList->tail->next = pNode;
		}

		pNode->next = NULL;
		pList->tail = pNode;
		pList->size++;
		pList->data_bytes += pNode->data_bytes;

		ret = 0;
	}

	return ret;
}


int Linked_List(LINKLIST *pDestList, LINKLIST* pSrcList)
{
	int ret = -1;
	int queue_size = 0;

	if(NULL == pDestList || NULL == pSrcList)
	{
		return POINT_ERROR;
	}

	queue_size = Get_List_Size(pSrcList);
	if(0==queue_size)
	{
		return SIZE_ERROR;
	}

	if(NULL == pDestList->tail)		// 链表DestList为空，只有（表示链表存在的）头节点
	{
		// 确保DestList的长度信息一致
		pDestList->size = 0;
		pDestList->data_bytes = 0;

		pSrcList->head->next->pre = pDestList->head;			// SrcList的第一个node链接到DestList的head
		pDestList->head->next = pSrcList->head->next;
	}
	else
	{
		pSrcList->head->next->pre = pDestList->tail;			// SrcList的第一个node链接到pDestList的tail
		pDestList->tail->next = pSrcList->head->next;
	}

	pDestList->tail = pSrcList->tail;
	pDestList->size += pSrcList->size;
	pDestList->data_bytes += pSrcList->data_bytes;

	pSrcList->tail->next = 0;	// 连接完成后，把队尾的next指针指向空
	pSrcList->size = 0;
	pSrcList->data_bytes = 0;
	pSrcList->tail = NULL;
	pSrcList->head->next = NULL;

	ret = 0;
	return ret;
}




LINKNODE* Pop_List(LINKLIST *pList)
{
	LINKNODE* pNode = NULL;

	if(NULL != pList)
	{
		if(pList->size > 1)
		{
			pNode = pList->head->next;						// 取出第一个节点
			pList->head->next = pList->head->next->next;	// 把原第二个节点置为新第一个节点
			pList->head->next->pre = pList->head;			// 新第一个节点的pre节点是head节点
			pList->size--;
		}
		else if(1==pList->size)
		{
			pNode = pList->head->next;						// 取出第一个节点
			pList->head->next = NULL;						// 原只有一个节点
			pList->tail->pre = NULL;
			pList->tail->next = NULL;
			pList->tail = NULL;
			pList->size--;
		}
		else
		{
			pNode = NULL;
		}
	}

	return pNode;
}
