//
// Created by paulp on 3/6/2019.
//

#ifndef CSC138LAB2_QUEUE_H
#define CSC138LAB2_QUEUE_H

#include <semaphore.h>

typedef void (*destructor)(void*);

typedef struct mtQueueNode mtQueueNode;
struct mtQueueNode
{
	mtQueueNode *next;
	mtQueueNode *prev;
	void* value;
};

typedef struct
{
	mtQueueNode *head;
	mtQueueNode *tail;
	int size;
	sem_t mutex;
} MTQueue;

//create, destroy, and get size of queue
//creates the queue
MTQueue*	mtQueueCreate();
//value destructor is called with the value of the queue node to destroy it. Passing NULL to valueDestructor does nothing
void		mtQueueDestroy(MTQueue* queue, destructor valueDestructor);//only do this if you know that no other threads have a pointer to this queue
//returns the size of the queuemay not give you the correct value if another thread enqueues or dequeues something
int			mtQueueSize(MTQueue* queue);
//enqueue & dequeue
void		enqueueHead(MTQueue* queue, void* value);
void		enqueueTail(MTQueue* queue, void* value);
void*		dequeueHead(MTQueue* queue);
void*		dequeueTail(MTQueue* queue);


#endif //CSC138LAB2_QUEUE_H
