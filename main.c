#include "main.h"

// struct tuple4 contains addresses and port numbers of the TCP connections
// the following auxiliary function produces a string looking like
// 10.0.0.1,1024 <-> 10.0.0.2,23
char * adres (struct tuple4 addr) {
	static char buf[256];
	strcpy (buf, int_ntoa (addr.saddr));
	sprintf (buf + strlen (buf), ":%i <-> ", addr.source);
	strcat (buf, int_ntoa (addr.daddr));
	sprintf (buf + strlen (buf), ":%i", addr.dest);
	return buf;
}

int tuple4eq(struct tuple4 a, struct tuple4 b) {
	if(	a.source == b.source &&
		a.dest   == b.dest   &&
		a.saddr  == b.saddr  &&
		a.daddr  == b.daddr)
		return 1;
	return 0;
}

int main(int argc, char *argv[]) {
	//signal(SIGINT, signal_handler);
	/*
	if(argc!=3)
	{
		fprintf(stderr,"Usage: http2file <interface>\n");
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

	pthread_t http_pid;

	if(pthread_create(&http_pid, NULL, process_http, NULL)!=0) {
		fprintf(stderr, "Cannot create process_http thread.\n");
		exit(1);
	}

	nids_run();
	return 0;
}
