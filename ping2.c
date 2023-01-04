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
#include <stdbool.h>

#define PACKET_SIZE 64 // define the packet size
#define BUFFER_SIZE 1500
#define PORT 3000

double time_to_recv;             // create a double to store the time to receive the packet
clock_t S_time;                  // create a clock_t to store the start time
int process_id = 1;              // create an int to store the process id
bool new_ping_message = true;    // create a bool to store if it is a new ping message or not
char buf[BUFFER_SIZE];           // buffer to store the message from the watchdog
int ping_socket;                 // socket to send and receive packets
unsigned char buffer_ping[1024]; // buffer to store the ping message

struct packet // define the packet structure to send and receive packets
{
    struct icmphdr hdr; // ICMP header structure
    // char msg[PACKET_SIZE - sizeof(struct icmphdr)]; // message to send
};

// Calculate the checksum of the ICMP header
unsigned short calculate_checksum(void *b, int len)
{
    unsigned short *buf = b;
    unsigned int sum = 0;
    unsigned short result;

    for (sum = 0; len > 1; len -= 2)
        sum += *buf++;
    if (len == 1)
        sum += *(unsigned char *)buf;
    sum = (sum >> 16) + (sum & 0xFFFF);
    sum += (sum >> 16);
    result = ~sum;
    return result;
}

// display function to print the incoming packets
void display(void *buf, int bytes)
{
    struct iphdr *ip = buf;                                                             // get the IP header from the incoming packet
    struct icmphdr *icmp = buf + ip->ihl * 4;                                           // get the ICMP header from the incoming packet
    int header_length = 8;                                                              // ICMP header length is 8 bytes
    char sourceIPAddrReadable[32] = {'\0'};                                             // create a string to store the source IP address in a readable format
    inet_ntop(AF_INET, &ip->saddr, sourceIPAddrReadable, sizeof(sourceIPAddrReadable)); // convert the source IP address to a readable format
    if (new_ping_message)
    {
        new_ping_message = false;
        // print exactly like the example in the task
        printf("PING %s: %d data bytes\n", sourceIPAddrReadable, bytes - header_length);
    }
    // print exactly like the example in the task
    printf("%d bytes from %s: icmp_seq=%d ttl=%d time=%.03f ms\n", bytes, sourceIPAddrReadable, icmp->un.echo.sequence, ip->ttl, time_to_recv);
}

// command:     sudo make clean && sudo make all && sudo ./partb 8.8.8.8

