#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <semaphore.h>
#include <string.h>
#include <sys/socket.h>
#include <netdb.h>
#include <errno.h>
#include <pthread.h>
#include "sbuf.h"

/* Recommended max cache and object sizes */
#define COUNT_OF_THREADS 8
#define SIZE_OF_QUEUE 5
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400
#define NINTENDO 64

static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (Macintosh; Intel Mac OS X 10.15; rv:97.0) Gecko/20100101 Firefox/97.0";

int complete_request_received(char *);
int parse_request(char *, char *, char *, char *, char *);
void test_parser();
void print_bytes(unsigned char *, int);

/* NEW FUNCTIONS*/
void handle_client(int sfd);
void *thread();
int open_sfd(int port);
void sbuf_init(sbuf_t *sp, int n);
void sbuf_deinit(sbuf_t *sp);
void sbuf_insert(sbuf_t *sp, int item);
int sbuf_remove(sbuf_t *sp);
/* Create an empty, bounded, shared FIFO buffer with n slots */
/* $begin sbuf_init */
void sbuf_init(sbuf_t *sp, int n)
{
	sp->buf = calloc(n, sizeof(int));
	sp->n = n;					/* Buffer holds max of n items */
	sp->front = sp->rear = 0;	/* Empty buffer iff front == rear */
	sem_init(&sp->mutex, 0, 1); /* Binary semaphore for locking */
	sem_init(&sp->slots, 0, n); /* Initially, buf has n empty slots */
	sem_init(&sp->items, 0, 0); /* Initially, buf has zero data items */
}
/* $end sbuf_init */

/* Clean up buffer sp */
/* $begin sbuf_deinit */
void sbuf_deinit(sbuf_t *sp)
{
	free(sp->buf);
}
/* $end sbuf_deinit */

/* Insert item onto the rear of shared buffer sp */
/* $begin sbuf_insert */
void sbuf_insert(sbuf_t *sp, int item)
{
	printf("before sem_wait(&sp->slots);\n");
	fflush(stdout);
	sem_wait(&sp->slots); /* Wait for available slot */
	printf("after sem_wait(&sp->slots);\n");
	fflush(stdout);
	sem_wait(&sp->mutex);					/* Lock the buffer */
	sp->buf[(++sp->rear) % (sp->n)] = item; /* Insert the item */
	sem_post(&sp->mutex);					/* Unlock the buffer */
	printf("before sem_post(&sp->items);\n");
	fflush(stdout);
	sem_post(&sp->items); /* Announce available item */
	printf("after sem_post(&sp->items);\n");
	fflush(stdout);
}
/* $end sbuf_insert */

/* Remove and return the first item from buffer sp */
/* $begin sbuf_remove */
int sbuf_remove(sbuf_t *sp)
{
	int item;

	printf("before sem_wait(&sp->items);\n");
	fflush(stdout);
	sem_wait(&sp->items); /* Wait for available item */
	printf("after sem_wait(&sp->items);\n");
	fflush(stdout);
	sem_wait(&sp->mutex);					 /* Lock the buffer */
	item = sp->buf[(++sp->front) % (sp->n)]; /* Remove the item */
	sem_post(&sp->mutex);					 /* Unlock the buffer */
	printf("before sem_post(&sp->slots);\n");
	fflush(stdout);
	sem_post(&sp->slots); /* Announce available slot */
	printf("after sem_post(&sp->slots);\n");
	fflush(stdout);
	return item;
}
/* $end sbuf_remove */
/* $end sbufc */

sbuf_t sbuf;

int main(int argc, char *argv[])
{
	struct sockaddr_storage remote_addr;
	socklen_t remote_addr_len = sizeof(remote_addr);
	pthread_t tid;

	sbuf_init(&sbuf, SIZE_OF_QUEUE);
	for (int i = 0; i < COUNT_OF_THREADS; i++)
	{
		pthread_create(&tid, NULL, thread, NULL);
	}

	int sfd = open_sfd(atoi(argv[1]));
	int newInteger;

	while (1)
	{

		if ((newInteger = accept(sfd, (struct sockaddr *)&remote_addr, &remote_addr_len)) < 0)
		{
			printf("accept error: %d\n", errno);
			fflush(stdout);
			break;
		}
		sbuf_insert(&sbuf, newInteger);
	}

	return 0;
}

void *thread()
{
	pthread_detach(pthread_self());

	while (1)
	{
		handle_client(sbuf_remove(&sbuf));
	}
}

