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
    int num = -1;
    char databuf[1024];
} Package;

//redunt part of a fec package
typedef struct Redunt_part
{
    int num = -1;
    int part = -1;
    char databuf[256] = "";
} Redunt_part;
//fec package

typedef struct Fec_package
{
    int fec_set = -1;
    int fec_check = -1;
    Redunt_part redunt_Part[4];
    Package package;
} Fec_package;

struct in_addr localInterface;
struct sockaddr_in groupSock;
socklen_t groupSocklen = sizeof(groupSock);

Package package; //package to be transport

int sd;               //socket number
int datasize;         //the entire datasize of the file
int packagenum;       //the newest package ID I send
char sizebuffer[128]; //this array to receive file size
char namebuffer[128]; //this array to receive file name
char modebuffer[128]; //this array to send mode

int main(int argc, char *argv[])
{
    char *ip = argv[1];       //Ip of local address (you can check your by typing "iconfig" on terminal)
    int port = atoi(argv[2]); //the port the socket connet with
    char *filename = argv[3]; //the file name (or msg)
    char *sendmode = argv[4]; //choose mode: normal, fec, slow
                              //fec : it will send redundent package to sender, it will add package number
                              //slow : server will send package more slowly, it witt reduce package miss rate

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
    memset(modebuffer, 0, 128);

    //open file which want to send
    File = fopen(filename, "rb");

    //get datasize and save into a local integer
    fseek(File, 0, SEEK_END);
    datasize = ftell(File);
    fseek(File, 0, SEEK_SET);

    //send file name to receiver
    sprintf(namebuffer, "%s", filename);
    sendto(sd, namebuffer, sizeof(namebuffer), 0,
           (struct sockaddr *)&groupSock, sizeof(groupSock));

    //send sendmode to receiver
    sprintf(modebuffer, "%s", sendmode);
    sendto(sd, modebuffer, sizeof(modebuffer), 0,
           (struct sockaddr *)&groupSock, sizeof(groupSock));

    //send data size to receiver
    sprintf(sizebuffer, "%d", datasize);
    sendto(sd, sizebuffer, sizeof(sizebuffer), 0,
           (struct sockaddr *)&groupSock, sizeof(groupSock));

    sleep(1); //take a rest

    /* Send a message to the multicast group specified by the*/
    /* groupSock sockaddr structure. */
    if (strcmp(sendmode, "fec") != 0)
    {
        while (1) //keep sending file
        {
            packagenum = 0;                                   //reset package ID
            while (fread(package.databuf, 1, 1024, File) > 0) //not end of file
            {
                package.num = packagenum; //set package ID
                if (strcmp(sendmode, "normal") == 0)
                    sendto(sd, &package, sizeof(package), 0, (struct sockaddr *)&groupSock, sizeof(groupSock));
                else if (strcmp(sendmode, "multi") == 0)
                {
                    for (int i = 0; i < 3; i++)
                        sendto(sd, &package, sizeof(package), 0, (struct sockaddr *)&groupSock, sizeof(groupSock));
                }
                else if (strcmp(sendmode, "slow") == 0)
                {
                    sendto(sd, &package, sizeof(package), 0, (struct sockaddr *)&groupSock, sizeof(groupSock));
                    sleep(0.001);
                }
                ++packagenum; //set next package ID
            }
            printf("Sending datagram message...OK\n");
            fseek(File, 0, SEEK_SET); //reset the fileread pointer to start of file
            sleep(0.6);
        }
    }
    else
    {
        //fec mode
        int fec_set_num = 0;   //which set is this package with? (one set have five packages)
        bool eof_flag = false; //a flag to indicate end of file

        while (1) //keep sending file
        {
            packagenum = 0;   //reset package ID
            fec_set_num = 0;  //reset set number
            eof_flag = false; //reset flag

            while (1) //not end of file
            {
                if (eof_flag) //break at end of file
                    break;

                Fec_package fec_package[5]; //decline 5 fec packages

                //move the file date to a set(five) of fec package
                if (fread(fec_package[0].package.databuf, 1, 1024, File) <= 0 && !eof_flag)
                    eof_flag = true; //if end of file, change the flag
                else
                {
                    fec_package[0].package.num = packagenum;
                    strncpy(fec_package[1].redunt_Part[0].databuf, fec_package[0].package.databuf, 256);
                    fec_package[1].redunt_Part[0].num = packagenum;
                    fec_package[1].redunt_Part[0].part = 0;
                    strncpy(fec_package[2].redunt_Part[0].databuf, fec_package[0].package.databuf + 256, 256);
                    fec_package[2].redunt_Part[0].num = packagenum;
                    fec_package[2].redunt_Part[0].part = 1;
                    strncpy(fec_package[3].redunt_Part[0].databuf, fec_package[0].package.databuf + 512, 256);
                    fec_package[3].redunt_Part[0].num = packagenum;
                    fec_package[3].redunt_Part[0].part = 2;
                    strncpy(fec_package[4].redunt_Part[0].databuf, fec_package[0].package.databuf + 768, 256);
                    fec_package[4].redunt_Part[0].num = packagenum;
                    fec_package[4].redunt_Part[0].part = 3;
                    ++packagenum;
                }
                if (fread(fec_package[1].package.databuf, 1, 1024, File) <= 0 && !eof_flag)
                    eof_flag = true;
                else
                {
                    fec_package[1].package.num = packagenum;
                    strncpy(fec_package[0].redunt_Part[0].databuf, fec_package[1].package.databuf, 256);
                    fec_package[0].redunt_Part[0].num = packagenum;
                    fec_package[0].redunt_Part[0].part = 0;
                    strncpy(fec_package[2].redunt_Part[1].databuf, fec_package[1].package.databuf + 256, 256);
                    fec_package[2].redunt_Part[1].num = packagenum;
                    fec_package[2].redunt_Part[1].part = 1;
                    strncpy(fec_package[3].redunt_Part[1].databuf, fec_package[1].package.databuf + 512, 256);
                    fec_package[3].redunt_Part[1].num = packagenum;
                    fec_package[3].redunt_Part[1].part = 2;
                    strncpy(fec_package[4].redunt_Part[1].databuf, fec_package[1].package.databuf + 768, 256);
                    fec_package[4].redunt_Part[1].num = packagenum;
                    fec_package[4].redunt_Part[1].part = 3;
                    ++packagenum;
                }
                if (fread(fec_package[2].package.databuf, 1, 1024, File) <= 0 && !eof_flag)
                    eof_flag = true;
                else
                {
                    fec_package[2].package.num = packagenum;
                    strncpy(fec_package[0].redunt_Part[1].databuf, fec_package[2].package.databuf, 256);
                    fec_package[0].redunt_Part[1].num = packagenum;
                    fec_package[0].redunt_Part[1].part = 0;
                    strncpy(fec_package[1].redunt_Part[1].databuf, fec_package[2].package.databuf + 256, 256);
                    fec_package[1].redunt_Part[1].num = packagenum;
                    fec_package[1].redunt_Part[1].part = 1;
                    strncpy(fec_package[3].redunt_Part[2].databuf, fec_package[2].package.databuf + 512, 256);
                    fec_package[3].redunt_Part[2].num = packagenum;
                    fec_package[3].redunt_Part[2].part = 2;
                    strncpy(fec_package[4].redunt_Part[2].databuf, fec_package[2].package.databuf + 768, 256);
                    fec_package[4].redunt_Part[2].num = packagenum;
                    fec_package[4].redunt_Part[2].part = 3;
                    ++packagenum;
                }
                if (fread(fec_package[3].package.databuf, 1, 1024, File) <= 0 && !eof_flag)
                    eof_flag = true;
                else
                {
                    fec_package[3].package.num = packagenum;
                    strncpy(fec_package[0].redunt_Part[2].databuf, fec_package[3].package.databuf, 256);
                    fec_package[0].redunt_Part[2].num = packagenum;
                    fec_package[0].redunt_Part[2].part = 0;
                    strncpy(fec_package[1].redunt_Part[2].databuf, fec_package[3].package.databuf + 256, 256);
                    fec_package[1].redunt_Part[2].num = packagenum;
                    fec_package[1].redunt_Part[2].part = 1;
                    strncpy(fec_package[2].redunt_Part[2].databuf, fec_package[3].package.databuf + 512, 256);
                    fec_package[2].redunt_Part[2].num = packagenum;
                    fec_package[2].redunt_Part[2].part = 2;
                    strncpy(fec_package[4].redunt_Part[3].databuf, fec_package[3].package.databuf + 768, 256);
                    fec_package[4].redunt_Part[3].num = packagenum;
                    fec_package[4].redunt_Part[3].part = 3;
                    ++packagenum;
                }
                if (fread(fec_package[4].package.databuf, 1, 1024, File) <= 0 && !eof_flag)
                    eof_flag = true;
                else
                {
                    fec_package[4].package.num = packagenum;
                    strncpy(fec_package[0].redunt_Part[3].databuf, fec_package[4].package.databuf, 256);
                    fec_package[0].redunt_Part[3].num = packagenum;
                    fec_package[0].redunt_Part[3].part = 0;
                    strncpy(fec_package[1].redunt_Part[3].databuf, fec_package[4].package.databuf + 256, 256);
                    fec_package[1].redunt_Part[3].num = packagenum;
                    fec_package[1].redunt_Part[3].part = 1;
                    strncpy(fec_package[2].redunt_Part[3].databuf, fec_package[4].package.databuf + 512, 256);
                    fec_package[2].redunt_Part[3].num = packagenum;
                    fec_package[2].redunt_Part[3].part = 2;
                    strncpy(fec_package[3].redunt_Part[3].databuf, fec_package[4].package.databuf + 768, 256);
                    fec_package[3].redunt_Part[3].num = packagenum;
                    fec_package[3].redunt_Part[3].part = 3;
                    ++packagenum;
                }

                //mark the check number and set_number
                for (int i = 0; i < 5; i++)
                {
                    fec_package[i].fec_check = i;
                    fec_package[i].fec_set = fec_set_num;
                }
                //send five package
                sendto(sd, &fec_package[0], sizeof(Fec_package), 0, (struct sockaddr *)&groupSock, sizeof(groupSock));
                sendto(sd, &fec_package[1], sizeof(Fec_package), 0, (struct sockaddr *)&groupSock, sizeof(groupSock));
                sendto(sd, &fec_package[2], sizeof(Fec_package), 0, (struct sockaddr *)&groupSock, sizeof(groupSock));
                sendto(sd, &fec_package[3], sizeof(Fec_package), 0, (struct sockaddr *)&groupSock, sizeof(groupSock));
                sendto(sd, &fec_package[4], sizeof(Fec_package), 0, (struct sockaddr *)&groupSock, sizeof(groupSock));
                ++fec_set_num; //add set_number
            }
            printf("Sending datagram message...OK\n");
            fseek(File, 0, SEEK_SET); //reset the fileread pointer to start of file
            sleep(0.6);
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
