#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/types.h> 
#include <unistd.h> 
#include <sys/stat.h> 
#include <fcntl.h> 
#include <sys/wait.h>

#define BUFFER_SZ 2048

int main(int argc, char *argv[]) {
    int sockfd, portno, n, len;
    struct sockaddr_in serv_addr;
    struct hostent *server;
    char username[200], msg[200];
    char buff_out[BUFFER_SZ];
    char buff_in[BUFFER_SZ / 2];
    pid_t pid;

    char buffer[256];

    if (argc < 3) {
        fprintf(stderr,"usage %s hostname port\n", argv[0]);
        exit(0);
    }

    portno = atoi(argv[2]);

    /* Create a socket point */
    sockfd = socket(AF_INET, SOCK_STREAM, 0);

    if (sockfd < 0) {
        perror("ERROR opening socket");
        exit(1);
    }

    server = gethostbyname(argv[1]);

    if (server == NULL) {
        fprintf(stderr,"ERROR, no such host\n");
        exit(0);
    }

    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy((char *)server->h_addr_list[0], (char *)&serv_addr.sin_addr.s_addr, server->h_length);
    serv_addr.sin_port = htons(portno);

    /* Now connect to the server */
    if (connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("ERROR connecting");
        exit(1);
    }

    strcpy(username,"/nick ");

    printf("Write yout user name no more then 200 characters\n");
    scanf("%s", username + 6);
    strcpy(buff_out, username);

    if (write(sockfd, buff_out , strlen(username)) < 0) {
                perror("Write to descriptor failed");
            }
    pid = fork (); 
    if (pid  == (pid_t) 0) {
        printf("Comands\n\r\
        /mssg to write all users\n\r\
        /user to write a user and the userid and the message\n\r\
        /list to list users and their id\n\r\
        /quit to exit\n");
        while (1){
            gets(msg);
            char comand[6];
            char uid[10];
            strncpy(comand,msg,5);
            /* Comands */
            if(!strcmp(comand, "/quit")){
                strcpy(buff_out, "/quit");    
                //printf("debug: %s\n",buff_out);
            } else if(!strcmp(comand, "/list")){
                strcpy(buff_out, "/list");
                //printf("debug: %s\n",buff_out);
            }
            else if(!strcmp(comand, "/mssg")){
                strcpy(buff_out, msg+6);
                //printf("debug: %s\n",buff_out);
            }
            else if(!strcmp(comand, "/user")){
                sscanf(msg+5,"%s", uid);
                sprintf(buff_out, "/msg %s %s", uid, msg+ 7 + strlen(uid));
                //printf("debug: %s\n",buff_out);
            }else
            {
                buff_out[0]= '\0';
                //printf("debug: %s\n",buff_out);
            }
            
            /* Send message */
            if (write(sockfd, buff_out , strlen(buff_out)) < 0) {
                perror("Write to descriptor failed");
            }
            if(!strcmp(comand, "/quit"))
                break; 
        }
    }
    while (1){            
        if((len = read(sockfd, buff_in, sizeof(buff_in) - 1)) > 0){
            buff_in[len] = '\0';
            printf("%s", buff_in);
        }
        if(waitpid (pid, NULL, WNOHANG) == pid)
                break;
    }

    
    
    close(sockfd);
    return 0;
}