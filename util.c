#include "main.h"

int digit2int(char *buf, int start, int end) {
	int result = 0;
	int i;
	for(i = start; i < end; i ++) {
		result = result * 10 + (int)(buf[i] - '0');
	}
	return result;
}

int char2int(char c) {
	if( '0' <= c && c <= '9')
		return c - '0';
	if( 'a' <= c && c <= 'f')
		return c - 'a' + 10;
	if( 'A' <= c && c <= 'F')
		return c - 'A' + 10;
	if(c = '\r')
		return -2;

	fprintf(stderr, "unknown charactor int %d\n", (int)c);

	return -1;
}

int tec_chunk_size(char *buf) {
	int result = 0;
	int nofail = 0;
	int t;
	int i = 0;

	while(1) {
		t = char2int(buf[i]);
		if(t == -2) {
			if(nofail)
				return result;
			return -1;
		}
		if(t == -1)
			return -1;

		result = result * 16 + t;
		nofail = 1;
		i ++;
	}

	return result;
}
