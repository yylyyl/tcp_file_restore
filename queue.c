#include "main.h"

pthread_mutex_t drop_mutex = PTHREAD_MUTEX_INITIALIZER;

struct drop_queue {
	struct tuple4 addr;
	struct drop_queue *next;
};

struct drop_queue *dq_head = NULL;

int check_drop(struct tuple4 addr) {
	struct drop_queue *p = dq_head, *q = NULL;

	pthread_mutex_lock(&drop_mutex);
	while(p != NULL) {
		if(tuple4eq(p->addr, addr)) {
			if(q==NULL)
				dq_head = p->next;
			else
				q->next = p->next;

			free(p);
			pthread_mutex_unlock(&drop_mutex);
			return 1;
		}
		q = p;
		p = p->next;
	}
	pthread_mutex_unlock(&drop_mutex);
	return 0;
}

int if_drop(struct tuple4 addr) {
	struct drop_queue *p = dq_head;
	pthread_mutex_lock(&drop_mutex);
	while(p != NULL) {
		if(tuple4eq(p->addr, addr)) {
			pthread_mutex_unlock(&drop_mutex);
			return 1;
		}
		p = p->next;
	}
	pthread_mutex_unlock(&drop_mutex);
	return 0;
}

void add_drop(struct tuple4 addr) {
	if(if_drop(addr))
		return;

	struct drop_queue *p = (struct drop_queue *)malloc(sizeof(struct drop_queue));
	if(!p) {
		perror("cannot malloc");
		exit(1);
	}
	memset(p, 0, sizeof(struct drop_queue));
	p->addr = addr;

	pthread_mutex_lock(&drop_mutex);
	if(dq_head == NULL) {
		dq_head = p;
	} else {
		p->next = dq_head;
		dq_head = p;
	}
	pthread_mutex_unlock(&drop_mutex);
}
