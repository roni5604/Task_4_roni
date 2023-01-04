#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/socket.h>
#include <resolv.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/ip_icmp.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <strings.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

#define PORT 3000      // define the port number to 3000
#define TIMEOUT 10     // define the timeout to 10 seconds
#define BUFFER_SIZE 64 // define the buffer size to 1024 bytes

int main(int argc, char *argv[])
{
    printf("hello partb watchdog\n");

    if (argc < 1)
    {
        // printf("watchdog Usage: ping <server ip>\n");
        printf("watchdog cant open well\n");
        exit(1);
    }

    struct hostent *server;   // hostent is a structure that will be used to store information about a given host
    char buffer[BUFFER_SIZE]; // buffer to store the message
    int bytes_res, bind_bytes, listen_bytes;
    time_t start, end;    // time_t is used to store time values
    double time_past = 0; // create a double to store the time past
    struct sockaddr_in server_addr;
    int socket_fd;
    socket_fd = socket(AF_INET, SOCK_STREAM, 0); // create a socket tcp socket
    if (socket_fd < 0)
    {
        perror("watchdog: ERROR opening socket");
        exit(1);
    }
    else
    {
        // printf("watchdog: socket created\n");
    }

    bzero((char *)&server_addr, sizeof(server_addr)); // set the server_addr to zero
    // memset((char *)&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET; // AF_INET is the address family for IPv4
    // bcopy is used to copy the contents of one buffer to another
    // bcopy((char *)server->h_addr, (char *)&server_addr.sin_addr.s_addr, server->h_length); // copy the server ip address to the server_addr
    // server_addr.sin_addr.s_addr = server->h_addr; // copy the server ip address to the server_addr
    server_addr.sin_addr.s_addr = INADDR_ANY; // copy the server ip address to the server_addr
    server_addr.sin_port = htons(PORT);       // convert the port number to network byte order
    ////////
    //////
    //////
    int yes=1;
        if(setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes)==-1)
        {
            perror("setsockopt");
            exit(1);
        }
/////
/////
//////
/////

    // bind the socket to the port
    bind_bytes = bind(socket_fd, (struct sockaddr *)&server_addr, sizeof(server_addr));
    if (bind_bytes < 0)
    {
        perror("watchdog: ERROR on binding socket to port ");
        exit(1);
    }
    // printf("watchdog: socket binded \n");

    // listen for incoming connections
    listen_bytes = listen(socket_fd, 5);
    if (listen_bytes < 0)
    {
        perror("watchdog: ERROR on listening ");
        exit(1);
    }
    printf("watchdog: socket listening \n");

    // server = gethostbyname(argv[1]); // get server ip address
    // if (server == NULL)
    // {
    //     fprintf(stderr, "watchdog: ERROR, no such host\n");
    //     exit(1);
    // }
    // printf("watchdog: server ip address is %s \n", argv[1]);

    // accept the connection from the server
    socklen_t server_addr_size = sizeof(server_addr);
    int bytes_accept = accept(socket_fd, (struct sockaddr *)&server_addr, &server_addr_size);
    printf("watchdog: bytes_accept=%d\n", bytes_accept);
    if (bytes_accept < 0)
    {
        perror("watchdog: ERROR on accept");
        exit(1);
    }
    printf("watchdog: connection accepted \n");
    
    fcntl(socket_fd, F_SETFL, O_NONBLOCK); // Set the socket into non-blocking state
    fcntl(bytes_accept, F_SETFL, O_NONBLOCK); // Set the socket into non-blocking state



    // start the loop to send and receive messages
    while (1)
    {
        
        bzero(buffer, BUFFER_SIZE); // set the buffer to zero
        time(&start);               // time is used to get the current time of the system
        bytes_res = recv(socket_fd, buffer, BUFFER_SIZE, 0);
        printf("watchdog: bytes_res=%d\n", bytes_res);
        if (bytes_res < 0)
        {
            perror("watchdog: ERROR receiving from socket");
            // exit(1);
        }
        printf("watchdog: receive message from better ping\n");
    
        char reply_message[64] = "";
        // strcmp is used to compare two strings
        printf("watchdog: strcpy=%d, buffer=%s\n", strcmp(buffer, "ICMP-ECHO-REPLY"), buffer);
        if (strcmp(buffer, "ICMP-REQUEST") == 0) // if the server is alive
        {
            time(&end); // time is used to get the current time of the system
            // printf("watchdog: Server %s responded.\n", argv[1]);
            time_past = difftime(end, start); // difftime is used to calculate the difference between two times
            printf("watchdog: time past: %f seconds to response \n", time_past);
            if (time_past > TIMEOUT) // if the time past is greater than the timeout time then the server is not reachable
            {
                memcpy(reply_message, "timeout", strlen("timeout"));
                // bytes_res = send(socket_fd, "timeout", strlen("timeout"), 0); // send message to server to check if it is alive
                bytes_res = send(socket_fd, reply_message, strlen("timeout"), 0); // send message to server to check if it is alive
                // printf("watchdog: Server %s cannot be reached from reply .\n", argv[1]);
                exit(1);
            }
            else
            {
                memcpy(reply_message, "ICMP-ECHO-REPLY", strlen("ICMP-ECHO-REPLY"));
                // bytes_res = send(socket_fd, "ICMP-ECHO-REPLY", strlen("ICMP-ECHO-REPLY"), 0); // send message to server to check if it is alive
                bytes_res = send(socket_fd, reply_message, strlen("ICMP-ECHO-REPLY"), 0); // send message to server to check if it is alive
                if (bytes_res < 0)
                {
                    perror("watchdog: ERROR sending to socket");
                    exit(1);
                }
            }
        }
        else
        {
            
            printf("watchdog: Server %s cannot be reached from -------------------request.\n", argv[1]);
            exit(1);
        }
        bzero(buffer, BUFFER_SIZE); // set the buffer to zero
        sleep(1);                   // sleep for 1 second
    }
    close(socket_fd); // close the socket
    exit(0);          // done the program
}

// int main()
// {
//     // printf("hello partb");
//     // while (timer < 10seconds)
//     // {
//     //     recv();
//     //     timer = 0seconds;
//     // }
//     // send("timeout")

//         return 0;
// }