#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <pthread.h>
#define h_addr h_addr_list[0]

#define NUM_CLIENTS 1000

#define createcomm 1
#define deletecomm 2
#define updatecomm 3
#define NUM_ITERATIONS 10000

long long resptime[NUM_CLIENTS * NUM_ITERATIONS];

void error(char *msg)
{
    perror(msg);
    exit(0);
}

void tokenize(char *input)
{
    char **tokens = NULL;
    tokens = (char **)malloc(5 * sizeof(char *));

    for (int i = 0; i < 5; i++)
    {
        tokens[i] = NULL;
    }

    const char *delim = " "; // space as delimiter
    char *token = strtok(input, delim);
    int i = 0;
    while (token != NULL)
    {
        // printf("%s\n", token);
        tokens[i] = (char *)malloc(strlen(token) + 1);
        strcpy(tokens[i], token);
        token = strtok(NULL, delim); // continue tokenizing
        i++;
    }
}

int send_string(int sockfd, const char *str)
{
    int len = strlen(str);
    if (write(sockfd, &len, sizeof(int)) != sizeof(int))
    {
        perror("Error writing string length");
        return -1;
    }
    if (write(sockfd, str, len) != len)
    {
        perror("Error writing string data");
        return -1;
    }
    return 0; // success
}

int receive_string(int sockfd, char **out)
{
    int len;
    if (read(sockfd, &len, sizeof(int)) != sizeof(int))
    {
        perror("Error reading string length");
        return -1;
    }
    *out = (char *)malloc(len + 1);
    if (!*out)
        return -1;

    if (read(sockfd, *out, len) != len)
    {
        perror("Error reading string data");
        free(*out);
        return -1;
    }
    (*out)[len] = '\0';
    return 0; // success
}

int receivefunInt(int sockfd, int *var)
{
    int n;
    n = read(sockfd, var, sizeof(int));
    return n;
}

int sendfunInt(int sockfd, int *var)
{
    int n;
    n = write(sockfd, var, sizeof(int));
    return n;
}

void *multiclient(void *arg)
{
    char **tokens = NULL;
    int threadid = *((int *)arg);
    char *host = "localhost";

    // printf("inside thread %d \n", threadid);
    struct timeval start, end;
    int clisockfd;
    int portno = 5000;
    struct hostent *server;
    struct sockaddr_in serv_addr;

    clisockfd = socket(AF_INET, SOCK_STREAM, 0);

    if (clisockfd == -1)
    {
        perror("Error opening socket");
    }

    // putting server info
    server = gethostbyname(host);
    if (server == NULL)
    {
        perror("Server Unreachable: ");
    }
    printf("Hostname is: %s\n", server->h_name);

    memset((char *)&serv_addr, 0, sizeof(serv_addr));
    memcpy(&serv_addr.sin_addr.s_addr, server->h_addr, server->h_length);
    serv_addr.sin_port = htons(portno);
    serv_addr.sin_family = AF_INET;

    // connecting to the server

    if (connect(clisockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == -1)
    {
        perror("Error connecting to the server: ");
    }

    char *buffer;
    receive_string(clisockfd, &buffer);
    printf("%s \n", buffer);
    free(buffer);

    int comm;
    int notokens;

    tokens = malloc(4 * sizeof(char *));
    for (int i = 0; i < 4; i++)
    {
        tokens[i] = malloc(256 * sizeof(char));
    }

    int q = 0;
    char strkey[32];

    for (long long i = 0; i < NUM_CLIENTS * NUM_ITERATIONS; i++)
    {
        resptime[i] = 0;
    }

    for (int i = 0; i < NUM_ITERATIONS; i++)
    {
        comm = rand() % 3 + 1;
        printf("generated rand %d \n", comm);
        notokens = 0;

        if (comm == createcomm)
        {
            notokens = 4;
            sprintf(strkey, "%d", q);
            strcpy(tokens[0], "create");
            strcpy(tokens[1], strkey);
            strcpy(tokens[2], "10");
            strcpy(tokens[3], "abcdefghij");
        }
        else if (comm == updatecomm)
        {
            notokens = 4;
            sprintf(strkey, "%d", q > 0 ? q - 1 : 0);
            strcpy(tokens[0], "update");
            strcpy(tokens[1], strkey);
            strcpy(tokens[2], "10");
            strcpy(tokens[3], "qwertyuiop");
        }
        else if (comm == deletecomm)
        {
            notokens = 2;
            sprintf(strkey, "%d", q > 0 ? q - 1 : 0);
            strcpy(tokens[0], "delete");
            strcpy(tokens[1], strkey);
        }
        q++;

        // start time
        gettimeofday(&start, NULL);
        sendfunInt(clisockfd, &notokens);
        for (int i = 0; i < notokens; i++)
        {
            send_string(clisockfd, tokens[i]);
        }

        if (strcmp(tokens[0], "read") == 0)
        {
            char *value;
            receive_string(clisockfd, &value);
            printf("%s\n", value);
            free(value);
        }
        else if (strcmp(tokens[0], "update") == 0 || strcmp(tokens[0], "create") == 0 || strcmp(tokens[0], "delete") == 0)
        {
            char *msg;
            receive_string(clisockfd, &msg);
            // printf("%s\n", msg);
            free(msg);
        }

        // end time
        gettimeofday(&end, NULL);
        resptime[threadid * NUM_ITERATIONS + i] = (end.tv_sec - start.tv_sec) * 1000000LL + (end.tv_usec - start.tv_usec);
    }

    close(clisockfd);
    clisockfd = -1;
    printf("Disconnected.\n");

    for (int i = 0; i < notokens; i++)
    {
        if (tokens[i])
            free(tokens[i]);
    }
    free(tokens);
    tokens = NULL;
    return 0;
}

int main(int argc, char *argv[])
{
    srand(time(NULL));
    pthread_t clients[NUM_CLIENTS];
    int clientid[NUM_CLIENTS];

    for (int i = 0; i < NUM_CLIENTS; i++)
    {
        clientid[i] = i;
    }

    for (int i = 0; i < NUM_CLIENTS; i++)
    {
        pthread_create(&clients[i], NULL, multiclient, &clientid[i]);
    }

    for (int i = 0; i < NUM_CLIENTS; i++)
    {
        pthread_join(clients[i], NULL);
    }

    double totaltime = 0, avg_time;
    long totalqueries = NUM_CLIENTS * NUM_ITERATIONS;

    for (int i = 0; i < NUM_CLIENTS * NUM_ITERATIONS; i++)
    {
        totaltime += (double)resptime[i];
    }
    avg_time = totaltime / totalqueries;

    printf("average time taken for each query is %.2f \n", avg_time);

    return 0;
}
