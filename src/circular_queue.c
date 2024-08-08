#include "circular_queue.h"
#include <string.h>
#include <stdlib.h>


#ifndef POINT_ERROR
#define POINT_ERROR		-128		// 指针错误：空指针
#endif

#ifndef SIZE_ERROR
#define SIZE_ERROR		-1			// 长度错误：长度与预期不符
#endif

#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wunused-value"
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"


void InitCircleQueue(CIRCULAR_QUEUE *queue, unsigned int nSize)
{
	queue->Head = -1;
	queue->Tail = -1;
	queue->QueueSize = 0;
	queue->QueueMaxSize = nSize;
	queue->pQueueStart = (Type*) malloc(queue->QueueMaxSize * sizeof(Type));
	memset(queue->pQueueStart, 0, queue->QueueMaxSize * sizeof(Type));
}

unsigned char IsFull(CIRCULAR_QUEUE *queue)
{
	unsigned char ret = false;

	if (queue && (queue->QueueSize >= queue->QueueMaxSize))
	{
		queue->QueueSize = queue->QueueMaxSize;
		ret = true;
	}

	return ret;
}

unsigned char IsEmpty(CIRCULAR_QUEUE *queue)
{
	unsigned char ret = false;

	if (queue && queue->QueueSize == 0)
	{
		ret = true;
	}

	return ret;
}

int Push(CIRCULAR_QUEUE *queue, Type data)
{
	if (!queue)
	{
		return POINT_ERROR;
	}

	if (!IsFull(queue))
	{
		if (-1 == queue->Head)
		{
			queue->Head++;
			queue->Head %= queue->QueueMaxSize;
		}
		queue->Tail++;		// 环形队列未满，tail最大只到(QueueMaxSize-1)，不需要取模
		if((unsigned int)queue->Tail >= queue->QueueMaxSize)
		{
			queue->Tail = 0;
		}
		queue->QueueSize++;
	}
	else if (IsFull(queue))	// 环形队列满，Head和Tail都需要移动
	{
		queue->Head++;
		queue->Tail++;

//		queue->Head %= queue->QueueMaxSize;
//		queue->Tail %= queue->QueueMaxSize;

 		if((unsigned int)queue->Tail >= queue->QueueMaxSize)
 		{
 			queue->Head = 1;
 			queue->Tail = 0;
 		}
 		else
 		{
 			queue->Head++;
 			queue->Tail++;
 		}
	}

	((Type*) queue->pQueueStart)[queue->Tail] = data;
	return 0;
}

void Pop(CIRCULAR_QUEUE *queue)
{
	if (queue)
	{
		if (!IsEmpty(queue))
		{
			queue->QueueSize--;

			if(queue->QueueSize > 0)	// 如果环形队列长度>0，队首指针要移动一位
			{
				queue->Head++;
				if((unsigned int)queue->Head >= queue->QueueMaxSize)
		 		{
		 			queue->Head = 0;
		 		}
			}
			else
			{
				queue->Head = -1;
				queue->Tail = -1;
			}
		}
		else
		{
			queue->Head = -1;
			queue->Tail = -1;
		}
	}
}

void Clear(CIRCULAR_QUEUE *queue)
{
	if (queue)
	{
		memset(queue->pQueueStart, 0, queue->QueueMaxSize * sizeof(Type));
		queue->QueueSize = 0;
		queue->Head = -1;
		queue->Tail = -1;
	}
}

int GetSize(CIRCULAR_QUEUE* queue)
{
	if(!queue)
		return -1;

	return queue->QueueSize;
}

void* GetElement(CIRCULAR_QUEUE* queue, unsigned int pos)
{
	void *p = NULL;
	Type *pType = NULL;
	int len = 0;

	if(!queue)
		return NULL;

	if ( queue->QueueMaxSize>0 && pos <= queue->QueueMaxSize)
	{
		len = (queue->Head + pos) % queue->QueueMaxSize;
		pType = (Type*) queue->pQueueStart + len;
		p = (void*) pType;
	}

	return (void*) p;
}

void SetElement(CIRCULAR_QUEUE *queue, unsigned int pos, Type data)
{
	Type *pType = NULL;

	if (queue && pos <= queue->QueueMaxSize)
	{
		pType = (Type*) queue->pQueueStart + (queue->Head + pos) % queue->QueueMaxSize;
		*(pType) = data;
	}
}


void TraverseQueue(CIRCULAR_QUEUE* queue)
{
	unsigned int pos;
	Type *pType = NULL;
	if(!queue)
		return;
	// printf("TraverseQueue QueueSize:%d  ", queue->QueueSize);
	for(pos=0; pos<queue->QueueSize; ++pos)
	{
		pType = (Type*) queue->pQueueStart + (queue->Head + pos) % queue->QueueMaxSize;
		// printf("Pos:%d value:%d ", pos, *pType);
	}
	// printf("\n");
}

#undef Type
