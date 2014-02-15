/**********************************************************************************************************************************************************************
 *file_server.c
 *Author: Rohith Narasimhamurthy
 *Date: 23 Nov 2013
 *Creates 3 File Servers which register with Directory Server(UDP) and service clients request for documuments(TCP)
 *********************************************************************************************************************************************************************/
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

#define DIRSERVERPORT 21498 /*Directory server port*/
#define BUFLEN 50
#define QUEUE 10 	    /*Queue Length of TCP server */
#define FS1_UDP 22498	    /*FileServer UDP port number for Phase 1 Registration*/
#define FS1_TCP 41498	    /*FileServer TCP port number for Phase 3 File Transfer */
#define FS2_UDP 23498
#define FS2_TCP 42498
#define FS3_UDP 24498
#define FS3_TCP 43498

int mainProc =1; 	     /*Identifies Parent process which is also FileServer1*/


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
 /*htons convertion of local/serv ports*/
 memset(&buf,0,sizeof(buf));
 sprintf(buf,"%d",htons(port));
 //printf("Binding to port:%d %s\n", port, buf);
 struct addrinfo * temp = *local_srv_infoPtr;
  /*Get local info in local_srv_info str*/
 errVal = getaddrinfo("nunki.usc.edu", buf, reqmtPtr, &temp);
 *local_srv_infoPtr = temp;
 if (errVal != 0) 
 {
	printf("getaddrinfo: %s\n", gai_strerror(errVal));
 }
 return errVal;
}

