#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <sys/types.h>
#include <netdb.h>
#include <pthread.h>

/* Recommended max object size */
#define MAX_OBJECT_SIZE 102400

static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (Macintosh; Intel Mac OS X 10.15; rv:97.0) Gecko/20100101 Firefox/97.0";

int complete_request_received(char *);
int parse_request(char *, char *, char *, char *, char *);
void test_parser();
void print_bytes(unsigned char *, int);

int main(int argc, char *argv[])
{
    // test_parser();
    // struct addrinfo *p, *listp, hints;
    // char buf[MAXLINE], method[16], hostname[64], port[8], path[64];

    // if (argc != 2)
    // {
    //     fprintf(stderr, "usage: %s <domain name>\n", argv[0]);
    //     exit(0);
    // }

    // memset(&hints, 0, sizeof(struct addrinfo));
    // hints.ai_socktype = SOCK_STREAM;
    // hints.ai_flags = AI_PASSIVE | AI_ADDRCONFIG;
    // hints.ai_flags = AI_NUMERICSERV;

    // if (getaddrinfo(argv[1], NULL, &hints, &listp) != 0)
    // {
    //     fprintf(stderr, "getaddrinfo error\n");
    //     exit(0);
    // }

    // for (p = listp; p != NULL; p = p->ai_next)
    // {
    //     if (getnameinfo(p->ai_addr, p->ai_addrlen, buf, MAXLINE, NULL, 0, NI_NUMERICHOST) == 0)
    //     {
    //         printf("%s\n", buf);
    //     }
    // }

    // freeaddrinfo(listp);

    printf("%s\n", user_agent_hdr);
    return 0;
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

    return 0;
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