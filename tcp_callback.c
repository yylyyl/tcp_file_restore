#include "main.h"

void process__(struct tcp_stream *a_tcp, int type);

void process_request(struct tcp_stream *a_tcp) {
	struct half_stream *hlf = &a_tcp->server;
	struct threads *p = NULL;

	if(hlf->offset==0) {
		// new connection! Judge if it is http GET request. Maybe use regex in the future.
		if(hlf->count <= 4) {
			a_tcp->client.collect--;
			a_tcp->server.collect--;
			fprintf(stderr, "Drop it (too short)\n");
			return;
		}
		if(!(hlf->data[0]=='G' && hlf->data[1]=='E' && hlf->data[2]=='T' && hlf->data[3]==' ')) {
			a_tcp->client.collect--;
			a_tcp->server.collect--;
			fprintf(stderr, "Drop it (not GET)\n");
			return;
		}
	fprintf(stderr, "Follow it\n");
	}
	process__(a_tcp, 0);
}

void process_response(struct tcp_stream *a_tcp) {
	process__(a_tcp, 1);
}

void process_close(struct tcp_stream *a_tcp) {
	fprintf(stderr, "Close it\n");
	process__(a_tcp, 4);
}

void process__(struct tcp_stream *a_tcp, int type) {
	// add to queue tail.
	//fprintf(stderr, "Process__.\n");
	pthread_mutex_lock(&q_mutex);
	//fprintf(stderr, "Process__..\n");
	if(q_tail == NULL) {
		// empty queue
		if(q_head != NULL) {
			fprintf(stderr, "Mistake in link list\n");
			exit(1);
		}
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

	q_tail->inout = type;
	switch(type) {
		case 0:
			q_tail->data_length = a_tcp->server.count - a_tcp->server.offset;
			memcpy(q_tail->data, a_tcp->server.data, q_tail->data_length);
			break;
		case 1:
			q_tail->data_length =  a_tcp->client.count - a_tcp->client.offset;
			memcpy(q_tail->data, a_tcp->client.data, q_tail->data_length);
			break;
		case 2:

			break;
		case 3:

			break;
		case 4:
			// connection closing. close all files.
			break;
		default:
			fprintf(stderr, "Unknown type code\n");
			exit(1);
	}
	if(q_tail->data_length > 1500) {
		fprintf(stderr, "Too big packet.\n");
		exit(1);
	}
	//fprintf(stderr, "Process__...\n");
	pthread_cond_signal(&ctrl_cond);
	pthread_mutex_unlock(&q_mutex);
	//fprintf(stderr, "Process__....\n");
	// finish in main thread
}

void tcp_callback(struct tcp_stream *a_tcp, void **arg) {
	char buf[1024];
	strncpy((char *)&buf, (char *)adres(a_tcp->addr), 1024);
	switch(a_tcp->nids_state){
		case NIDS_JUST_EST:
			// Connection just established. Judge if the connection is interesting.
			a_tcp->client.collect++; // We need data received by client
			a_tcp->server.collect++; // We need data received by server aka sent by client. 

			fprintf(stderr, "%s established\n", buf);
			break;

		case NIDS_DATA:
			//New data has arrived on a connection. The half_stream structures contain buffers of data.
			if(a_tcp->server.count_new){
				//fprintf(stderr, "Send data...\n");
				process_request(a_tcp);
				break;
			}
			if(a_tcp->client.count_new){
				//fprintf(stderr, "Receive data...\n");
				process_response(a_tcp);
				break;
			}
			fprintf(stderr, "Unknown NIDS_DATA direction.");
			exit(1);

			break;

		case NIDS_CLOSE:
		case NIDS_RESET:
		case NIDS_TIMED_OUT:
			//Connection has closed. The TCP callback function should free any resources it may have allocated for this connection.
			fprintf(stderr, "%s closing\n", buf);
			process_close(a_tcp);

			break;
		case NIDS_EXITING:
			fprintf(stderr,"NIDS exit\n");
			break;

		default:
			fprintf(stderr,"Unknown NIDS state\n");
			exit(1);
	}
}
