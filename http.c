#include "main.h"
#include "regex.h"

struct conn {
	struct tuple4 addr; // point to point, ip and port.
	//int inout; //up = 0, down = 1, request close = 2, response close = 3, close all = 4
	int state; // 0: sending request

	char *request_head;
	int qh_length;
	char *url;

	char *response_head;
	int sh_length;
	int rnrn_tail;

	int content_length;
	int ignore;
	int wrote;

	char *buf;
	int buf_len;

	FILE *fp;
	int f_count;

	struct conn *next;
};

int count = 0;

struct conn *c_head = NULL;

regex_t rnrn_regex;
regex_t uri_regex;
regex_t host_regex;
regex_t response_code_regex;
regex_t contentlength_regex;
regex_t tec_regex;

void process_http_(struct conn *c, struct queue *qnode);

void* process_http(void* arg) {
	fprintf(stderr, "starting http processor...");
	// init reges
	int flags = REG_EXTENDED;
	if(regcomp(&rnrn_regex, "\r\n\r\n", flags)) {
		fprintf(stderr, "regex error 1\n");
		exit(1);
	}

	if(regcomp(&uri_regex, "^(GET|POST|HEAD) ([^ ]+) HTTP/1.[01]", flags)) {
		fprintf(stderr, "regex error 2\n");
		exit(1);
	}

	if(regcomp(&host_regex, "Host: ([^\r\n]+)\r\n", flags)) {
		fprintf(stderr, "regex error 3\n");
		exit(1);
	}

	if(regcomp(&response_code_regex, "HTTP/1.[01] ([0-9]{3}) [^\r\n]+\r\n", flags)) {
		fprintf(stderr, "regex error 4\n");
		exit(1);
	}

	if(regcomp(&contentlength_regex, "Content-Length: ([0-9]+)\r\n", flags)) {
		fprintf(stderr, "regex error 5\n");
		exit(1);
	}

	if(regcomp(&tec_regex, "Transfer-Encoding: chunked\r\n", flags)) {
		fprintf(stderr, "regex error 6\n");
		exit(1);
	}


	// start process
	pthread_mutex_lock(&q_mutex);
	while(1) {
		pthread_cond_wait(&q_cond, &q_mutex);
		while(q_head != NULL) {
			// find the conn
			struct conn *p = c_head;
			while(p != NULL) {
				if(tuple4eq(p->addr, q_head->addr))
					break;
				p = p->next;
			}
			pthread_mutex_unlock(&q_mutex);

			process_http_(p, q_head);

			pthread_mutex_lock(&q_mutex);
			struct queue *q = q_head->next;

			free(q_head->data);
			free(q_head);
			q_head = q;

		}
	}
	// never reach here
	pthread_mutex_unlock(&q_mutex);
}

void shut_it(struct conn *c) {
	if(c->request_head) {
		free(c->request_head);
	}

	if(c->url) {
		free(c->url);
	}

	if(c->response_head) {
		free(c->response_head);
	}

	if(c->buf) {
		free(c->buf);
	}

	if(c->fp) {
		fclose(c->fp);
	}

	if(c == c_head) {
		c_head = c->next;
	} else {
		struct conn *p = c_head;
		while(p->next != c)
			p = p->next;
		p->next = c->next;
	}
	add_drop(c->addr);
	free(c);
}

int request_head_on_read(char *buf, int len);
int request_head_read_cb(char *buf, int len, struct conn *c);
int response_head_on_read(char *buf, int len, struct conn *c);
int response_head_read_cb(char *buf, int len, struct conn *c);
void move_data(struct conn *c);
void write_tail_data(struct conn *c);
int wrap_tec_process(struct conn *c);
void after_got_file(struct conn *c);

