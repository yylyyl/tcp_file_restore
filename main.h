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

struct connection {
	char tmp_name[8];
	FILE *request_file;
	FILE *response_file;
};

struct threads {
	pthread_t *tid;
	struct tuple4 addr; // point to point, ip and port.
	struct connection conn;
	int inout; //request = 0, response = 1, request close = 2, response close = 3, close all = 4
	char data[1600];
	int data_length;
	//char request_ending[4];
	//int request_ending_count;
	//char response_ending[4];
	//int response_ending_count;
	pthread_mutex_t mutex;
	pthread_mutex_t wait_mutex;
	pthread_cond_t cond;
	bool consumed;
	struct threads *next;
};

struct queue {
	struct tuple4 addr;
	int inout; //request = 0, response = 1, request close = 2, response close = 3, close all = 4
	char data[1600];
	int data_length;
	struct queue *next;
};

extern struct threads *t_head;
extern struct queue *q_head, *q_tail;
extern pthread_t ctrl_t;
extern pthread_mutex_t q_mutex;
extern pthread_mutex_t t_mutex;
extern pthread_mutex_t ctrl_mutex;
extern pthread_cond_t ctrl_cond;
extern bool ctrl_exit;
extern bool exiting;

void* process_thread(void *arg);
