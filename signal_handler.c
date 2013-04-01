#include "main.h"

void signal_handler(int i) {
	if(exiting) {
		return;
	}
	exiting = true;
	nids_exit();
	// add close file to the tail of queue
	pthread_mutex_lock(&q_mutex);
	struct threads *p = t_head;
	while(p != NULL) {
		if(q_tail == NULL) {
				q_tail = (struct queue *) malloc(sizeof(struct queue));
			if(q_tail == NULL) {
				fprintf(stderr, "Cannot malloc q_head.\n");
				exit(1);
			}
		} else {
			q_tail->next = (struct queue *) malloc(sizeof(struct queue));
			if(q_tail->next == NULL) {
				fprintf(stderr, "Cannot malloc q_tail.\n");
				exit(1);
			}
			q_tail = q_tail->next;
		}
		memset(q_tail, 0, sizeof(struct queue));
		q_tail->addr = p->addr;
		q_tail->inout = 4;
		p = p->next;
	}
	pthread_mutex_unlock(&q_mutex);
	// exit controller;
	pthread_mutex_lock(&ctrl_mutex);
	ctrl_exit = true;
	pthread_mutex_unlock(&ctrl_mutex);
	pthread_cond_signal(&ctrl_cond);
	fprintf(stderr, "Waiting for controller to exit...\n");
	pthread_join(ctrl_t, NULL);
	fprintf(stderr, "Bye...\n");
	exit(1);
}
