#include "main.h"
static int n = 0;

void* controller_thread(void *arg) {
	fprintf(stderr, "Controller thread %u start.\n", (unsigned int)pthread_self());
	pthread_mutex_lock(&q_mutex);
	while(1) {
		//fprintf(stderr, "Controller: check queue.\n");
		struct queue *p = q_head, *prev_q = q_head;
		bool new_thread = false;
		while(p != NULL) {
			//fprintf(stderr, "Controller: queue not empty.\n");
			static struct queue tmp_q;
			bool delete = false;
			memcpy(&tmp_q, p, sizeof(struct queue));
			pthread_mutex_unlock(&q_mutex);
			// copy it, so that we can lock at fast as we can.
			// find thread.
			pthread_mutex_lock(&t_mutex);
			struct threads *q = t_head, *prev_t;
			while(q != NULL) {
				if(memcmp(&q->addr, &tmp_q.addr, sizeof(struct tuple4))==0)
					break;
				prev_t = q;
				q = q->next;
			}
			if(q == NULL && tmp_q.inout < 2) {
				// thread not found and is exchanging data. create a thread!
				new_thread = true;
				q = (struct threads *) malloc(sizeof(struct threads));
				if(q == NULL) {
					fprintf(stderr, "Controller: Cannot alloc queue item\n");
					exit(1);
				}
				if(t_head==NULL)
					t_head = q;
				else
					prev_t->next = q;

				memset(q, 0, sizeof(struct threads));
				//give it data.
				q->addr = tmp_q.addr;
				sprintf((char *)&q->conn.tmp_name, "%d", n++);
				pthread_mutex_init(&q->mutex, NULL);
				pthread_mutex_init(&q->wait_mutex, NULL);
				pthread_cond_init(&q->cond, NULL);
				q->inout = tmp_q.inout;
				memcpy(q->data, &tmp_q.data, tmp_q.data_length);
				q->data_length = tmp_q.data_length;
				q->consumed = false;
				if(pthread_create((pthread_t *)&q->tid, NULL, process_thread, q)!=0) {
					fprintf(stderr, "Controller: Cannot create thread\n");
					exit(1);
				}
				delete = true;
				//fprintf(stderr, "Controller: Thread %u is ok.\n", (unsigned int)q->tid);
			}
			pthread_mutex_unlock(&t_mutex);
			if(q != NULL && !new_thread){
				// let it write file.
				if(pthread_mutex_trylock(&q->mutex)==0) {
					if(q->consumed) {
						//fprintf(stderr, "Controller: Thread %u locked. Give it data.\n", (unsigned int)q->tid);
						q->inout = tmp_q.inout;
						memcpy(q->data, &tmp_q.data, tmp_q.data_length);
						q->data_length = tmp_q.data_length;
						q->consumed = false;
						pthread_cond_signal(&q->cond);
						pthread_mutex_unlock(&q->mutex);
						//fprintf(stderr, "Controller: Thread %u unlocked. It should wake.\n", (unsigned int)q->tid);
						delete = true;
					} else {
						//fprintf(stderr, "Controller: Thread %u hasn't consumed data. Wait it.\n", (unsigned int)q->tid);
						pthread_mutex_unlock(&q->mutex);
					}
				} else {
					//fprintf(stderr, "Controller: Cannot lock thread %u, may be busy.\n", (unsigned int)q->tid);
				}
			}
			if(q==NULL && tmp_q.inout >=2) {
				//nout found and closing. ignore it.
				fprintf(stderr, "Controller: Ignore closing connection without thread.\n");
				delete = true;
			}
			pthread_mutex_lock(&q_mutex);
			if(delete) {
				// remove from queue.
				if(p == q_head) {
					// prev == NULL
					q_head = q_head->next;
					free(prev_q);
					// prev = NULL;
					prev_q = q_head;
				}
				else {
					prev_q->next = p->next;
					free(p);
					p = prev_q;
				}
				if (q_head==NULL)
					q_tail = NULL;
				//pthread_mutex_unlock(&q_mutex);
			}
			p = p->next;
		}
		if (q_head==NULL) {
			//fprintf(stderr, "Controller: queue empty.\n");
			pthread_mutex_lock(&ctrl_mutex);
			if(ctrl_exit)
				break;
			pthread_mutex_unlock(&ctrl_mutex);
			//fprintf(stderr, "Controller: sleep.\n");
			pthread_cond_wait(&ctrl_cond, &q_mutex);
			//fprintf(stderr, "Controller: awake.\n");
		}
	}
	pthread_mutex_unlock(&q_mutex);
	pthread_mutex_unlock(&ctrl_mutex);
	fprintf(stderr, "Controller thread %u exit\n", (unsigned int)pthread_self());
	pthread_exit(0);
}
