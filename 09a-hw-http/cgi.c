#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

int main(int argc, char *argv[]) {
    // Send the first line and initial headers associated with the HTTP response
    printf("Content-Type: text/html\n\n");


    // Set the value of the QUERY_STRING environment variable to the value of the query string sent by the client
    char *query_string = getenv("QUERY_STRING");
    printf(query_string);
    // Set the value of the CONTENT_LENGTH environment variable to the value of the Content-Length header sent by the client
    // char *content_length_str = getenv("CONTENT_LENGTH");
    // int content_length = atoi(content_length_str);
    // char *content = malloc(content_length + 1);
    // fread(content, content_length, 1, stdin);
    // content[content_length] = '\0';
    
    // char *response_body = malloc(1024);
    // sprintf(response_body, "Hello CS324\nQuery string: %s\nRequest body: %s\n", query_string, content);
    
    // printf("Content-Type: text/plain\r\n");
    // printf("Content-Length: %ld", strlen(response_body));
    // printf("\r\n\r\n");
    // // Send the response body
    // printf("%s", response_body);
    return 0;
}
