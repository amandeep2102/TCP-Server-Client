#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <string.h>
#define h_addr h_addr_list[0]

char **tokens = NULL;

void error(char *msg)
{
    perror(msg);
    exit(0);
}

void tokenize(char *input)
{
    tokens = malloc(5 * sizeof(char *));

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
        tokens[i] = malloc(strlen(token) + 1);
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

int sendfunInt(int sockfd, int *var)
{
    int n;
    n = write(sockfd, var, sizeof(int));
    return n;
}

int main(int argc, char *argv[])
{
    int alreadyconnected = 0;
    char input[256];
    int clisockfd, portno;
    struct hostent *server;
    struct sockaddr_in serv_addr;
    FILE *fp;
    char line[1024];

    if (argc < 2)
    {
        printf("Invalid number of arguments Usage: ./<Progname> interactive or batch <file>\n");
        exit(1);
    }

    if (strcmp(argv[1], "batch") == 0)
    {
        if (argc < 3)
        {
            perror("Invalid number of arguments");
            exit(1);
        }

        fp = fopen(argv[2], "r");
    }
    else if (strcmp(argv[1], "interactive") != 0)
    {
        perror("Wrong option!");
        exit(1);
    }

    while (1)
    {
        if (strcmp(argv[1], "batch") == 0)
        {
            if (feof(fp))
            {
                exit(0);
            }
            if (fgets(input, sizeof(input), fp) == NULL)
            {
                perror("Error reading from the file!");
                exit(1);
            }
        }
        else if (strcmp(argv[1], "interactive") == 0)
        {
            printf("> ");
            if (fgets(input, sizeof(input), stdin) == NULL)
            {
                perror("Error reading input\n");
                exit(1);
            }
        }

        input[strcspn(input, "\n")] = '\0';

        tokenize(input);
        // for (int i = 0; i < 5; i++)
        // {
        //     printf("token %d: %s\n", i, tokens[i]);
        // }

        if (strcmp(tokens[0], "connect") == 0)
        {
            if (alreadyconnected)
            {
                printf("Already Connected!!");
                continue;
            }

            if (tokens[2] == NULL)
            {
                perror("Wrong Number of arguments");
                continue;
            }

            portno = atoi(tokens[2]);
            clisockfd = socket(AF_INET, SOCK_STREAM, 0);

            if (clisockfd == -1)
            {
                perror("Error opening socket");
            }

            // putting server info
            server = gethostbyname(tokens[1]);
            printf("Hostname is: %s\n", server->h_name);

            if (server == NULL)
            {
                perror("Server Unreachable: ");
            }

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
            alreadyconnected = 1;
        }
        else if (strcmp(tokens[0], "disconnect") == 0)
        {
            if (alreadyconnected)
            {
                close(clisockfd);
                clisockfd = -1;
                alreadyconnected = 0;
                printf("Disconnected.\n");
            }
            else
            {
                printf("Not connected.\n");
            }
        }
        else
        {
            int notokens = 0, n;
            for (int i = 0; i < 5; i++)
            {
                if (tokens[i] == NULL)
                {
                    break;
                }
                notokens++;
            }

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
            }
            else if (strcmp(tokens[0], "update") == 0)
            {
                char *msg;
                receive_string(clisockfd, &msg);
                printf("%s\n", msg);
            }
            else if (strcmp(tokens[0], "delete") == 0)
            {
                char *msg;
                receive_string(clisockfd, &msg);
                printf("%s\n", msg);
            }
            else if(strcmp(tokens[0], "create") == 0){
                char *msg;
                receive_string(clisockfd, &msg);
                printf("%s\n", msg);
            }
        }

        for (int i = 0; i < 5; i++)
        {
            if (tokens[i])
                free(tokens[i]);
        }
        free(tokens);
        tokens = NULL;
    }
    close(clisockfd);
    return 0;
}