void process_http_(struct conn *c, struct queue *qnode) {
	if(c == NULL && qnode->inout >= 2)
		return;
	if(c != NULL && qnode->inout >= 2 && c->state == 4) {
		// save all of it
		if(!c->ignore) {
			fwrite(c->buf, c->buf_len, 1, c->fp);
			fprintf(stderr, "wrote %d\n", c->buf_len);
		}
		goto finished;
	}
	if(c != NULL && qnode->inout >= 2) {
		shut_it(c);
		return;
	}
	if(c == NULL) {
		c = (struct conn *)malloc(sizeof(struct conn));
		memset(c, 0, sizeof(struct conn));
		c->addr = qnode->addr;

		c->next = c_head;
		c_head = c;
	}
	//fprintf(stderr, "inout %d\n", qnode->inout);

	switch(c->state) {
		case 0: {
			// up only
			if(qnode->inout != 0) {
				shut_it(c);
				break;
			}
			//printf("inout %d\n", qnode->inout);

			int delta = 0;
			if(c->qh_length == 0) {
				c->qh_length = qnode->data_length + 1; // 0 in the end
				c->request_head = (char *)malloc(c->qh_length);
			} else {
				delta = c->qh_length;
				c->qh_length += qnode->data_length;
				c->request_head = (char *)realloc(c->request_head, c->qh_length);
			}
			if(c->request_head == NULL) {
				perror("cannot malloc");
				exit(1);
			}
			
			memcpy(c->request_head + delta, qnode->data, qnode->data_length);
			c->request_head[c->qh_length - 1] = 0;
			int res;
			res = request_head_on_read(c->request_head, c->qh_length);
			if(res == 0)
				break; // continue
			if(res == 2) {
				shut_it(c);
				fprintf(stderr, "Cannot read request (not http?), drop it\n");
				break;
			}
			if(res != 1) {
				fprintf(stderr, "error at %s:%d\n", __FILE__, __LINE__);
				exit(1);
			}
			// process it
			res = request_head_read_cb(c->request_head, c->qh_length, c);
			if(res == 0) {
				shut_it(c);
				fprintf(stderr, "Cannot parse request, drop it\n");
				//printf("%s\n", c->request_head);
				break;
			}
			c->state = 1;

			free(c->request_head);
			c->request_head = NULL;
			c->qh_length = 0;

			break;
		}
		case 1: {
			// down only
			if(qnode->inout != 1) {
				shut_it(c);
				break;
			}

			int delta = 0;
			if(c->sh_length == 0) {
				c->sh_length = qnode->data_length + 1; // 0 in the end
				c->response_head = (char *)malloc(c->sh_length);
			} else {
				delta = c->sh_length;
				c->sh_length += qnode->data_length;
				c->response_head = (char *)realloc(c->response_head, c->sh_length);
			}
			if(c->response_head == NULL) {
				perror("cannot malloc");
				exit(1);
			}

			memcpy(c->response_head + delta, qnode->data, qnode->data_length);
			c->response_head[c->sh_length - 1] = 0;
			int res;
			res = response_head_on_read(c->response_head, c->sh_length, c);
			if(res == 0)
				break; // continue
			if(res == 2) {
				shut_it(c);
				fprintf(stderr, "Cannot read reponse (not http?), drop it\n");
				break;
			}
			if(res != 1) {
				fprintf(stderr, "error at %s:%d\n", __FILE__, __LINE__);
				exit(1);
			}
			// process it
			res = response_head_read_cb(c->response_head, c->sh_length, c);
			if(res == 0) {
				shut_it(c);
				fprintf(stderr, "Cannot parse response, drop it\n");
				//printf("%s\n", c->request_head);
				break;
			}
			if(res == 1 && c->content_length == -1) {
				free(c->response_head);
				c->response_head = NULL;
				c->sh_length = 0;
				c->state = 5;
				break;
			}

			if(!c->ignore) {
				char fn[32] = { 0 };
				sprintf(fn, "data/%d", count);
				fprintf(stderr, "file %d\n", count);
				c->f_count = count;
				count ++;
				c->fp = fopen(fn, "w");
				if(!c->fp) {
					perror("cannot create file");
					exit(1);
				}
			}
			
			if(res == 1) {
				c->state = 2;
				if(!c->ignore) {
					write_tail_data(c);
				}
				if(c->wrote == c->content_length) {
					free(c->response_head);
					c->response_head = NULL;
					c->sh_length = 0;
					goto finished;
				}
			}
			if(res == 2) {
				c->state = 3;
				move_data(c);
				free(c->response_head);
				c->response_head = NULL;
				c->sh_length = 0;
				goto tec_p;
			}
			if(res == 3) {
				c->state = 4;
				write_tail_data(c);
			}

			free(c->response_head);
			c->response_head = NULL;
			c->sh_length = 0;

			break;
		}
		case 2: {
			// down only
			if(qnode->inout != 1) {
				shut_it(c);
				break;
			}
			if(!c->ignore) {
				fprintf(stderr, "wrote %d\n", qnode->data_length);
				fwrite(qnode->data, qnode->data_length, 1, c->fp);
			}
			c->wrote += qnode->data_length;

			if(c->wrote == c->content_length) {
				c->state = 5;
				goto finished;
			}
			if(c->wrote > c->content_length) {
				fprintf(stderr, "error at %s:%d\n", __FILE__, __LINE__);
				exit(1);
			}
			break;
		}
		case 3: {
			// down only
			if(qnode->inout != 1) {
				shut_it(c);
				break;
			}
			int delta = 0;
			if(c->buf_len == 0) {
				c->buf_len = qnode->data_length + 1; // 0 in the end
				c->buf = (char *)malloc(c->buf_len);
			} else {
				delta = c->buf_len - 1;
				c->buf_len += qnode->data_length;
				c->buf = (char *)realloc(c->buf, c->buf_len);
			}
			if(c->buf == NULL) {
				perror("cannot malloc");
				exit(1);
			}

			memcpy(c->buf + delta, qnode->data, qnode->data_length);
			c->buf[c->buf_len - 1] = 0;
			int res;
tec_p:
			res = wrap_tec_process(c);
			if(res < 0) {
				fprintf(stderr, "cannot process chunked data\n");
				fprintf(stderr, "%s\n", c->buf);
				shut_it(c);
				break;
			}
			if(res == 2) {
				free(c->buf);
				c->buf = NULL;
				c->buf_len = 0;
				c->state = 5;
				goto finished;
			}

			break;
		}
		case 4: {
			if(qnode->inout != 1) {
				shut_it(c);
				break;
			}
			if(!c->ignore) {
				fprintf(stderr, "wrote %d\n", qnode->data_length);
				fwrite(qnode->data, qnode->data_length, 1, c->fp);
			}
		}
		case 5: {
finished:
			if(!c->ignore && c->content_length != -1) {
				fclose(c->fp);
				c->fp = NULL;
				fprintf(stderr, "close file\n");
			}
			after_got_file(c);
			// clear c and go back to 0
			if(qnode->inout >= 2) {
				shut_it(c);
				break;
			}
			free(c->url);
			struct tuple4 addr = c->addr;
			struct conn *n = c->next;
			memset(c, 0, sizeof(struct conn));
			c->addr = addr;
			c->next = n;
			break;
		}
		default: {
			fprintf(stderr, "unknown state %d\n", c->state);
			shut_it(c);
		}
	}
	
}