int main(int argc, char *argv[])
{
    printf("hello parta\n");

    // Check if the user provided an argument for the hostname
    if (argc != 2)
    {
        printf("Usage: %s <hostname>\n", argv[0]);
        exit(1);
    }

    // -------------------- Setting the TCP connection to the watchdog --------------------
    struct sockaddr_in watchdog_addr;                          // create a socket address struct to store the destination address
    int watchdog_TCP_socket = socket(AF_INET, SOCK_STREAM, 0); // create a socket
    if (watchdog_TCP_socket < 0)
    {
        perror("better_ping: trouble creating socket (tcp)\n");
        exit(1);
    }

    //  Set up the watchdog address
    memset(&watchdog_addr, 0, sizeof(watchdog_addr));
    watchdog_addr.sin_family = AF_INET;
    watchdog_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    watchdog_addr.sin_port = htons(PORT);

    // Connect to the watchdog
    int watchdog_connect = connect(watchdog_TCP_socket, (struct sockaddr *)&watchdog_addr, sizeof(watchdog_addr));
    if (watchdog_connect < 0)
    {
        perror("better_ping: trouble connecting to the watchdog (tcp)\n");
        exit(1);
    }
    printf("better_ping: connected to the watchdog (tcp)\n");

    fcntl(watchdog_TCP_socket, F_SETFL, O_NONBLOCK); // Set the socket to non-blocking state
    // -------------------- End of set the TCP connection to the watchdog --------------------

    // -------------------- Setting the address to send the ping --------------------
    struct hostent *ping_address = gethostbyname(argv[1]); // Get the hostent structure by name
    if (ping_address == NULL)
    {
        perror("better_ping: problem get ping by name");
        exit(1);
    }

    struct sockaddr_in addr;                    // create a socket address struct to store the destination address
    bzero(&addr, sizeof(addr));                 // clear the buffer
    addr.sin_family = ping_address->h_addrtype; // set the address family
    addr.sin_addr.s_addr = inet_addr(argv[1]);  // set the IP address
    addr.sin_port = 0;                          // set the port to 0 since we are using ICMP
    // -------------------- End of set the address to send the  ping --------------------

    // -------------------- Create the raw socket --------------------
    const int val = 255; // set the TTL to 255
    // struct packet packet_ping;                                 // create a packet struct to send the ping message
    struct icmphdr icmp_header;                                // create an ICMP header struct to send the ping message
    struct sockaddr_in reply_addr;                             // create a socket address struct to store the reply address
    int ping_socket = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP); // create raw socket
    if (ping_socket < 0)
    {
        perror("better_ping: trouble open socket (raw - ping)\n");
        exit(1);
    }
    printf("better_ping: created raw socket\n");

    fcntl(ping_socket, F_SETFL, O_NONBLOCK); // Set the socket into non-blocking state

    int ttl_set = setsockopt(ping_socket, SOL_IP, IP_TTL, &val, sizeof(val)); // set the TTL to 255 for the socket
    if (ttl_set != 0)
    {
        perror("better_ping: trouble set socket TTL (raw - ping)\n");
        exit(1);
    }
    // -------------------- End of create the raw socket --------------------

    char watchdog_message[PACKET_SIZE] = {'\0'};
    char data_packet[BUFFER_SIZE] = " ping from roni and elor.\n";
    int data_length = strlen(data_packet) + 1;


    int number_of_loops = 4;
    while ((number_of_loops--) >= 0)
    {
        int watchdog_message_length;
        // -------------------- Send the ping --------------------
        socklen_t len_reply = sizeof(addr);
        memset(&icmp_header, 0, sizeof(icmp_header)); // clear the buffer
        icmp_header.type = ICMP_ECHO;                 // Set the type to ICMP_ECHO
        icmp_header.un.echo.id = process_id;          // Set the process id
        icmp_header.code = 0;
        static int sequence_number = 0;                                               // Set the sequence number to 0 and increment it by 1 for each ping message
        icmp_header.un.echo.sequence = sequence_number++;                             // Set the sequence number
        icmp_header.checksum = 0;                                                     // Set the checksum to 0 before calculating it

        char data_packet[BUFFER_SIZE];
        memset(data_packet, 0, sizeof(data_packet));
        memcpy((data_packet), &icmp_header, 8);
        memcpy((data_packet + 8), &data_packet, data_length);

        // Calculate the ICMP header checksum
        icmp_header.checksum = calculate_checksum((unsigned short *)(data_packet), data_length + 8);
        memcpy((data_packet), &icmp_header, 8);
        
        S_time = clock();  // start the timer

        // int sleep_time = icmp_header.un.echo.sequence;
        // sleep(sleep_time); // sleep for the sequence number of seconds

        sleep(1);

        // int if_send = sendto(ping_socket, &packet_ping, sizeof(packet_ping), 0, (struct sockaddr *)&addr, sizeof(addr)); // send the ping message
        int if_send = sendto(ping_socket, data_packet,data_length +8 , 0, (struct sockaddr *)&addr, sizeof(addr)); // send the ping message
        if (if_send < 0)
        {
            perror("Ping Error: problem send the ping\n");
            exit(1);
        }
        // -------------------- End of send the ping --------------------

        // -------------------- Receive the ping replay --------------------
        struct sockaddr_in addr_dest;            // create a socket address struct to store the destination address
        int len = sizeof(addr_dest);             // get the size of the address
                           // get the time before the recvfrom function
        bzero(data_packet, sizeof(data_packet)); // clear the buffer
        int bytes = 1;
        while (bytes > 0)
        {
            bytes = recvfrom(ping_socket, data_packet, sizeof(data_packet), 0, (struct sockaddr *)&addr_dest, &len); // receive the incoming packet
            if (bytes > 0)
            {
                time_to_recv = ((double)(clock() - S_time) / (CLOCKS_PER_SEC / 1000));               // calculate the time to receive the packet
                display(data_packet, bytes);                                       // call the display function to print the incoming packet
                // -------------------- Send an OK message to the watchdog --------------------
                bzero(watchdog_message, sizeof(watchdog_message));
                strcpy(watchdog_message, "getting a ping and let watchdog know that all OK");
                watchdog_message_length = send(watchdog_TCP_socket, watchdog_message, sizeof(watchdog_message), 0);
                // printf("%d\n", watchdog_message_length);
                if (watchdog_message_length < 0)
                {
                    perror("better_ping: problem send message to the watchdog\n");
                    exit(1);
                }
                // -------------------- End of send an OK message to the watchdog --------------------
            }
        }
        // -------------------- End of receive the ping replay --------------------

        // receive message from the watchdog if want to exit
        bzero(watchdog_message, sizeof(watchdog_message));
        watchdog_message_length = recv(watchdog_TCP_socket, watchdog_message, sizeof(watchdog_message), 0);
        if (watchdog_message_length > 0)
        {
            printf("better_ping: received message from the watchdog: %s\n", watchdog_message);
            if (strcmp(watchdog_message, "timeout") == 0)
            {
                printf("better_ping: received exit message from the watchdog\n");
                break;
            }
        }
    }
    close(watchdog_TCP_socket);
    close(ping_socket);

    return 0;
}