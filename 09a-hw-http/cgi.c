#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

int main(int argc, char *argv[]) {
    // Send the first line and initial headers associated with the HTTP response
    printf("Content-Type: text/html\n\n");

    // Fork to create a child process
    pid_t pid = fork();

    if (pid == 0) {
        // Child process
        // Duplicate the socket onto standard input for reading the request body from the client
        dup2(STDIN_FILENO, 0);

        // Duplicate the socket onto standard output for writing the remaining response headers and the response body
        dup2(STDOUT_FILENO, 1);

        // Set the value of the QUERY_STRING environment variable to the value of the query string sent by the client
        char *query_string = getenv("QUERY_STRING");
        setenv("QUERY_STRING", query_string, 1);

        // Set the value of the CONTENT_LENGTH environment variable to the value of the Content-Length header sent by the client
        char *content_length_str = getenv("CONTENT_LENGTH");
        int content_length = atoi(content_length_str);
        char *content = malloc(content_length + 1);
        fread(content, content_length, 1, stdin);
        content[content_length] = '\0';
        setenv("CONTENT_LENGTH", content_length_str, 1);

        // Create the response body
        char *response_body = malloc(1024);
        sprintf(response_body, "Hello CS324\nQuery string: %s\nRequest body: %s\n", query_string, content);
        // Send "Content-Type" and "Content-Length" headers of the HTTP response to the client
        printf("Content-Type: text/plain\r\n");
        printf("Content-Length: %ld", strlen(response_body));
        printf("\r\n\r\n");
        // Send the response body
        printf("%s", response_body);

        // Execute the program in the child process
        execve(argv[0], argv, NULL);

        // If execve returns, it must have failed
        perror("execve failed");
        exit(1);
    } 

    return 0;
}