int open_sfd(int port)
{

	int sfd;
	struct sockaddr_in ipv4addr;
	memset(&ipv4addr, 0, sizeof(struct sockaddr_in));

	ipv4addr.sin_family = AF_INET;
	ipv4addr.sin_addr.s_addr = INADDR_ANY;
	ipv4addr.sin_port = htons(port);

	if ((sfd = socket(AF_INET, SOCK_STREAM, 0)) < -1)
	{
		perror("Error creating socket");
		exit(EXIT_FAILURE);
	}

	int optval = 1;
	setsockopt(sfd, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval));

	if (bind(sfd, (struct sockaddr *)&ipv4addr, sizeof(struct sockaddr)) < 0)
	{
		perror("Could not bind");
		exit(EXIT_FAILURE);
	}

	listen(sfd, 100);
	printf("Listening on port %d\n", ntohs(ipv4addr.sin_port));
	fflush(stdout);

	return sfd;
}

void handle_client(int sfd)
{

	char buf[MAX_OBJECT_SIZE];
	bzero(buf, MAX_OBJECT_SIZE);

	int index = 0;
	int nread;

	do
	{
		nread = recv(sfd, &buf[index], 10, 0);
		if (nread < 0)
		{
			printf("recv error: %d\n", errno);
			fflush(stdout);
			break;
		}
		index += nread;
	} while (complete_request_received(buf) == 0);

	char method[16], hostname[NINTENDO], port[8], path[NINTENDO], headers[1024];
	if (parse_request(buf, method, hostname, port, path))
	{

		char newRequest[MAX_OBJECT_SIZE];
		bzero(newRequest, MAX_OBJECT_SIZE);

		strcat(newRequest, method);
		strcat(newRequest, path);
		strcat(newRequest, " HTTP/1.0\r\nHost: ");
		strcat(newRequest, hostname);

		if (strcmp(port, "80"))
		{
			strcat(newRequest, ":");
			strcat(newRequest, port);
		}

		strcat(newRequest, "\r\nUser-Agent: ");
		strcat(newRequest, user_agent_hdr);
		strcat(newRequest, "\r\nConnection: close\r\nProxy-Connection: close\r\n\r\n");

		struct addrinfo hints;
		struct addrinfo *result, *rp;
		int s, sfd2;
		memset(&hints, 0, sizeof(struct addrinfo));
		hints.ai_family = AF_INET;
		hints.ai_socktype = SOCK_STREAM;

		if ((s = getaddrinfo(hostname, port, &hints, &result)) != 0)
		{
			fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
			exit(EXIT_FAILURE);
		}

		for (rp = result; rp != NULL; rp = rp->ai_next)
		{
			sfd2 = socket(AF_INET, SOCK_STREAM, 0);
			if (sfd2 == -1)
				continue;
			if (connect(sfd2, rp->ai_addr, rp->ai_addrlen) != -1)
				break;
			close(sfd2);
		}

		if (rp == NULL)
		{ /* No address succeeded */
			fprintf(stderr, "Could not connect\n");
			exit(EXIT_FAILURE);
		}

		freeaddrinfo(result);

		int offset = 0, bytesSent, bytesRead, totalBytesRead;
		while (offset < strlen(newRequest))
		{
			bytesSent = write(sfd2, newRequest + offset, 1);
			offset += bytesSent;
		}

		char response[MAX_OBJECT_SIZE];
		bzero(response, MAX_OBJECT_SIZE);
		totalBytesRead = 0;
		while ((bytesRead = read(sfd2, response + totalBytesRead, MAX_OBJECT_SIZE)) != 0)
		{
			if (bytesRead == -1)
			{
				perror("read");
				exit(EXIT_FAILURE);
			}
			totalBytesRead += bytesRead;
		}
		// for (int i=0; i<totalBytesRead; i++) {
		// 	printf("%c", (char) response[i]);
		// }
		// printf("\n"); fflush(stdout);

		close(sfd2);

		offset = 0;
		while (offset < totalBytesRead)
		{
			bytesSent = write(sfd, response + offset, 1);
			offset += bytesSent;
		}

		close(sfd);
	}
	else
	{
		printf("REQUEST INCOMPLETE:\n%s\n\n", buf);
	}
}

int complete_request_received(char *request)
{
	return strstr(request, "\r\n\r\n") == NULL ? 0 : 1;
}

