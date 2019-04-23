/* Receiver/client multicast Datagram example. */
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <unistd.h>
#include <memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <iostream>
using namespace std;

//the package will br transport between client & server
typedef struct Package
{
    int num;            //the ID of package
    char databuf[1024]; //the data this package contain
} Package;

struct sockaddr_in localSock;
struct ip_mreq group;

Package package; //package to be transport

int sd;                      //socket number
int datasize;                //the entire datasize of the file
int nowrecv_datasize = 0;    //the received datasize of the file
int all_package_num = 0;     //the all package number of the file
int packagenum = 0;          //the newest package ID I receive
int past_package_num = -1;   //the earlier package ID I recieve
int receive_pageage_num = 0; //the package number i receive
bool send_file_flag = true;  //the flag to indicate the server send one turn of file

char namebuffer[128]; //this array to receive file name
char sizebuffer[128]; //this array to receive file size
char filename[128];   //this array to save file name without directory name
char typebuffer[128]; //this array to receive type
char outputbuffer[1024] = "";

int main(int argc, char *argv[])
{
    char *ip = argv[1];       //Ip of local address (you can check your by typing "iconfig" on terminal)
    int port = atoi(argv[2]); //the port the socket connet with

    FILE *to; //file I/O

    /* Create a datagram socket on which to receive. */
    sd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sd < 0)
    {
        perror("Opening datagram socket error");
        exit(1);
    }
    else
        printf("Opening datagram socket....OK.\n");

    /* Enable SO_REUSEADDR to allow multiple instances of this */
    /* application to receive copies of the multicast datagrams. */
    {
        int reuse = 1;
        if (setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, (char *)&reuse, sizeof(reuse)) < 0)
        {
            perror("Setting SO_REUSEADDR error");
            close(sd);
            exit(1);
        }
        else
            printf("Setting SO_REUSEADDR...OK.\n");
    }

    /* Bind to the proper port number with the IP address */
    /* specified as INADDR_ANY. */
    memset((char *)&localSock, 0, sizeof(localSock));
    localSock.sin_family = AF_INET;
    localSock.sin_port = htons(port);
    localSock.sin_addr.s_addr = INADDR_ANY;
    if (bind(sd, (struct sockaddr *)&localSock, sizeof(localSock)))
    {
        perror("Binding datagram socket error");
        close(sd);
        exit(1);
    }
    else
        printf("Binding datagram socket...OK.\n");

    /* Join the multicast group 226.1.1.1 on the local interface */
    /* interface. Note that this IP_ADD_MEMBERSHIP option must be */
    /* called for each local interface over which the multicast */
    /* datagrams are to be received. */
    group.imr_multiaddr.s_addr = inet_addr("226.1.1.1");
    group.imr_interface.s_addr = inet_addr(ip);
    if (setsockopt(sd, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char *)&group, sizeof(group)) < 0)
    {
        perror("Adding multicast group error");
        close(sd);
        exit(1);
    }
    else
        printf("Adding multicast group...OK.\n");

    //initial buffer
    memset(package.databuf, 0, 1024);
    memset(sizebuffer, 0, 128);
    memset(namebuffer, 0, 128);
    memset(filename, 0, 128);
    memset(typebuffer, 0, 128);

    //receive type
    read(sd, typebuffer, 128);
    cout << "receive file name: " << typebuffer << endl;

    //receive file name
    read(sd, namebuffer, 128);
    cout << "receive file name: " << namebuffer << endl;

    //receive data size
    read(sd, sizebuffer, 128);
    datasize = atoi(sizebuffer);
    cout << "receive data size:" << datasize << endl;

    //create the output directory
    mkdir("output", 0777);

    if (strcmp("file", typebuffer) == 0) //receive file
    {
        //create the new file
        sprintf(filename, "output/%s", strrchr(namebuffer, '/') == nullptr ? namebuffer : strrchr(namebuffer, '/') + 1);
        //int to = creat(filename, 0777);
        to = fopen(filename, "wb+");
        if (to < 0)
        {
            cout << "Error creating destination file\n";
        }

        all_package_num = ceil((float)datasize / (float)1024);

        /* Read from the socket. */
        while (nowrecv_datasize < datasize)
        {
            read(sd, &package, sizeof(package));    //receive

            if (package.num > past_package_num && send_file_flag)
                ++receive_pageage_num;
            else
                send_file_flag = false;
            past_package_num = package.num;

            if (packagenum - package.num == 0) //it means package isn't drop
            {
                //write(to, package.databuf, sizeof(package.databuf));
                if (strcmp(strrchr(namebuffer, '.') + 1, "txt") != 0)
                    fwrite(package.databuf, 1, sizeof(package.databuf), to);
                else
                    fwrite(package.databuf, 1, strlen(package.databuf), to);
                nowrecv_datasize += 1024; //update the datasize I receive
                ++packagenum;             //update the newest package num I recieve
            }
        }
        fclose(to);
    }
    else //receive msg
    {
        read(sd, &package, sizeof(package));
        sprintf(outputbuffer, "%s", package.databuf);
        printf("ths msg is:%s\n", outputbuffer);
    }

    close(sd);
    printf("Reading datagram message...OK.\n");
    printf("all_package_num is: %d\n", all_package_num);
    printf("receive_pageage_num is: %d\n", receive_pageage_num);
    printf("the miss rate is: %f\n", (float)(all_package_num - receive_pageage_num) / (float)all_package_num);

    return 0;
}
