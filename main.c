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
	pthread_mutex_t mutex;
	pthread_cond_t cond;
	struct threads *next;
};

struct queue {
	struct tuple4 addr;
	int inout; //request = 0, response = 1, request close = 2, response close = 3, close all = 4
	char data[1600];
	int data_length;
	struct queue *next;
};

struct threads *t_head = NULL;
struct queue *q_head = NULL, *q_tail = NULL;
pthread_t ctrl_t;
pthread_mutex_t q_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t t_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t ctrl_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t ctrl_cond = PTHREAD_COND_INITIALIZER;
bool ctrl_exit = false;
bool exiting = false;

void signal_handler(int i);
void tcp_callback(struct tcp_stream *a_tcp, void **arg);
void* controller_thread(void *arg);

// struct tuple4 contains addresses and port numbers of the TCP connections
// the following auxiliary function produces a string looking like
// 10.0.0.1,1024 <-> 10.0.0.2,23
char * adres (struct tuple4 addr)
{
	static char buf[256];
	strcpy (buf, int_ntoa (addr.saddr));
	sprintf (buf + strlen (buf), ":%i <-> ", addr.source);
	strcat (buf, int_ntoa (addr.daddr));
	sprintf (buf + strlen (buf), ":%i", addr.dest);
	return buf;
}

int main(int argc, char *argv[]) {
	signal(SIGINT, signal_handler);
	/*
	if(argc!=3)
	{
		fprintf(stderr,"Usage: gfw <interface> <block_list_file>\n");
		exit(1);
	}
	*/
	nids_params.device = argv[1];

	//disable checksum, or nothing we can get.
	struct nids_chksum_ctl ctl;
	ctl.netaddr = 0;
	ctl.mask = 0;
	ctl.action = NIDS_DONT_CHKSUM;
	nids_register_chksum_ctl(&ctl,1); //1 means 1 item.

	if(!nids_init()) {
		fprintf(stderr,"%s\n",nids_errbuf);
		exit(1);
	}

	nids_register_tcp(tcp_callback);

	if(pthread_create(&ctrl_t, NULL, controller_thread, NULL)!=0) {
		fprintf(stderr, "Cannot create controller thread.\n");
		exit(1);
	}

	nids_run();
	return 0;
}
