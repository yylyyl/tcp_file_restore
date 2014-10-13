#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <pthread.h>
#include <signal.h>
#include "nids.h"

#define int_ntoa(x)	inet_ntoa(*((struct in_addr *)&x))

struct queue {
	struct tuple4 addr;
	int inout; //up = 0, down = 1, request close = 2, response close = 3, close all = 4
	char *data;
	int data_length;
	struct queue *next;
};

extern struct queue *q_head, *q_tail;
extern pthread_mutex_t q_mutex;
extern pthread_cond_t q_cond;
//static bool exiting = false;

int tuple4eq(struct tuple4 a, struct tuple4 b);
void* process_http(void *arg);
void tcp_callback(struct tcp_stream *a_tcp, void **arg);
void add_drop(struct tuple4 addr);

int digit2int(char *buf, int start, int end);
int tec_chunk_size(char *buf);