// return 1 success, 0 not finished, 2 error
int request_head_on_read(char *buf, int len) {
	if(len < 5)
		return 0;

	if(	strncmp(buf, "GET ", 4) != 0 &&
		strncmp(buf, "POST ", 5) != 0 &&
		strncmp(buf, "HEAD ", 5) != 0) {
		fprintf(stderr, "method not supported\n");
		return 2;
	}

	int res;
	regmatch_t rnrn_match[1];
	regmatch_t contentlength_match[2];
	res = regexec(&rnrn_regex, buf, 1, rnrn_match, 0);
	if(res != 0) {
		// has no rnrn
		return 0;
	}
	// has rnrn

	res = regexec(&contentlength_regex, buf, 2, contentlength_match, 0);
	if(res != 0) {
		// has no Content-Length
		if(rnrn_match[0].rm_eo == len - 1)
			return 1;
		// rnrn in the tail
		printf("%d %d\n", rnrn_match[0].rm_eo, len);
		printf("%s\n", buf);
		return 2;
	}
	// has Content-Length
	int cl;
	cl = digit2int(buf, contentlength_match[1].rm_so, contentlength_match[1].rm_eo);
	if(rnrn_match[0].rm_eo + cl == len - 1)
		return 1;

	return 0;
}

// return 1 success, 0 error
int request_head_read_cb(char *buf, int len, struct conn *c) {
	int res;
	regmatch_t host_match[2];
	regmatch_t uri_match[3];

	res = regexec(&host_regex, buf, 2, host_match, 0);
	if(res != 0) {
		fprintf(stderr, "no match for host\n");
		return 0;
	}

	res = regexec(&uri_regex, buf, 3, uri_match, 0);
	if(res != 0) {
		fprintf(stderr, "no match for uri\n");
		return 0;
	}

	int url_len = host_match[1].rm_eo - host_match[1].rm_so + uri_match[2].rm_eo - uri_match[2].rm_so;
	c->url = (char *)malloc(url_len + 1);
	if(c->url == NULL) {
		perror("cannot malloc");
		exit(1);
	}
	int pos = 0;

	int i;
	for(i = uri_match[1].rm_so; i < uri_match[1].rm_eo; i++) {
		putchar(buf[i]);
	}
	printf(" http://");
	for(i = host_match[1].rm_so; i < host_match[1].rm_eo; i++) {
		//putchar(buf[i]);
		c->url[pos ++] = buf[i];
	}
		
	for(i = uri_match[2].rm_so; i < uri_match[2].rm_eo; i++) {
		//putchar(buf[i]);
		c->url[pos ++] = buf[i];
	}
	c->url[pos] = 0;
	
	//putchar('\n');
	printf("%s\n", c->url);

	return 1;
}

