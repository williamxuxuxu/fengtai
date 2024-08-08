#ifndef CIRCULAR_QUEUE_H
#define CIRCULAR_QUEUE_H


#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif



//typedef unsigned char	bool;

#define false 0
#define true  1


typedef struct _CIRCULAR_QUEUE
{
	unsigned int QueueSize;      // 循环队列中实际存在的成员个数
	unsigned int QueueMaxSize;   // 循环队列的成员最大个数
	int   Head;                  // 循环队列的队首
	int   Tail;                  // 循环队列的队尾
	void* pQueueStart;			 // 循环队列缓冲区的首地址

}CIRCULAR_QUEUE;


//#define Type short
typedef short Type;



void InitCircleQueue(CIRCULAR_QUEUE* queue, unsigned int nSize);
unsigned char IsFull(CIRCULAR_QUEUE* queue);
unsigned char IsEmpty(CIRCULAR_QUEUE* queue);
int Push(CIRCULAR_QUEUE* queue, Type data);
void Pop(CIRCULAR_QUEUE* queue);
void Clear(CIRCULAR_QUEUE* queue);
int  GetSize(CIRCULAR_QUEUE* queue);
void* GetElement(CIRCULAR_QUEUE* queue, unsigned int pos);
void  SetElement(CIRCULAR_QUEUE* queue, unsigned int pos, Type data);
void  TraverseQueue(CIRCULAR_QUEUE* queue);


#ifdef __cplusplus
}
#endif



#endif // CIRCULAR_QUEUE_H
