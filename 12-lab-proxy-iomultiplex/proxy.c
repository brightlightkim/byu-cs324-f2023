#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <sys/types.h>
#include <netdb.h>
#include <pthread.h>
#include <semaphore.h>

/* Recommended max object size */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400
#define NTHREADS 8
#define SBUFSIZE 5

static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (Macintosh; Intel Mac OS X 10.15; rv:97.0) Gecko/20100101 Firefox/97.0";

/* $begin sbuft */
typedef struct
{
	int *buf;	 /* Buffer array */
	int n;		 /* Maximum number of slots */
	int front;	 /* buf[(front+1)%n] is first item */
	int rear;	 /* buf[rear%n] is last item */
	sem_t mutex; /* Protects accesses to buf */
	sem_t slots; /* Counts available slots */
	sem_t items; /* Counts available items */
} sbuf_t;
/* $end sbuft */

void sbuf_init(sbuf_t *sp, int n);
void sbuf_deinit(sbuf_t *sp);
void sbuf_insert(sbuf_t *sp, int item);
int sbuf_remove(sbuf_t *sp);

int complete_request_received(char *);
int parse_request(char *, char *, char *, char *, char *);
void test_parser();
void print_bytes(unsigned char *, int);

int open_sfd();
void handle_client();
void *handle_request(void *vargp);

sbuf_t sbuf; /* Shared buffer of connected descriptors */

int main(int argc, char *argv[])
{
	struct sockaddr_storage peer_addr;
	socklen_t peer_addr_len;
	int sfd, connfd, i;
	pthread_t tid;

	// test_parser();
	printf("%s\n", user_agent_hdr);
	sfd = open_sfd(argv[1]);
	sbuf_init(&sbuf, SBUFSIZE);
	for (i = 0; i < NTHREADS; i++) /* Create worker threads */
		pthread_create(&tid, NULL, handle_request, NULL);
	while (1)
	{
		peer_addr_len = sizeof(struct sockaddr_storage);
		connfd = accept(sfd, (struct sockaddr *)&peer_addr, &peer_addr_len);
		sbuf_insert(&sbuf, connfd); /* Insert connfd in buffer */
	}

	return 0;
}

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
	printf("before sem_wait()\n");
	fflush(stdout);
	sem_wait(&sp->slots); /* Wait for available slot */
	printf("after sem_wait()\n");
	fflush(stdout);
	sem_wait(&sp->mutex);					/* Lock the buffer */
	sp->buf[(++sp->rear) % (sp->n)] = item; /* Insert the item */

	sem_post(&sp->mutex); /* Unlock the buffer */
	printf("before sem_post()\n");
	fflush(stdout);
	sem_post(&sp->items); /* Announce available item */
	printf("after sem_post()\n");
	fflush(stdout);
}
/* $end sbuf_insert */

/* Remove and return the first item from buffer sp */
/* $begin sbuf_remove */
int sbuf_remove(sbuf_t *sp)
{
	int item;
	printf("before sem_wait()\n");
	fflush(stdout);
	sem_wait(&sp->items); /* Wait for available item */
	printf("after sem_wait()\n");
	fflush(stdout);
	sem_wait(&sp->mutex);					 /* Lock the buffer */
	item = sp->buf[(++sp->front) % (sp->n)]; /* Remove the item */
	sem_post(&sp->mutex);					 /* Unlock the buffer */
	printf("before sem_post()\n");
	fflush(stdout);
	sem_post(&sp->slots); /* Announce available slot */
	printf("after sem_post()\n");
	fflush(stdout);
	return item;
}
/* $end sbuf_remove */
/* $end sbufc */

/* Thread routine */
void *handle_request(void *vargp)
{
	pthread_detach(pthread_self());
	while (1)
	{
		int connfd = sbuf_remove(&sbuf); /* Remove connfd from buffer */ // line:conc:pre:removeconnfd
		handle_client(connfd);											 /* Service client */
		close(connfd);
	}
}

int open_sfd(char *port)
{
	struct addrinfo hints;
	int sfd, s;
	struct addrinfo *result;

	memset(&hints, 0, sizeof(struct addrinfo));

	hints.ai_family = AF_INET;		 /* Choose IPv4 or IPv6 */
	hints.ai_socktype = SOCK_STREAM; /* Datagram socket */
	hints.ai_flags = AI_PASSIVE;	 /* For wildcard IP address */
	hints.ai_protocol = 0;			 /* Any protocol */
	hints.ai_canonname = NULL;
	hints.ai_addr = NULL;
	hints.ai_next = NULL;

	if ((s = getaddrinfo(NULL, port, &hints, &result)) < 0)
	{
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
		exit(EXIT_FAILURE);
	}

	if ((sfd = socket(result->ai_family, result->ai_socktype, 0)) < 0)
	{
		perror("Error creating socket");
		exit(EXIT_FAILURE);
	}

	int optval = 1;
	setsockopt(sfd, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval));

	if (bind(sfd, result->ai_addr, result->ai_addrlen) < 0)
	{
		perror("Could not bind");
		exit(EXIT_FAILURE);
	}

	freeaddrinfo(result); /* No longer needed */

	if (listen(sfd, 100) < 0)
	{
		perror("Could not listen");
		exit(EXIT_FAILURE);
	}

	return sfd;
}