// return 1 success, 0 not finished, 2 error
int response_head_on_read(char *buf, int len, struct conn *c) {
	if(len < 5)
		return 0;

	if(strncmp(buf, "HTTP/", 5) != 0) {
		fprintf(stderr, "invalid response\n");
		return 2;
	}

	int res;
	regmatch_t rnrn_match[1];
	res = regexec(&rnrn_regex, buf, 1, rnrn_match, 0);
	if(res != 0) {
		// has no rnrn
		return 0;
	}
	// has rnrn
	c->rnrn_tail = rnrn_match[0].rm_eo;

	return 1;
}

// 0 error, 1 content-length, 2 tec, 3 close
int response_head_read_cb(char *buf, int len, struct conn *c) {
	regmatch_t response_code_match[2];
	regmatch_t contentlength_match[2];
	regmatch_t tec_match[1];
	int res;
	res = regexec(&response_code_regex, buf, 2, response_code_match, 0);
	if(res != 0) {
		fprintf(stderr, "no match for response code\n");
		return 0;
	}
	int code;
	code = digit2int(buf, response_code_match[1].rm_so, response_code_match[1].rm_eo);
	if(code != 200) {
		fprintf(stderr, "code != 200, dont save data\n");
		c->ignore = 1;
	}
	int cl_flag = 0, tec_flag = 0;
	res = regexec(&contentlength_regex, buf, 2, contentlength_match, 0);
	if(res == 0) {
		cl_flag++;
		c->content_length = digit2int(buf, contentlength_match[1].rm_so, contentlength_match[1].rm_eo);
		fprintf(stderr, "content_length %d\n", c->content_length);
		if(c->content_length == 0)
			c->content_length = -1;
	}

	res = regexec(&tec_regex, buf, 1, tec_match, 0);
	if(res == 0) {
		tec_flag++;
		fprintf(stderr, "chunked\n");
	}

	if(cl_flag == 0 && tec_flag == 0)
		return 3;

	if(cl_flag == 1 && tec_flag == 1)
		return 0;

	if(cl_flag == 1)
		return 1;

	// tec_flag == 1
	return 2;
}