int parse_request(char *request, char *method, char *hostname, char *port, char *path)
{

	if (complete_request_received(request) == 0)
		return 0;

	bzero(method, 16);
	bzero(hostname, NINTENDO);

	bzero(port, 8);
	bzero(path, NINTENDO);

	char *indexPtr = request;
	char *endPtr;
	int length;

	endPtr = strstr(request, " ");
	length = endPtr - request;
	memcpy(method, request, length + 1);

	char URL[136];
	memset(URL, 0, 136);
	indexPtr = strstr(endPtr, "/") + 2;
	endPtr = strstr(endPtr + 1, " ");
	length = endPtr - indexPtr;
	memcpy(URL, indexPtr, length);

	if ((endPtr = strstr(URL, ":")) != NULL)
	{
		length = endPtr - URL;
		memcpy(hostname, URL, length);
	}
	else
	{

		if ((endPtr = strstr(URL, "/")) != NULL)
		{
			length = endPtr - URL;
			memcpy(hostname, URL, length);
		}
		else
		{
			printf("Error parsing hostname.\n");
			fflush(stdout);
		}
	}

	indexPtr = strstr(URL, ":");
	if (indexPtr == NULL)
	{
		char *defaultPort = "80";
		memcpy(port, defaultPort, 2);
	}
	else
	{
		indexPtr++;
		endPtr = strstr(indexPtr, "/");
		length = endPtr - indexPtr;
		memcpy(port, indexPtr, length);
	}

	indexPtr = strstr(URL, "/");
	endPtr = &URL[strlen(URL)];

	length = endPtr - indexPtr;
	memcpy(path, indexPtr, length);

	indexPtr = strstr(request, "\r\n") + 2;
	endPtr = strstr(request, "\r\n\r\n");

	length = endPtr - indexPtr;
	// memcpy(headers, indexPtr, length);

	return 1;
}

void test_parser()
{
	int i;
	char method[16], hostname[64], port[8], path[64];

	char *reqs[] = {
		"GET http://www.example.com/index.html HTTP/1.0\r\n"
		"Host: www.example.com\r\n"
		"User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:68.0) Gecko/20100101 Firefox/68.0\r\n"
		"Accept-Language: en-US,en;q=0.5\r\n\r\n",

		"GET http://www.example.com:8080/index.html?foo=1&bar=2 HTTP/1.0\r\n"
		"Host: www.example.com:8080\r\n"
		"User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:68.0) Gecko/20100101 Firefox/68.0\r\n"
		"Accept-Language: en-US,en;q=0.5\r\n\r\n",

		"GET http://localhost:1234/home.html HTTP/1.0\r\n"
		"Host: localhost:1234\r\n"
		"User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:68.0) Gecko/20100101 Firefox/68.0\r\n"
		"Accept-Language: en-US,en;q=0.5\r\n\r\n",

		"GET http://www.example.com:8080/index.html HTTP/1.0\r\n",

		NULL};

	for (i = 0; reqs[i] != NULL; i++)
	{
		printf("Testing %s\n", reqs[i]);
		if (parse_request(reqs[i], method, hostname, port, path))
		{
			printf("METHOD: %s\n", method);
			printf("HOSTNAME: %s\n", hostname);
			printf("PORT: %s\n", port);
			printf("PATH: %s\n", path);
		}
		else
		{
			printf("REQUEST INCOMPLETE\n");
		}
	}
}

void print_bytes(unsigned char *bytes, int byteslen)
{
	int i, j, byteslen_adjusted;

	if (byteslen % 8)
	{
		byteslen_adjusted = ((byteslen / 8) + 1) * 8;
	}
	else
	{
		byteslen_adjusted = byteslen;
	}
	for (i = 0; i < byteslen_adjusted + 1; i++)
	{
		if (!(i % 8))
		{
			if (i > 0)
			{
				for (j = i - 8; j < i; j++)
				{
					if (j >= byteslen_adjusted)
					{
						printf("  ");
					}
					else if (j >= byteslen)
					{
						printf("  ");
					}
					else if (bytes[j] >= '!' && bytes[j] <= '~')
					{
						printf(" %c", bytes[j]);
					}
					else
					{
						printf(" .");
					}
				}
			}
			if (i < byteslen_adjusted)
			{
				printf("\n%02X: ", i);
			}
		}
		else if (!(i % 4))
		{
			printf(" ");
		}
		if (i >= byteslen_adjusted)
		{
			continue;
		}
		else if (i >= byteslen)
		{
			printf("   ");
		}
		else
		{
			printf("%02X ", bytes[i]);
		}
	}
	printf("\n");
}