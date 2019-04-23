/* Send Multicast Datagram code example. */
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <iostream>
using namespace std;

//the package will br transport between client & server
typedef struct Package
{
    int num;
    char databuf[1024];
} Package;

struct in_addr localInterface;
struct sockaddr_in groupSock;
socklen_t groupSocklen = sizeof(groupSock);

Package package; //package to be transport

int sd;               //socket number
int datasize;         //the entire datasize of the file
int packagenum;       //the newest package ID I send
char sizebuffer[128]; //this array to receive file size
char namebuffer[128]; //this array to receive file name
char typebuffer[128]; //this array to send type

int main(int argc, char *argv[])
{
    char *ip = argv[1];       //Ip of local address (you can check your by typing "iconfig" on terminal)
    int port = atoi(argv[2]); //the port the socket connet with
    char *filename = argv[3]; //the file name (or msg)
    char *sendtype = argv[4];     //choose file or msg

    FILE *File; //read file

    /* Create a datagram socket on which to send. */
    sd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sd < 0)
    {
        perror("Opening datagram socket error");
        exit(1);
    }
    else
        printf("Opening the datagram socket...OK.\n");

    /* Initialize the group sockaddr structure with a */
    /* group address of 226.1.1.1 and port 4321. */
    memset((char *)&groupSock, 0, sizeof(groupSock));
    groupSock.sin_family = AF_INET;
    groupSock.sin_addr.s_addr = inet_addr("226.1.1.1");
    groupSock.sin_port = htons(port);

    /* Disable loopback so you do not receive your own datagrams.
	{
	char loopch = 0;
	if(setsockopt(sd, IPPROTO_IP, IP_MULTICAST_LOOP, (char *)&loopch, sizeof(loopch)) < 0)
	{
	perror("Setting IP_MULTICAST_LOOP error");
	close(sd);
	exit(1);
	}
	else
	printf("Disabling the loopback...OK.\n");
	}
	*/

    /* Set local interface for outbound multicast datagrams. */
    /* The IP address specified must be associated with a local, */
    /* multicast capable interface. */

    localInterface.s_addr = inet_addr(ip);
    if (setsockopt(sd, IPPROTO_IP, IP_MULTICAST_IF, (char *)&localInterface, sizeof(localInterface)) < 0)
    {
        perror("Setting local interface error");
        exit(1);
    }
    else
        printf("Setting the local interface...OK\n");

    //reset buffer
    memset(package.databuf, 0, 1024);
    memset(sizebuffer, 0, 128);
    memset(namebuffer, 0, 128);

    //open file which want to send
    if (strcmp("file", sendtype) == 0)
    {
        File = fopen(filename, "rb");

        //get datasize and save into a local integer
        fseek(File, 0, SEEK_END);
        datasize = ftell(File);
        fseek(File, 0, SEEK_SET);
    }
    else{
        datasize = strlen(filename);
    }

    //send type to receiver
    sprintf(typebuffer, "%s", sendtype);
    sendto(sd, typebuffer, sizeof(typebuffer), 0,
           (struct sockaddr *)&groupSock, sizeof(groupSock));

    //send file name to receiver
    if (strcmp("file", sendtype) == 0)
        sprintf(namebuffer, "%s", filename);
    else
        sprintf(namebuffer, "%s", "nothing.txt");
    sendto(sd, namebuffer, sizeof(namebuffer), 0,
           (struct sockaddr *)&groupSock, sizeof(groupSock));

    //send data size to receiver
    sprintf(sizebuffer, "%d", datasize);
    sendto(sd, sizebuffer, sizeof(sizebuffer), 0,
           (struct sockaddr *)&groupSock, sizeof(groupSock));

    sleep(1);    //take a rest

    /* Send a message to the multicast group specified by the*/
    /* groupSock sockaddr structure. */
    if (strcmp("file", sendtype) == 0)
        while (1) //keep sending file
        {
            packagenum = 0;                                   //reset package ID
            while (fread(package.databuf, 1, 1024, File) > 0) //not end of file
            {
                package.num = packagenum; //set package ID
                sendto(sd, &package, sizeof(package), 0, (struct sockaddr *)&groupSock, sizeof(groupSock));
                ++packagenum; //set next package ID
            }
            printf("Sending datagram message...OK\n");
            fseek(File, 0, SEEK_SET); //reset the fileread pointer to start of file
            sleep(0.5);
        }
    else        //send msg
    {
        packagenum = 0;     //only one package
        sprintf(package.databuf, "%s", filename);  //in sending msg, the array:filename is store the msg which will be send
        while (1)
        {
            sendto(sd, &package, sizeof(package), 0, (struct sockaddr *)&groupSock, sizeof(groupSock));
            printf("Sending datagram message...OK\n %s\n", package.databuf);
            sleep(1);
        }
    }

    /* Try the re-read from the socket if the loopback is not disable
	if(read(sd, databuf, datalen) < 0)
	{
	perror("Reading datagram message error\n");
	close(sd);
	exit(1);
	}
	else
	{
	printf("Reading datagram message from client...OK\n");
	printf("The message is: %s\n", databuf);
	}
	*/
    fclose(File);
    return 0;
}