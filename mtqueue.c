
#include <stdlib.h>
#include "mtqueue.h"


MTQueue* mtQueueCreate()
{
	MTQueue* result = malloc(sizeof(MTQueue));
	result->head = result->tail = NULL;
	result->size = 0;
	sem_init(&result->mutex,0,1);
	return result;
}
//value destructor is called with the value of the queue node to destroy it. Passing NULL to valueDestructor does nothing
void mtQueueDestroy(MTQueue* queue, destructor valueDestructor)
{
	sem_destroy(&queue->mutex);

	mtQueueNode* it = queue->head;
	while (it!=NULL)
	{
		mtQueueNode* current = it;
		it=it->next;
		if (valueDestructor!=NULL)
			(*valueDestructor)(current->value);
		free(current);
	}
	free(queue);
}

//returns the size of the queuemay not give you the correct value if another thread enqueues or dequeues something
int mtQueueSize(MTQueue* queue)
{
	return queue->size;
}

//enqueue & dequeue
void enqueueHead(MTQueue* queue, void* value)
{
	//make&init the node
	mtQueueNode* curr = malloc(sizeof(mtQueueNode));
	curr->next = curr->prev = NULL;
	curr->value=value;

	//do insertion
	sem_wait(&queue->mutex);

	if (queue->head==NULL)
	{
		queue->head = queue->tail = curr;
		queue->size = 1;
	}
	else
	{
		curr->next = queue->head;
		queue->head->prev = curr;
		queue->head = curr;
		queue->size++;
	}
	//CS done
	sem_post(&queue->mutex);
}
void		enqueueTail(MTQueue* queue, void* value)
{
	//make&init the node
	mtQueueNode* curr = malloc(sizeof(mtQueueNode));
	curr->next = curr->prev = NULL;
	curr->value=value;

	//do insertion
	sem_wait(&queue->mutex);

	if (queue->tail==NULL)
	{
		queue->tail = queue->head = curr;
		queue->size = 1;
	}
	else
	{
		curr->prev = queue->tail;
		queue->tail->next = curr;
		queue->tail = curr;
		queue->size++;
	}
	//CS done
	sem_post(&queue->mutex);
}
void*		dequeueHead(MTQueue* queue)
{
	mtQueueNode* removed;

	sem_wait(&queue->mutex);
	//CS begin

	if (queue->head==NULL)
	{
		removed = NULL;
	}
	else if (queue->head==queue->tail)
	{
		removed = queue->head;
		queue->head = queue->tail = NULL;
		queue->size = 0;
	}
	else
	{
		removed = queue->head;
		queue->head = removed->next;
		queue->head->prev=NULL;
		queue->size--;
	}
	//CS done
	sem_post(&queue->mutex);

	if (removed==NULL)
	{
		return NULL;
	}
	else
	{
		void* result = removed->value;
		free(removed);
		return result;
	}
}
void*		dequeueTail(MTQueue* queue)
{
	mtQueueNode* removed;

	sem_wait(&queue->mutex);
	//CS begin

	if (queue->tail==NULL)
	{
		removed = NULL;
	}
	else if (queue->head==queue->tail)
	{
		removed = queue->tail;
		queue->head = queue->tail = NULL;
		queue->size = 0;
	}
	else
	{
		removed = queue->tail;
		queue->tail = removed->prev;
		queue->tail->next=NULL;
		queue->size--;
	}
	//CS done
	sem_post(&queue->mutex);

	if (removed==NULL)
	{
		return NULL;
	}
	else
	{
		void* result = removed->value;
		free(removed);
		return result;
	}
}



















