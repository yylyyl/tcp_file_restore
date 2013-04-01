#include "main.h"

void* process_thread(void *arg) {
	struct threads *p = (struct threads *) arg;
	//pthread_cond_signal(&p->cond);
	fprintf(stderr, "Thread %u: start\n", (unsigned int)pthread_self());
	pthread_mutex_lock(&p->mutex);
	while(1) {
		//fprintf(stderr, "Thread %u: self locked\n", (unsigned int)pthread_self());
		char request_filename[16];
		char response_filename[16];
		switch(p->inout) {
			case 0:
				fprintf(stderr, "Thread %u: will write request\n", (unsigned int)pthread_self());
				if(p->conn.request_file == NULL) {
					sprintf(request_filename, "%s_request", p->conn.tmp_name);
					p->conn.request_file = fopen(request_filename, "w");
					if(p->conn.request_file == NULL) {
						fprintf(stderr, "Thread %u: Cannot open file %s\n", (unsigned int)pthread_self(), request_filename);
						exit(1);
					}
					fprintf(stderr, "Thread %u: Open file %s\n", (unsigned int)pthread_self(), request_filename);
				}
				//fprintf(stderr, "Write... %s %d %u\n", p->data, p->data_length, p->conn.request_file);
				fwrite(p->data, p->data_length, 1, p->conn.request_file);
				fprintf(stderr, "Thread %u: Write ok\n", (unsigned int)pthread_self());
				break;
				/* 
				// judge if it ends
				// save ending				
				if(p->data_length >= 4) {
					memcpy((char *)p->request_ending, (char *)p->data + p->data_length - 4, 4);
					p->request_ending_count = 4;
				} else {
					int i;
					for(i = 0; i < p->data_length; i++) {
						p->request_ending[i] = p->request_ending[i+1];
					}
					p->request_ending_count -= p->data_length;
					if(p->request_ending_count < 0)
						p->request_ending_count = 0;
					for(i = 0; i < p->data_length; i++) {
						p->request_ending[p->request_ending_count + i] = p->data[i];
					}
					p->request_ending_count += p->data_length;
					if(p->request_ending_count > 4)
						p->request_ending_count = 4;
				}
				// judge
				if(!(p->request_ending[0]=='\r' && p->request_ending[1]=='\n' && p->request_ending[2]=='\r' && p->request_ending[3]=='\n'))
				break;
				fprintf(stderr, "Thread %u: Request ends with \\r\\n\\r\\n.\n", (unsigned int)pthread_self());
				*/
			case 2: // close request
				fprintf(stderr, "Thread %u: Will close request\n", (unsigned int)pthread_self());
				sprintf(request_filename, "%s_request", p->conn.tmp_name);
				if(p->conn.request_file != NULL) {
					fclose(p->conn.request_file);
					p->conn.request_file = NULL;
					fprintf(stderr, "Thread %u: Close file %s\n", (unsigned int)pthread_self(), request_filename);
				}
				break;
			case 1:
				fprintf(stderr, "Thread %u: Will write response\n", (unsigned int)pthread_self());
				if(p->conn.response_file == NULL) {
					sprintf(response_filename, "%s_response", p->conn.tmp_name);
					p->conn.response_file = fopen(response_filename, "w");
					if(p->conn.response_file == NULL) {
						fprintf(stderr, "Thread %u: Cannot open file %s\n", (unsigned int)pthread_self(), response_filename);
						exit(1);
					}
					fprintf(stderr, "Thread %u: Open file %s\n", (unsigned int)pthread_self(), response_filename);
				}
				fwrite(p->data, p->data_length, 1, p->conn.response_file);
				fprintf(stderr, "Thread %u: Write ok\n", (unsigned int)pthread_self());
				break;
			case 3: // close response
				fprintf(stderr, "Thread %u: Will close response\n", (unsigned int)pthread_self());
				sprintf(response_filename, "%s_response", p->conn.tmp_name);
				if(p->conn.response_file != NULL) {
					fclose(p->conn.response_file);
					p->conn.response_file = NULL;
					fprintf(stderr, "Thread %u: Close file %s\n", (unsigned int)pthread_self(), response_filename);
				}
				break;
			case 4: // close all
				fprintf(stderr, "Thread %u: will close all\n", (unsigned int)pthread_self());
				sprintf(request_filename, "%s_request", p->conn.tmp_name);
				if(p->conn.request_file != NULL) {
					fclose(p->conn.request_file);
					p->conn.request_file = NULL;
					fprintf(stderr, "Thread %u: Close file %s\n", (unsigned int)pthread_self(), request_filename);
				}
				sprintf(response_filename, "%s_response", p->conn.tmp_name);
				if(p->conn.response_file != NULL) {
					fclose(p->conn.response_file);
					p->conn.response_file = NULL;
					fprintf(stderr, "Thread %u: Close file %s\n", (unsigned int)pthread_self(), response_filename);
				}
				break;
			default:
				fprintf(stderr, "Thread %u: Unknown thread status\n", (unsigned int)pthread_self());
				exit(1);
		}
		p->consumed = true;
		//pthread_mutex_unlock(&p->mutex);
		//fprintf(stderr, "Thread %u: self unlocked\n", (unsigned int)pthread_self());
		if(p->conn.request_file == NULL && p->conn.response_file == NULL) {
			// delete thread
			pthread_mutex_lock(&t_mutex);
			struct threads *q = t_head, *prev;
			if(t_head==p) {
				t_head = t_head->next;
			} else {
				while(q != p && q != NULL) {
					prev = q;
					q = q->next;
				}
				if(q == NULL) {
					fprintf(stderr, "Thread %u: Cannot delete thread from link list\n", (unsigned int)pthread_self());
					exit(1);
				}
				prev->next = p->next;
			}
			pthread_mutex_destroy(&p->mutex);
			pthread_cond_destroy(&p->cond);
			free(p);
			pthread_mutex_unlock(&t_mutex);
			// exit
			fprintf(stderr, "Thread %u: exit\n", (unsigned int)pthread_self());
			pthread_exit(0);
		}
		fprintf(stderr, "Thread %u: sleep\n", (unsigned int)pthread_self());
		//pthread_mutex_lock(&p->mutex);
		pthread_cond_wait(&p->cond, &p->mutex);
		fprintf(stderr, "Thread %u: awake from wait\n", (unsigned int)pthread_self());
	}
}
