#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include "uthash.h"

#define MAX_EVENTS 16

void error(char *msg)
{
    perror(msg);
    exit(1);
}

typedef struct
{
    int key;     // key
    char *value; // value
    UT_hash_handle hh;
} kv_pair;

kv_pair *table = NULL;

int update_pair(int key, const int valuesize, const char *new_value)
{
    kv_pair *s;
    HASH_FIND_INT(table, &key, s);
    if (s)
    {
        free(s->value);
        s->value = malloc(valuesize + 1);
        strcpy(s->value, new_value);
        s->value[valuesize] = '\0';
        return 0;
    }
    else
    {
        return -1;
    }
}

void add_pair(const int *key, const int valuesize, const char *value)
{
    kv_pair *s = malloc(sizeof(kv_pair));
    s->key = *key;
    s->value = malloc(valuesize + 1);
    strcpy(s->value, value);
    s->value[valuesize] = '\0';
    HASH_ADD_INT(table, key, s);
}

char *get_value(const int key)
{
    kv_pair *s;
    HASH_FIND_INT(table, &key, s);
    return s ? s->value : NULL;
}

int delete_pair(int key)
{
    kv_pair *s;
    HASH_FIND_INT(table, &key, s);
    if (s)
    {
        HASH_DEL(table, s);
        free(s->value);
        free(s);
        return 0;
    }
    else
    {
        return -1;
    }
}

void delete_table()
{
    kv_pair *s, *tmp;
    HASH_ITER(hh, table, s, tmp)
    {
        HASH_DEL(table, s);
        free(s->value);
        free(s);
    }
}

char *concat_strings(int count, ...)
{
    va_list args;
    va_start(args, count);

    int total_len = 0;
    for (int i = 0; i < count; i++)
    {
        total_len += strlen(va_arg(args, const char *));
    }

    va_end(args);
    char *result = malloc(total_len + 1);
    if (!result)
        return NULL;
    result[0] = '\0';

    va_start(args, count);
    for (int i = 0; i < count; i++)
    {
        strcat(result, va_arg(args, const char *));
    }
    va_end(args);

    return result;
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
    *out = malloc(len + 1);
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

void print_table()
{
    kv_pair *entry, *tmp;
    printf("-------------PRINTING THE KEY VALUE PAIRS-----------------\n");
    HASH_ITER(hh, table, entry, tmp)
    {
        printf("Key: %d, Value: %s\n", entry->key, entry->value);
    }
}

int main(int argc, char *argv[])
{
    struct sockaddr_in serv_addr, cli_addr;
    int sockfd, portno, clilen, clisockfd;
    char **tokens;

    int epollfd, nfds;
    struct epoll_event events[MAX_EVENTS], event;

    if (argc < 2)
    {
        error("Incorrect number of arguments: ");
    }

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1)
    {
        error("Error opening Socket: ");
    }

    memset((char *)&serv_addr, 0, sizeof(serv_addr));
    portno = atoi(argv[1]);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(portno);

    if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        error("Error binding: ");
    }

    fcntl(sockfd, F_SETFL, O_NONBLOCK);

    listen(sockfd, 5);
    clilen = sizeof(cli_addr);

    epollfd = epoll_create1(0);
    event.events = EPOLLET | EPOLLIN | EPOLLOUT;
    event.data.fd = sockfd;

    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, sockfd, &event) < 0)
    {
        perror("epollctl failed to add fd");
    }

    while (1)
    {
        nfds = epoll_wait(epollfd, events, MAX_EVENTS, -1);
        if (nfds == -1)
        {
            perror("epoll wait failed!");
        }

        for (int i = 0; i < nfds; i++)
        {
            if (events[i].data.fd == sockfd)
            {
                clisockfd = accept(sockfd, (struct sockaddr *)&cli_addr, &clilen);
                if (clisockfd < 0)
                {
                    perror("Error Accepting the client connection: ");
                }
                send_string(clisockfd, "OK");

                fcntl(clisockfd, F_SETFL, O_NONBLOCK);
                event.events = EPOLLIN | EPOLLET;
                event.data.fd = clisockfd;

                if (epoll_ctl(epollfd, EPOLL_CTL_ADD, clisockfd, &event) == -1)
                {
                    perror("error on adding clisocfd epoll ctl");
                }
            }
            else if (events[i].events & EPOLLIN)
            {
                int ntokens = 0;
                tokens = malloc(5 * sizeof(char *));
                for (int i = 0; i < 5; i++)
                {
                    tokens[i] = NULL;
                }

                if (receivefunInt(clisockfd, &ntokens) <= 0)
                {
                    printf("client is disconnected!\n");
                    break;
                }

                printf("ntokens %d\n", ntokens);

                for (int i = 0; i < ntokens; i++)
                {
                    if (receive_string(clisockfd, &tokens[i]) < 0)
                    {
                        perror("error reading: ");
                    }
                }

                for (int i = 0; i < ntokens; i++)
                {
                    printf("token %d %s \n", i, tokens[i]);
                }

                if (strcmp(tokens[0], "create") == 0)
                {
                    if (ntokens < 4)
                    {
                        perror("Invalid number of arguments!\n");
                        continue;
                    }
                    int key = atoi(tokens[1]);
                    int valuesize = atoi(tokens[2]);
                    add_pair(&key, valuesize, tokens[3]);
                }
                else if (strcmp(tokens[0], "read") == 0)
                {
                    if (ntokens < 2)
                    {
                        perror("Invalid number of arguments!\n");
                        continue;
                    }
                    int key = atoi(tokens[1]);
                    char *value = NULL;
                    value = get_value(key);

                    if (value == NULL)
                    {
                        send_string(clisockfd, "ERROR KEY DOES NOT EXIST");
                    }
                    else
                    {
                        char *msg;
                        msg = concat_strings(4, "key -> ", tokens[1], " value -> ", value);
                        printf("sending %s\n", msg);
                        send_string(clisockfd, msg);
                    }
                }
                else if (strcmp(tokens[0], "update") == 0)
                {
                    if (ntokens < 4)
                    {
                        perror("Invalid number of arguments!\n");
                        continue;
                    }
                    int key = atoi(tokens[1]);
                    int valuesize = atoi(tokens[2]);

                    if (update_pair(key, valuesize, tokens[3]) < 0)
                    {
                        send_string(clisockfd, "ERROR KEY DOES NOT EXIST");
                    }
                    else
                    {
                        send_string(clisockfd, "SUCCESS");
                    }
                }
                else if (strcmp(tokens[0], "delete") == 0)
                {
                    int key = atoi(tokens[1]), out;
                    out = delete_pair(key);
                    if (out < 0)
                    {
                        send_string(clisockfd, "ERROR KEY DOESN'T EXIST");
                    }
                    else
                    {
                        send_string(clisockfd, "SUCCESS");
                    }
                }
            }
        }
    }

    int key = 5;
    char *value = "anyuthoing";
    add_pair(&key, strlen(value), value);

    print_table();
    close(sockfd);

    if (clisockfd != -1)
    {
        close(clisockfd);
    }

    return 0;
}
