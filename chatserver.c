#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>
#include <sys/types.h>
#include <signal.h>

#define MAX_CLIENTS 100
#define BUFFER_SZ 2048

/* Estructura del cliente */
typedef struct {
    struct sockaddr_in addr; /* Client remote address */
    int connfd;              /* Connection file descriptor */
    int uid;                 /* Client unique identifier */
    char name[32];           /* Client name */
} client_t;

client_t *clients[MAX_CLIENTS];
int cli_count, uid;

pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;


pthread_mutex_t topic_mutex = PTHREAD_MUTEX_INITIALIZER;

/* Print ip address */
void print_client_addr(struct sockaddr_in addr){
    printf("%d.%d.%d.%d",
        addr.sin_addr.s_addr & 0xff,
        (addr.sin_addr.s_addr & 0xff00) >> 8,
        (addr.sin_addr.s_addr & 0xff0000) >> 16,
        (addr.sin_addr.s_addr & 0xff000000) >> 24);
}

void add_cli(client_t * cli){
    pthread_mutex_lock(&clients_mutex);
    for(int i = 0 ; i < MAX_CLIENTS; i++){
        if(!clients[i]){
            clients[i] = cli;
            break;
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}
void delete_cli(int uid){
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; ++i) {
        if (clients[i]) {
            if (clients[i]->uid == uid) {
                clients[i] = NULL;
                break;
            }
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

void send_message_self(const char *s, int connfd){
    if (write(connfd, s, strlen(s)) < 0) {
        perror("Write to descriptor failed");
        exit(-1);
    }
}

void send_active_clients(int connfd){
    char s[64];

    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; ++i){
        if (clients[i]) {
            sprintf(s, "<< [%d] %s\r\n", clients[i]->uid, clients[i]->name);
            send_message_self(s, connfd);
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

void send_message_all(char *s){
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i <MAX_CLIENTS; ++i){
        if (clients[i]) {
            if (write(clients[i]->connfd, s, strlen(s)) < 0) {
                perror("Write to descriptor failed");
                break;
            }
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

void send_message_except(char *s, int uid){
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; ++i) {
        if (clients[i]) {
            if (clients[i]->uid != uid) {
                if (write(clients[i]->connfd, s, strlen(s)) < 0) {
                    perror("Write to descriptor failed");
                    break;
                }
            }
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

void send_message_client(char *s, int uid){
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; ++i){
        if (clients[i]) {
            if (clients[i]->uid == uid) {
                if (write(clients[i]->connfd, s, strlen(s))<0) {
                    perror("Write to descriptor failed");
                    break;
                }
            }
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}


char *allocate(const char *s) {
    size_t size = strlen(s) + 1;
    char *p = malloc(size);
    if (p) {
        memcpy(p, s, size);
    }
    return p;
}

void *handle_cli (void *arg){
    char buff_out[BUFFER_SZ];
    char buff_in[BUFFER_SZ / 2];
    int len;

    cli_count++;
    client_t *cli = (client_t *)arg;

    printf(" accept ");
    print_client_addr(cli->addr);
    printf(" referenced by %d\n", cli->uid);

    sprintf(buff_out, "<< %s has joined\r\n", cli->name);
    send_message_all(buff_out);

    //send_message_self("<< see /help for assistance\r\n", cli->connfd);

    while ((len = read(cli->connfd, buff_in, sizeof(buff_in) - 1)) > 0) {
        buff_in[len] = '\0';
        buff_out[0] = '\0';

        if (!strlen(buff_in)) {
            continue;
        }

        if (buff_in[0] == '/') {
            char *command, *param;
            command = strtok(buff_in," ");
            if (!strcmp(command, "/quit")) {
                break;
            } else if(!strcmp(command, "/list")) {
                sprintf(buff_out, "<< clients %d\r\n", cli_count);
                send_message_self(buff_out, cli->connfd);
                send_active_clients(cli->connfd);
            }else if (!strcmp(command, "/nick")) {
                //printf("here\n");
                param = strtok(NULL, " ");
                if (param) {
                    char *old_name = allocate(cli->name);
                    if (!old_name) {
                        perror("Cannot allocate memory");
                        continue;
                    }
                    strncpy(cli->name, param, sizeof(cli->name));
                    cli->name[sizeof(cli->name)-1] = '\0';
                    sprintf(buff_out, "<< %s is now known as %s\r\n", old_name, cli->name);
                    free(old_name);
                    send_message_all(buff_out);
                } else {
                    send_message_self("<< name cannot be null\r\n", cli->connfd);
                }
            } else if (!strcmp(command, "/msg")) {
                param = strtok(NULL, " ");
                if (param) {
                    int uid = atoi(param);
                    param = strtok(NULL, " ");
                    if (param) {
                        sprintf(buff_out, "Private Mesage from %s:", cli->name);
                        while (param != NULL) {
                            strcat(buff_out, " ");
                            strcat(buff_out, param);
                            param = strtok(NULL, " ");
                        }
                        strcat(buff_out, "\n");
                        send_message_client(buff_out, uid);
                    } else {
                        send_message_self("<< message cannot be null\r\n", cli->connfd);
                    }
                } else {
                    send_message_self("<< reference cannot be null\r\n", cli->connfd);
                }
            } else {
                send_message_self("<< unknown command\r\n", cli->connfd);
            }
        } else {
            /* Send message */
            snprintf(buff_out, sizeof(buff_out), "[%s] %s\r\n", cli->name, buff_in);
            send_message_except(buff_out, cli->uid);
        }
    }

    sprintf(buff_out, "<< %s has left\n", cli->name);
    send_message_all(buff_out);
    close(cli->connfd);
    
    delete_cli(cli->uid);
    printf("<< quit ");
    print_client_addr(cli->addr);
    
    printf(" referenced by %d\n", cli->uid);
    free(cli);
    cli_count--;
    pthread_detach(pthread_self());

    return NULL;
}

int main(int argc, char *argv[]){
    int listenfd = 0, connfd = 0;
    struct sockaddr_in serv_addr;
    struct sockaddr_in cli_addr;
    pthread_t tid;

    /* Socket settings */
    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(5000);

    /* Bind */
    if (bind(listenfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Socket binding failed");
        return EXIT_FAILURE;
    }

    /* Listen */
    if (listen(listenfd, 10) < 0) {
        perror("Socket listening failed");
        return EXIT_FAILURE;
    }

    printf("SERVER SERVICE STARTED\n");

    while (1){
        socklen_t clilen = sizeof(cli_addr);
        connfd = accept(listenfd, (struct sockaddr*)&cli_addr, &clilen);

        if ((cli_count + 1) == MAX_CLIENTS) {
            printf("<< max clients reached\n");
            printf("<< reject ");
            print_client_addr(cli_addr);
            printf("\n");
            close(connfd);
            continue;

        }

        client_t *cli = (client_t *)malloc(sizeof(client_t));
        cli->addr = cli_addr;
        cli->connfd = connfd;
        cli->uid = uid++;
        sprintf(cli->name, "%d", cli->uid);
        
        add_cli(cli);
        pthread_create(&tid, NULL, &handle_cli, (void*)cli);

        sleep(1);

    }
}
