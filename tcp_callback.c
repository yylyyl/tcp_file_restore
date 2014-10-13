#include "main.h"

struct queue *q_head = NULL, *q_tail = NULL;
pthread_mutex_t q_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t q_cond = PTHREAD_COND_INITIALIZER;

void process__(struct tcp_stream *a_tcp, int close);

void process_up(struct tcp_stream *a_tcp) {
	struct half_stream hlf = a_tcp->server;
	struct threads *p = NULL;

	if(hlf.offset==0) {
		// new connection! Judge if it is http GET/POST HEAD request.
		if(hlf.count <= 5) {
			a_tcp->client.collect--;
			a_tcp->server.collect--;
			fprintf(stderr, "Drop it (too short)\n");
			return;
		}
		if( strncmp(hlf.data, "GET ", 4) != 0 &&
			strncmp(hlf.data, "HEAD ", 5) != 0 &&
			strncmp(hlf.data, "POST ", 4) != 0) {
			a_tcp->client.collect--;
			a_tcp->server.collect--;
			fprintf(stderr, "Drop it (not HTTP)\n");
			return;
		}
	}

	process__(a_tcp, false);
}

void process_down(struct tcp_stream *a_tcp) {
	process__(a_tcp, false);
}

void process_close(struct tcp_stream *a_tcp) {
	process__(a_tcp, true);
}

void process__(struct tcp_stream *a_tcp, int close) {
	// add to queue tail.
	//fprintf(stderr, "Process__.\n");
	pthread_mutex_lock(&q_mutex);
	//fprintf(stderr, "Process__..\n");
	if(q_head == NULL) {
		// empty queue
		q_tail = (struct queue *) malloc(sizeof(struct queue));
		if(q_tail == NULL) {
			fprintf(stderr, "Cannot malloc q_head.\n");
			exit(1);
		}
		q_head = q_tail;
	} else {
		q_tail->next = (struct queue *) malloc(sizeof(struct queue));
		if(q_tail->next == NULL) {
			fprintf(stderr, "Cannot malloc q_tail.\n");
			exit(1);
		}
		q_tail = q_tail->next;
	}
	memset(q_tail, 0, sizeof(struct queue));
	q_tail->addr = a_tcp->addr;
	if(a_tcp->server.count_new) {
		q_tail->inout = close ? 2:0;
		q_tail->data_length = a_tcp->server.count - a_tcp->server.offset;
		q_tail->data = (char*)malloc(q_tail->data_length);
		if(!q_tail->data) {
			perror("cannot malloc");
			exit(1);
		}
		//printf("pkt c %d o %d n %d l %d\n", a_tcp->server.count, a_tcp->server.offset, a_tcp->server.count_new, q_tail->data_length);
		memcpy(q_tail->data, a_tcp->server.data, q_tail->data_length);
	} else {
		q_tail->inout = close ? 3:1;
		q_tail->data_length =  a_tcp->client.count - a_tcp->client.offset;
		q_tail->data = (char*)malloc(q_tail->data_length);
		if(!q_tail->data) {
			perror("cannot malloc");
			exit(1);
		}
		//printf("pkt c %d o %d n %d l %d\n", a_tcp->client.count, a_tcp->client.offset, a_tcp->client.count_new, q_tail->data_length);
		memcpy(q_tail->data, a_tcp->client.data, q_tail->data_length);
	}
	//fprintf(stderr, "Process__...\n");
	pthread_cond_signal(&q_cond);
	pthread_mutex_unlock(&q_mutex);
	//fprintf(stderr, "Process__....\n");
}

void tcp_callback(struct tcp_stream *a_tcp, void **arg) {
	char buf[1024];
	strcpy(&buf, adres(a_tcp->addr));
	switch(a_tcp->nids_state){
		case NIDS_JUST_EST:
			if(a_tcp->addr.dest == 80) {
				// Connection just established. Judge if the connection is interesting.
				a_tcp->client.collect++; // We need data received by client
				a_tcp->server.collect++; // We need data received by server aka sent by client. 

				fprintf(stderr, "%s established\n", buf);
			}
			
			break;

		case NIDS_DATA:
			//New data has arrived on a connection. The half_stream structures contain buffers of data.
			if(check_drop(a_tcp->addr)) {
				// drop it
				a_tcp->client.collect--;
				a_tcp->server.collect--;
				break;
			}
			if(a_tcp->server.count_new){
				//fprintf(stderr, "Send data...\n");
				process_up(a_tcp);
				break;
            }
            if(a_tcp->client.count_new){
            	//fprintf(stderr, "Receive data...\n");
            	process_down(a_tcp);
            	break;
            }
			fprintf(stderr, "Unknown NIDS_DATA direction.");
			exit(1);

			break;

		case NIDS_CLOSE:
		case NIDS_RESET:
		case NIDS_TIMED_OUT:
			//Connection has closed. The TCP callback function should free any resources it may have allocated for this connection.
			process_close(a_tcp);

			fprintf(stderr, "%s closing\n", buf);
			break;
		case NIDS_EXITING:
			fprintf(stderr,"NIDS exit\n");
			break;

		default:
			fprintf(stderr,"Unknown NIDS state\n");
			exit(1);
	}
}
