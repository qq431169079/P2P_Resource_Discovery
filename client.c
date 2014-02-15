/********************************************************************************************************************************************************************
 * client.c
 * Author: Rohith Narasimhamurthy
 * Date: 23 Nov 2013
 * Creates 2 clients and sends Discover requests to Directory server and gets back the contact details of File Servers. 
 * Requests files from FileServers over TCP 
 * *******************************************************************************************************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/wait.h>

#define DIRSERVERPORT 31498 /*Port number of the Directory server to send Discover request*/
#define CLIENT1_PORT 32498  /*static UDP port on which Client1 sends discover request*/
#define CLIENT2_PORT 33498  /*static UDP port on which Client2 sends discover request*/

#define BUFLEN 50

/*  * get_local_info() will set the ip port details of the host
  * And set TCP or UDP connection based on last arg 1 or 0 resp
  * int port: sets the port field in getaddrinfo()
  * int ptype: 1 sets up a TCP stream and 0 UDP datagram
  * return: Sets the local_srv_infoPtr pointer
  * * */
int get_local_info(struct addrinfo * reqmtPtr,struct addrinfo** local_srv_infoPtr, int port, int ptype)
{
 int errVal = -1;
 char buf[BUFLEN];
 /*Set requirements to get local address info*/
 memset(reqmtPtr, 0, sizeof(*reqmtPtr));
 reqmtPtr->ai_family = AF_INET; /*IPv4*/	
 if(ptype == 0)
 	reqmtPtr->ai_socktype = SOCK_DGRAM;/*UDP connection*/
 else
	reqmtPtr->ai_socktype = SOCK_STREAM;/*TCP connection*/
 
 reqmtPtr->ai_flags = AI_PASSIVE; /* use local IP*/
 
 struct addrinfo * temp = *local_srv_infoPtr;
  /*Get local info in local_srv_info str*/
 if(port)
 {
	 sprintf(buf,"%d",htons(port));
 errVal = getaddrinfo("nunki.usc.edu", buf, reqmtPtr, &temp);
 }

 *local_srv_infoPtr = temp;
 if (errVal != 0) 
 {
 	fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(errVal));
 }
 return errVal;
}