void handle_client(int sfd)
{
	ssize_t nread, nwrite;
	char buf[MAX_OBJECT_SIZE], req[MAX_OBJECT_SIZE];
	char method[16], hostname[64], port[8], path[64], req_headers[1024];
	int total = 0;
	struct addrinfo hints;
	struct addrinfo *result;
	int sfd2, s;
	memset(req, 0, MAX_OBJECT_SIZE);
	memset(buf, 0, MAX_OBJECT_SIZE);

	while (1)
	{
		nread = recv(sfd, buf + total, sizeof(buf) - total, 0);
		total += nread;
		if (parse_request(buf, method, hostname, port, path))
		{
			break;
		}
	}

	// print_bytes((unsigned char *)buf, total);

	printf("Received %d bytes\n",
		   total);

	printf("METHOD: %s\n", method);
	printf("HOSTNAME: %s\n", hostname);
	printf("PORT: %s\n", port);

	strcat(req, method);
	strcat(req, " ");
	strcat(req, path);
	strcat(req, " HTTP/1.0\r\n");
	if (strcmp(port, "80") == 0)
	{
		sprintf(req_headers, "Host: %s\r\n%s\r\n",
				hostname, user_agent_hdr);
	}
	else
	{
		sprintf(req_headers, "Host: %s:%s\r\n%s\r\n",
				hostname, port, user_agent_hdr);
	}
	strcat(req_headers, "Connection: close\r\n");
	strcat(req_headers, "Proxy-Connection: close\r\n\r\n");
	strcat(req, req_headers);
	printf("%s\n", req);

	// communicate with the HTTP server
	hints.ai_family = AF_INET;		 /* Choose IPv4 or IPv6 */
	hints.ai_socktype = SOCK_STREAM; /* Datagram socket */
	hints.ai_flags = AI_PASSIVE;	 /* For wildcard IP address */
	hints.ai_protocol = 0;			 /* Any protocol */
	hints.ai_canonname = NULL;
	hints.ai_addr = NULL;
	hints.ai_next = NULL;

	if ((s = getaddrinfo(hostname, port, &hints, &result)) < 0)
	{
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
		exit(EXIT_FAILURE);
	}

	if ((sfd2 = socket(result->ai_family, result->ai_socktype, 0)) < 0)
	{
		perror("Error creating socket");
		exit(EXIT_FAILURE);
	}

	if (connect(sfd2, result->ai_addr, result->ai_addrlen) < 0)
	{
		fprintf(stderr, "Could not connect\n");
		exit(EXIT_FAILURE);
	}

	freeaddrinfo(result); /* No longer needed */

	if ((nwrite = send(sfd2, req, strlen(req), 0)) != strlen(req))
	{
		fprintf(stderr, "Error sending response\n");
	};
	printf("num bytes sent to server: %ld\n", nwrite);

	total = 0;
	memset(buf, 0, MAX_OBJECT_SIZE);
	while ((nread = recv(sfd2, buf + total, sizeof(buf) - total, 0)))
	{
		total += nread;
	}
	printf("bytes received from the server:\n %s\n", buf);
	printf("num bytes recieved from server: %d\n", total);

	close(sfd2);

	if ((nwrite = send(sfd, buf, total, 0)) != total)
	{
		fprintf(stderr, "Error sending response\n");
	}
	printf("num bytes sent to client: %ld\n", nwrite);

	close(sfd);
}

int complete_request_received(char *request)
{
	if (strstr(request, "\r\n\r\n") != NULL)
	{
		return 1;
	}
	return 0;
}

int parse_request(char *request, char *method,
				  char *hostname, char *port, char *path)
{
	if (!complete_request_received(request))
	{
		return 0;
	}

	char req_str[MAX_OBJECT_SIZE];
	strcpy(req_str, request);
	char *url;
	char ptr[129];
	char *ptr2;
	char pathPtr[129];
	// get method
	strcpy(method, strtok(req_str, " "));

	// get and parse URL
	url = strtok(NULL, " ");

	strcpy(ptr, strstr(url, "//") + 2);

	if ((ptr2 = strstr(ptr, ":")) != NULL)
	{ // custom port

		strcpy(pathPtr, strstr(ptr, "/"));
		strcpy(hostname, strtok(ptr, ":"));

		strcpy(port, strtok(ptr2 + 1, "/"));

		strcpy(path, strtok(pathPtr, " "));
	}
	else
	{ // default port

		strcpy(pathPtr, strstr(ptr, "/"));
		strcpy(hostname, strtok(ptr, "/"));

		strcpy(port, "80");

		strcpy(path, strtok(pathPtr, " "));
	}

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