int main(void)
{
 /******************************************** Main process File_Server1 ************************************************************************************/
 int sockfd = -1,new_fd;
 struct addrinfo reqmt;
 struct addrinfo *local_cli_info_UDP = NULL, *local_srv_info_TCP, *temp_UDP = NULL, *temp_TCP = NULL;
 int errVal =-1;
 int recv_bytes = -1;
 struct sockaddr client_addr;
 char send_buf[BUFLEN];
 socklen_t addr_len;
 char msgBuf[BUFLEN];
 int fs1;
 struct hostent *host;
 struct sockaddr_in sin;
 socklen_t len; 
 /******************************************** Forking File_Server2 ************************************************************************************/
 mainProc = fork(); //FS2 created

 /********FS client setup*************************
  * get_local_info() will set the ip port details of the host
  * And set TCP or UDP connection based on last arg 1 or 0 resp
  * UDP connection details are pointeed to local_cli_info_UDP and TCP by local_srv_info_TCP
  * */

if(!mainProc)
{
	/*Setup FS 2*/
	errVal = get_local_info(&reqmt,&local_cli_info_UDP,FS2_UDP, 0);
	memset(&reqmt,0,sizeof(reqmt));
	get_local_info(&reqmt,&local_srv_info_TCP,FS2_TCP,1);
	/*msgBuf has the information which has to be sent to DS*/
	sprintf(msgBuf,"File_Server2 %d",FS2_TCP);
}
else 
{
	/*Setup FS 1*/
	errVal = get_local_info(&reqmt,&local_cli_info_UDP,FS1_UDP,0);
	memset(&reqmt,0,sizeof(reqmt));
	get_local_info(&reqmt,&local_srv_info_TCP,FS1_TCP,1);
	sprintf(msgBuf,"File_Server1 %d",FS1_TCP);
/******************************************** Forking File_Server3 ************************************************************************************/

	fs1 = fork(); // FS3
	if(!fs1)
	{
		/*Setup FS3*/
		errVal = get_local_info(&reqmt,&local_cli_info_UDP,FS3_UDP,0);
		memset(&reqmt,0,sizeof(reqmt));
		get_local_info(&reqmt,&local_srv_info_TCP,FS3_TCP,1);
		sprintf(msgBuf,"File_Server3 %d",FS3_TCP);

	}
}

temp_UDP = local_cli_info_UDP;
temp_TCP = local_srv_info_TCP;
struct sockaddr_storage their_addr;
/*char s[INET6_ADDRSTRLEN];*/

/**********************************************UDP client setup****************************************************************************************
 * Create a socket
 * bind it to the static port number according to spec
 * Throw appropriate errors if any
 * */

/* loop through available results in the host from getaddrinfo()*/ 
 while(temp_UDP != NULL) 
 {
	sockfd = socket(temp_UDP->ai_family, temp_UDP->ai_socktype,temp_UDP->ai_protocol);
	if(sockfd < 0)
	{
		temp_UDP = temp_UDP->ai_next;

	}
	else
	{
		break;
	}
	
 }
 if (temp_UDP == NULL) 
 {
	fprintf(stderr, "FileServer: failed to create UDP socket\n");
	return 1;
 }

/*Bind to static port*/
  errVal = bind(sockfd, temp_UDP->ai_addr, temp_UDP->ai_addrlen);
 
  if(errVal < 0)
  {
	close(sockfd);
	perror("FileServer: bind");
	return 1;
  }
struct sockaddr_in* pa = (struct sockaddr_in*)(temp_UDP->ai_addr);
char tmp[10];
/*msgBuf has the information which has to be sent to DS*/
sscanf(msgBuf,"%s",tmp);
/*Extracting FileServer name*/
tmp[12]='\0';
const char t[12];
strcpy((char *)t,tmp);

host=gethostbyname("nunki.usc.edu");
if(host==NULL)
{
 perror("Error in gethostbyname(): ");
 exit(1);
}
len = sizeof(sin);
if (getsockname(sockfd, (struct sockaddr *)&sin, &len) < 0)
    perror("getsockname");

printf("Phase 1: The %s has UDP port number %d and IP address %s\n",t,/*pa->sin_port*/ntohs(sin.sin_port),/*inet_ntoa(pa->sin_addr)*/inet_ntoa(*((struct in_addr *)host->h_addr)));
 
freeaddrinfo(local_cli_info_UDP);

 /*************************************UDP client setup complete*****************************************************************************************************/

 /***************************************TCP server - client setup start***********************************************************************************************/
 int tcp_sockfd;
 int yes=1;
     // loop through all the results and bind to the first we can
 while(temp_TCP != NULL) 
 {
	tcp_sockfd = socket(temp_TCP->ai_family, temp_TCP->ai_socktype,temp_TCP->ai_protocol);
	if(tcp_sockfd < 0)
	{
		temp_TCP = temp_TCP->ai_next;
	}
	else
	{
		break;
	}
	
 }
 if (temp_TCP == NULL) 
 {
	fprintf(stderr, "FileServer: failed to create TCP socket\n");
	return 2;
 }

/*Sets Reuse on socket*/
/*Code Block from Beej's Guide start*/
 if (setsockopt(tcp_sockfd, SOL_SOCKET, SO_REUSEADDR, &yes,sizeof(int)) == -1)
{
	 perror("FileServer TCP socket setsockopt");
	 return 1;
}
/*Code Block from Beej's Guide end*/

 if (bind(tcp_sockfd, temp_TCP->ai_addr, temp_TCP->ai_addrlen) == -1)
{
	    close(tcp_sockfd);
	    perror("FileServer TCP socket: bind");
	    return 1;
}


 /****************************************TCP setup complete******************************************************************************************************/

 /****************************************UDP Client start *************************************************************************************************************
  * File Server registers itself with the Directory Server(DS)
  * Sends a msg to DS "File_Server# <TCP_port#>"
  */
/*setup destination info*/
 struct addrinfo  dest, *destptr;
 int bytes_read;
 memset(&dest, 0, sizeof(dest));
 dest.ai_family = AF_INET; /*IPv4*/	
 dest.ai_socktype = SOCK_DGRAM;/*UDP connection*/ 
 dest.ai_flags = AI_PASSIVE; /* use local IP*/
 char buf[BUFLEN];
 /*htons for directory server port number*/
 sprintf(buf,"%d",htons(DIRSERVERPORT));

 /*Get destination info in destptr */
 if((errVal = getaddrinfo("nunki.usc.edu", buf, &dest, &destptr)) < 0)
{
	close(sockfd);
	printf("FileServer: getting address details of Directory Server failed\n");
	return -1;
}
/*Sending Registration to DirectoryServer*/
 if ((bytes_read = sendto(sockfd, msgBuf, (strlen(msgBuf)+1), 0,destptr->ai_addr, destptr->ai_addrlen)) < 0) 
{
	close(sockfd);
	perror("FileServer UDP Registration : sendto");
	exit(1);
}
  printf("Phase 1: The Registration request from %s has been sent to the Directory Server\n",t);
  freeaddrinfo(destptr);
  close(sockfd);/*Close the socket fd*/
  printf("Phase 1: End of Phase 1 for %s\n", t);
/*********************************************************** UDP Client end *****************************************************************************************/

/*********************************************************** TCP server start ***************************************************************************************/

/*Setup a listen queue*/
if (listen(tcp_sockfd, QUEUE) == -1) 
{
	perror("FileServer TCP listen");
	exit(1);
}

host=gethostbyname("nunki.usc.edu");
if(host==NULL)
{
 perror("Error in gethostbyname(): ");
 exit(1);
}
len = sizeof(sin);
if (getsockname(tcp_sockfd, (struct sockaddr *)&sin, &len) < 0)
    perror("getsockname");

pa = (struct sockaddr_in*)(temp_TCP->ai_addr);
printf("Phase 3: The %s has TCP port number %d and IP address %s\n",t,/*pa->sin_port*/ntohs(sin.sin_port),/*inet_ntoa(pa->sin_addr)*/inet_ntoa(*((struct in_addr *)host->h_addr)));
    
freeaddrinfo(local_srv_info_TCP); // all done with this structure

socklen_t sin_size;
/*Accepting new connections*/
 while(1)
{  
	sin_size = sizeof their_addr;
	new_fd = accept(tcp_sockfd, (struct sockaddr *)&their_addr, &sin_size);
	if (new_fd == -1) 
	{
	    perror("accept");
	    continue;
	}

/*Forking to handle each new incoming socket connection*/	
	if (!fork()) /*Reference to Beej's guide code block*/
	{ // this is the child process
	    close(tcp_sockfd); // child doesn't need the listener

	    memset(&buf,0,sizeof(buf));
	    
	    if ((bytes_read = recv(new_fd, buf, BUFLEN-1, 0)) == -1)
	     {
		perror("recv");
	     }
	     char clientName[BUFLEN];
	     char docNumber[10];
	     memset(&docNumber,0,sizeof(docNumber));
	     /*Extract the fileName to be sent back*/
	     sscanf(buf,"%s %s",clientName,docNumber);
	     printf("Phase 3: %s received the request from the %s for the file %s\n",t,clientName,docNumber);

	     /*Response to the received request. Sending back just the documentName*/
	     if (send(new_fd, docNumber, strlen(docNumber)+1, 0) == -1)
		perror("send");
	    printf("Phase 3: %s has sent %s to %s\n",t,docNumber,clientName);	
	    close(new_fd);
	    exit(0);
	}
	close(new_fd);  // parent doesn't need this
    }

}