void move_data(struct conn *c) {
	c->buf_len = c->sh_length - c->rnrn_tail;
	if(c->buf_len == 0)
		return;
	c->buf = (char *)malloc(c->buf_len);
	if(!c->buf) {
		perror("cannot malloc");
		exit(1);
	}
	memcpy(c->buf, c->response_head + c->rnrn_tail, c->buf_len);
	//fprintf(stderr, "%s\n-\n%s\n%d %d\n", c->response_head, c->buf, c->buf_len, c->rnrn_tail);
}

void write_tail_data(struct conn *c) {
	c->wrote = c->sh_length - 1 - c->rnrn_tail; // remember: 0 is in the end
	if(c->wrote == 0)
		return;
	fprintf(stderr, "wrote %d\n", c->wrote);
	fwrite(c->response_head + c->rnrn_tail, c->wrote, 1, c->fp);
}

// 0 not enough, -1 error, 1 gogo, 2 finish
int tec_process(struct conn *c) {
	if(c->buf_len < 6) // 0rnrn0
		return 0;
	// check if has chunk head
	int i;
	int h_len = 0; // lenth of digit in 1234rn
	for(i = 2; i < c->buf_len; i++) {
		if(c->buf[i-1] == '\r' && c->buf[i] == '\n') {
			h_len = i - 1;
			break;
		}
	}
	if(i > 10 && !h_len)
		return -1;
	if(!h_len)
		return 0;

	// process
	int size;
	size = tec_chunk_size(c->buf);
	if(size < 0) {
		fprintf(stderr, "cannot process chunked item\n");
		return -1;
	}
	if(size == 0) {
		if(c->buf_len != 6) {
			fprintf(stderr, "error at %s:%d\n", __FILE__, __LINE__);
			exit(1);
		}
		return 2;
	}
	//              123     rn  data   rn  0
	if(c->buf_len < h_len + 2 + size + 2 + 1)
		return 0;

	// yes! process it!
	if(!c->ignore) {
		fwrite(c->buf + h_len + 2, size, 1, c->fp);
		fprintf(stderr, "wrote %d\n", size);
	}

	// move
	if(c->buf_len == h_len + 2 + size + 2 + 1) {
		free(c->buf);
		c->buf = NULL;
		c->buf_len = 0;
	} else {
		int chunk_len = h_len + 2 + size + 2;
		int nlen = c->buf_len - chunk_len;
		char *nbuf = (char*)malloc(nlen);
		memcpy(nbuf, c->buf + chunk_len, nlen);
		free(c->buf);
		c->buf = nbuf;
		c->buf_len = nlen;
	}

	return 1;
}

int wrap_tec_process(struct conn *c) {
	int res = 1;
	while(res == 1) {
		res = tec_process(c);
	}
	return res;
}

char sip[20];
char dip[20];
char sp[10];
char dp[10];

void address (struct tuple4 addr) {
	static char buf[256];
	strcpy (sip, int_ntoa (addr.saddr));
	sprintf (sp, "%i", addr.source);
	strcpy (dip, int_ntoa (addr.daddr));
	sprintf (dp, "%i", addr.dest);
}

void write_data_file(struct conn *c) {
	static char fn[32] = { 0 };
	sprintf(fn, "data/%d.txt", c->f_count);
	FILE *f = fopen(fn, "w");
	if (f == NULL)
	{
	    perror("Error opening file!");
	    return;
	}

	/* print some text */
	const char *text = "Write this to the file";
	fprintf(f, "%s,%s,%s,%s,%s", sip, sp, dip, dp, c->url);

	fclose(f);
}

void after_got_file(struct conn *c) {
	address(c->addr);
	printf("=====\nFrom: %s:%s\nTo:   %s:%s\nURL:  %s\nFILE: %d\n=====\n", sip, sp, dip, dp, c->url, c->f_count);
	//send( sockfd, str, strlen(str)+1, 0 ); 
	write_data_file(c);
}

