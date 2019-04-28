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
#include <vector>
using namespace std;

//the package will br transport between client & server
typedef struct Package
{
    int num;            //the ID of package
    char databuf[1024]; //the data this package contain
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

struct sockaddr_in localSock;
struct ip_mreq group;

Package package; //package to be transport

int sd;                            //socket number
int datasize;                      //the entire datasize of the file
int nowrecv_datasize = 0;          //the received datasize of the file
int all_package_num = 0;           //the all package number of the file
float packagenum = 0;              //the newest package ID I receive
int past_package_num = -1;         //the earlier package ID I recieve
int receive_pageage_num = 0;       //the package number i receive
int first_broadcast_data_size = 0; //the first_broadcast_data_size
float file_integrity = 0;          //file integrity of first broadcast
bool send_file_flag = true;        //the flag to indicate the server send one turn of file

char namebuffer[128]; //this array to receive file name
char sizebuffer[128]; //this array to receive file size
char filename[128];   //this array to save file name without directory name
char modebuffer[128]; //this array to save sendmode
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
    memset(modebuffer, 0, 128);

    //receive file name
    read(sd, namebuffer, 128);
    cout << "receive file name: " << namebuffer << endl;

    //receive send mode
    read(sd, modebuffer, 128);
    cout << "receive send mode: " << modebuffer << endl;

    //receive data size
    read(sd, sizebuffer, 128);
    datasize = atoi(sizebuffer);
    cout << "receive data size:" << datasize << endl;

    //create the output directory
    mkdir("output", 0777);

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
    if (strcmp(modebuffer, "fec") != 0)
    {
        while (nowrecv_datasize < datasize)
        {
            read(sd, &package, sizeof(package)); //receive

            if (package.num >= past_package_num && send_file_flag)
            {
                ++receive_pageage_num;
                if (package.num > past_package_num)
                    first_broadcast_data_size += 1024;
            }
            else
            {
                send_file_flag = false;
            }
            past_package_num = package.num;

            if (packagenum == package.num) //it means package isn't drop
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
    }
    else //fec part
    {
        vector<Fec_package> receive_vector; //a vector to receive package
        int fec_set_package_count = 0;      //how many fec package with same set_number I received this time
        int fec_set = 0;                    //the fec set_number this time
        while (nowrecv_datasize < datasize)
        {
            fec_set_package_count = 0; //reset fec_set_package_count

            while (receive_vector.size() < 5) //read package until 5 package in the vector
            {
                Fec_package fec_package;
                read(sd, &fec_package, sizeof(Fec_package));
                receive_vector.push_back(fec_package);

                if (fec_package.package.num >= past_package_num && send_file_flag)
                {
                    ++receive_pageage_num;
                    if (fec_package.package.num > past_package_num)
                        first_broadcast_data_size += 1024;
                }
                else
                {
                    send_file_flag = false;
                }
                past_package_num = package.num;
            }

            fec_set = receive_vector[0].fec_set; //get set_number

            for (int i = 0; i < 5; i++) //get fec_set_package_count
            {
                if (fec_set == receive_vector[i].fec_set)
                    ++fec_set_package_count;
            }

            // means all of this set's package have received
            if (fec_set_package_count == 5)
            {
                for (int i = 0; i < 5; i++)
                {
                    if (packagenum == receive_vector[i].package.num) //it means package isn't drop
                    {
                        //write(to, package.databuf, sizeof(package.databuf));
                        if (strcmp(strrchr(namebuffer, '.') + 1, "txt") != 0)
                            fwrite(receive_vector[i].package.databuf, 1, sizeof(receive_vector[i].package.databuf), to);
                        else
                            fwrite(receive_vector[i].package.databuf, 1, strlen(receive_vector[i].package.databuf), to);
                        nowrecv_datasize += 1024; //update the datasize I receive
                        ++packagenum;             //update the newest package num I recieve
                    }
                }
                if (nowrecv_datasize >= datasize)
                    break;
            } //fec_set_package_count = 4,means one package missed, and it can be construct by other four package
            else if (fec_set_package_count == 4)
            {
                int check = 10;
                int miss_package_num = -1;
                for (int i = 0; i < 4; i++)
                {
                    check -= receive_vector[i].fec_check;
                }
                if (check == 0)
                    miss_package_num = 0;
                else if (check == 1)
                    miss_package_num = 1;
                else if (check == 2)
                    miss_package_num = 2;
                else if (check == 3)
                    miss_package_num = 3;
                else if (check == 4)
                    miss_package_num = 4;
                else
                {
                    printf("error at miss package num check\n");
                    exit(1);
                }
                for (int i = 0; i < miss_package_num; i++)
                {
                    if (packagenum == receive_vector[i].package.num) //it means package isn't drop
                    {
                        //write(to, package.databuf, sizeof(package.databuf));
                        if (strcmp(strrchr(namebuffer, '.') + 1, "txt") != 0)
                            fwrite(receive_vector[i].package.databuf, 1, sizeof(receive_vector[i].package.databuf), to);
                        else
                            fwrite(receive_vector[i].package.databuf, 1, strlen(receive_vector[i].package.databuf), to);
                        nowrecv_datasize += 1024; //update the datasize I receive
                        ++packagenum;             //update the newest package num I recieve
                    }
                }
                if (nowrecv_datasize >= datasize)
                    break;
                int part_check = 0;
                for (int i = 0; i < 4; i++)
                {
                    for (int j = 0; i < 4; i++)
                    {
                        if (receive_vector[i].redunt_Part[j].part == part_check)
                        {
                            ++part_check;
                            if (packagenum == receive_vector[i].redunt_Part[j].num) //it means package isn't drop
                            {
                                //write(to, package.databuf, sizeof(package.databuf));
                                if (strcmp(strrchr(namebuffer, '.') + 1, "txt") != 0)
                                    fwrite(receive_vector[i].redunt_Part[j].databuf, 1, sizeof(receive_vector[i].redunt_Part[j].databuf), to);
                                else
                                    fwrite(receive_vector[i].redunt_Part[j].databuf, 1, strlen(receive_vector[i].redunt_Part[j].databuf), to);
                                nowrecv_datasize += 256; //update the datasize I receive
                                packagenum += 0.25;      //update the newest package num I recieve
                            }
                        }
                    }
                }

                if (nowrecv_datasize >= datasize)
                    break;
                for (int i = miss_package_num; i < 4; i++)
                {
                    if (packagenum == receive_vector[i].package.num) //it means package isn't drop
                    {
                        //write(to, package.databuf, sizeof(package.databuf));
                        if (strcmp(strrchr(namebuffer, '.') + 1, "txt") != 0)
                            fwrite(receive_vector[i].package.databuf, 1, sizeof(receive_vector[i].package.databuf), to);
                        else
                            fwrite(receive_vector[i].package.databuf, 1, strlen(receive_vector[i].package.databuf), to);
                        nowrecv_datasize += 1024; //update the datasize I receive
                        ++packagenum;             //update the newest package num I recieve
                    }
                }
                if (nowrecv_datasize >= datasize)
                    break;
            }
            else
            {
                for (int i = 0; i < fec_set_package_count; i++)
                {
                    if (packagenum == receive_vector[i].package.num) //it means package isn't drop
                    {
                        //write(to, package.databuf, sizeof(package.databuf));
                        if (strcmp(strrchr(namebuffer, '.') + 1, "txt") != 0)
                            fwrite(receive_vector[i].package.databuf, 1, sizeof(receive_vector[i].package.databuf), to);
                        else
                            fwrite(receive_vector[i].package.databuf, 1, strlen(receive_vector[i].package.databuf), to);
                        nowrecv_datasize += 1024; //update the datasize I receive
                        ++packagenum;             //update the newest package num I recieve
                    }
                }
            }

            for (int i = 0; i < 5; i++)
            {
                if (receive_vector[0].fec_set == fec_set)
                    receive_vector.erase(receive_vector.begin());
            }
        }
    }
    fclose(to);

    if (first_broadcast_data_size < datasize)
        file_integrity = (float)first_broadcast_data_size / (float)datasize;
    else
        file_integrity = 1;

    close(sd);
    printf("Reading datagram message...OK.\n");
    /*
    if (strcmp(modebuffer, "multi") == 0)
        printf("all_package_num is: %d\n", all_package_num * 3);
    else
        printf("all_package_num is: %d\n", all_package_num);
    printf("receive_pageage_num is: %d\n", receive_pageage_num);
    */
    if (strcmp(modebuffer, "multi") == 0)
        printf("the package miss rate of first broadcast is: %f\n", (float)((all_package_num * 3) - receive_pageage_num) / (float)(all_package_num * 3));
    else
        printf("the package miss rate of first broadcast is: %f\n", (float)(all_package_num - receive_pageage_num) / (float)all_package_num);
    printf("file integrity of first broadcast is: %f\n", file_integrity);

    return 0;
}