int main(int argc, char *argv[])
{
 int sockfd=-1;
 struct addrinfo reqmt;
 struct addrinfo *local_srv_info = NULL, *temp = NULL;
 int errVal=-1;
 int numbytes;
 struct addrinfo *local_cli_info_UDP = NULL, *local_srv_info_TCP, *temp_UDP = NULL, *temp_TCP = NULL;
 char msg[BUFLEN];
memset(&msg,0,sizeof(msg));
struct hostent *host;
struct sockaddr_in sin;
socklen_t len;

/***************************************** Fork to create client2 ****************************************************************************************************/
int client1 = fork();

if(client1)
{
  errVal = get_local_info(&reqmt,&local_srv_info,CLIENT1_PORT,0);
  /*Set the msg which Client1 has to send to DS*/
  strcpy(msg,"Client1 doc1");
}
else
{
 memset(&reqmt,0,sizeof(reqmt));
 errVal = get_local_info(&reqmt,&local_srv_info,CLIENT2_PORT,0);
 /*Set the msg which Client2 has to send to DS*/
 strcpy(msg,"Client2 doc2");

}
temp = local_srv_info;
/***************************************************************** UDP client setup *********************************************************************************/
 while(temp != NULL)
 {
	 sockfd = socket(temp->ai_family, temp->ai_socktype,temp->ai_protocol); 
	 if(sockfd < 0)
	 {
 		temp =	temp->ai_next;	
 	 }
	 else
	 {
 		break;
	 }
 }	 
 if (temp == NULL) 
 {
 	fprintf(stderr, "Client: failed to create socket\n");
 	return 2;
 }


 errVal = bind(sockfd, temp->ai_addr, temp->ai_addrlen);
 if(errVal < 0)
  {
	close(sockfd);
	perror("Client: bind");
	return 1;
  }
struct sockaddr_in* pa = (struct sockaddr_in*)(temp->ai_addr);
char tmp[10];
sscanf(msg,"%s",tmp);
host=gethostbyname("nunki.usc.edu");
if(host==NULL)
{
 perror("Error in gethostbyname(): ");
 exit(1);
}
len = sizeof(sin);
if (getsockname(sockfd, (struct sockaddr *)&sin, &len) < 0)
    perror("getsockname");
printf("Phase 2: %s has UDP port number %d and IP address %s\n",tmp,/*pa->sin_port*/ntohs(sin.sin_port),/*inet_ntoa(pa->sin_addr)*/inet_ntoa(*((struct in_addr *)host->h_addr)));
 
freeaddrinfo(local_srv_info);
/***************************************************************** UDP Client setup complete ************************************************************************/
/***************************************************************** Discover Phase UDP Client ************************************************************************/
 
/*Setup destination info*/
struct addrinfo  dest, *destptr;
 memset(&dest, 0, sizeof(dest));
 dest.ai_family = AF_INET; /*IPv4*/	
 dest.ai_socktype = SOCK_DGRAM;/*UDP connection*/ 
 dest.ai_flags = AI_PASSIVE; /* use local IP*/
 char buf[10],recv_buf[BUFLEN];
 memset(&recv_buf,0,sizeof(recv_buf));
 /*htons of DirSrvPort*/
 sprintf(buf,"%d",htons(DIRSERVERPORT));
 /*Get local info in local_srv_info str*/
  errVal = getaddrinfo("nunki.usc.edu", buf, &dest, &destptr);

/*Sending Discover request to dir serv*/
if ((numbytes = sendto(sockfd, msg, strlen(msg)+1, 0,destptr->ai_addr, destptr->ai_addrlen)) < 0) 
{
 perror("Client Discover request: sendto");
 exit(1);
 }
printf("Phase 2: The File request from %s has been sent to the Directory Server\n",tmp);
  freeaddrinfo(destptr);
 
 /*Waiting for Discover Response from directory server*/
  if((numbytes = recvfrom(sockfd, recv_buf, BUFLEN-1 , 0, destptr->ai_addr, &destptr->ai_addrlen)) == -1)
{
 perror("Client Discover response: recvfrom");
 exit(1);

}
 close(sockfd);


 /************************************************************* TCP client setup start *****************************************************************************/
 int tcp_sockfd;
 int reuse=1;
 char fsName[BUFLEN];
 int fs_port;
 /*Extract TCP port from Discover response*/
 sscanf(recv_buf,"%s %d",fsName,&fs_port);
 printf("Phase 2: The File requested by %s is present in %s and the File Server’s TCP port number is %d\n",tmp,fsName,fs_port);

 /*htons for commn with fs*/
 get_local_info(&reqmt,&local_srv_info_TCP,fs_port,1);


temp_TCP = local_srv_info_TCP;
     
    while(temp_TCP !=NULL)
{
	tcp_sockfd =socket(temp_TCP->ai_family, temp_TCP->ai_socktype,temp_TCP->ai_protocol);
	if(errVal <0)
	{
		temp_TCP = temp_TCP->ai_next;
	}
	else
	{
		break;
	}
}
    if (temp_TCP == NULL)  {
        fprintf(stderr, "client: failed to create TCP socket\n");
        return 2;
    }

 if (setsockopt(tcp_sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse,sizeof(int)) == -1)
{
         perror("FileServer TCP socket setsockopt");
         return 1;
}
errVal = connect(tcp_sockfd, temp_TCP->ai_addr, temp_TCP->ai_addrlen);
if(errVal < 0)
{
 printf("Client: Failed to connect\n");
 return 2;
}

printf("Phase 2: End of Phase 2 for %s\n",tmp);
len = sizeof(sin);
if (getsockname(tcp_sockfd, (struct sockaddr *)&sin, &len) < 0)
    perror("getsockname");

pa = (struct sockaddr_in*)(temp_TCP->ai_addr);
host=gethostbyname("nunki.usc.edu");
if(host==NULL)
{
 perror("Error in gethostbyname(): ");
 exit(1);
}
printf("Phase 3: %s has dynamic TCP port number %d and IP address %s\n",tmp,ntohs(sin.sin_port),/*inet_ntoa(pa->sin_addr)*/inet_ntoa(*((struct in_addr *)host->h_addr)));
    freeaddrinfo(local_srv_info_TCP); // all done with this structure

/**********************************************************TCP setup complete*****************************************************************************************/
/**********************************************************TCP client start******************************************************************************************/  
/*Send the file request to File Server*/
if (send(tcp_sockfd, msg, strlen(msg)+1, 0) == -1)
                perror("send");
printf("Phase 3: The File request from %s has been sent to the %s\n",tmp,fsName);
   memset(&msg,0,sizeof(memset));

   /*Waiting to receive the response from FileServer*/
    if ((numbytes = recv(tcp_sockfd, msg, BUFLEN-1, 0)) == -1)
	{
 	perror("recv");
 	exit(1);
 	}
    printf("Phase 3: %s received %s from %s\n",tmp,msg,fsName);

 	buf[numbytes] = '\0';
 	close(sockfd);
	if(!client1)
	{
	printf("Phase 3: End of Phase 3 for %s\n",tmp);
	      	exit(1);/*Exit child client*/
	}
 	waitpid(client1,NULL,0); /*wait for child client*/
	printf("Phase 3: End of Phase 3 for %s\n",tmp);
	exit(0);
}
