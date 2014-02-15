/********************************************************************************************************************************************************************
 * directory_server.c
 * Author: Rohith Narasimhamurthy
 * Date: 23 Nov 2013
 * Registers File Servers and creates directory.txt with their TCP port details. 
 * Accepts Discover requests from clients and responds with the contact details of File server port for download 
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

#define REG_PORT 21498     /*This is port number on which Directory Server receives UDP for Registration*/
#define DISCV_PORT 31498   /*This is port number on which Directory Server receives UDP for Discover*/

#define BUFLEN 50
pid_t discv = -1;          /*Identifies the child process*/
typedef struct top{	   /*Holds the list of File Servers which has requested document number*/	
	int pres;
	char FileServer[BUFLEN];
	char DocName[BUFLEN];
}top_t;

/*  * get_local_info() will set the ip port details of the host
  * And set TCP or UDP connection based on last arg 1 or 0 resp
  * return: Sets the local_srv_infoPtr pointer
  * * */
int get_local_info(struct addrinfo * reqmtPtr,struct addrinfo** local_srv_infoPtr)
{
 int errVal = -1;
 char buf[BUFLEN];
 /*Set requirements to get local address info*/
 memset(reqmtPtr, 0, sizeof(*reqmtPtr));
 reqmtPtr->ai_family = AF_INET; /*IPv4*/	
 reqmtPtr->ai_socktype = SOCK_DGRAM;/*UDP connection*/
 reqmtPtr->ai_flags = AI_PASSIVE; /* use local IP*/

 /*Child process - Handles Discover Phase
  *Parent process - Handles Registration Phase
  *discv = fork() -> discv = 0 is child
  * */
 /*using htons*/
 sprintf(buf,"%d",discv == 0?htons(DISCV_PORT):htons(REG_PORT));

 struct addrinfo * temp = *local_srv_infoPtr;
  /*Get local info in local_srv_info str*/
 errVal = getaddrinfo("nunki.usc.edu"/*NULL*/, buf, reqmtPtr, &temp);
*local_srv_infoPtr = temp;
 if (errVal != 0) 
 {
	fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(errVal));
 }
 return errVal;
}

top_t top[3]; /*Array for structure to hold the file servers list*/

int main(void)
{

 memset(top,0,sizeof(top_t));
 int sockfd = -1;
 struct addrinfo reqmt;
 struct addrinfo *local_srv_info = NULL, *temp = NULL;
 int errVal =-1;
 int recv_bytes = -1;
 struct sockaddr_in client_addr;
 char recv_buf[BUFLEN];
 socklen_t addr_len;
 struct hostent *host;
 struct sockaddr_in sin;
 socklen_t len; 
/****************************************Check directory.txt .Remove if existing *****************************************************************************************/
 if(access("directory.txt",F_OK)!= -1) 
 {
    /*File exists*/
   remove("directory.txt");
 }
 
 
/***************************************** Fork to create 2 processes to listen on DISCOVER and REGISTRATION port **************************************************/ 
 discv = fork();
 /*Child process - Handles Discover Phase
  *Parent process - Handles Registration Phase
  *discv = fork() -> discv = 0 is child
  * */
 
 /*Get local host info according to parent/ child process*/
errVal = get_local_info(&reqmt,&local_srv_info);
temp = local_srv_info;

 /*********************************************************** UDP server setup ****************************************************************************
  * get_local_info() will set the ip port details of the host
  * Create socket and bind it to local port
  * */

 while(temp != NULL) 
 {
	sockfd = socket(temp->ai_family, temp->ai_socktype,temp->ai_protocol);
	if(sockfd < 0)
	{
		temp = temp->ai_next;
	}
	else
	{
		break;
	}
	
 }
 if (temp == NULL) 
 {
	fprintf(stderr, "DirectoryServer: failed to create socket\n");
	return 2;
 }

/* This binds DISCOVER PORT 31xxx to child process 
 * and binds REG PORT 21xxx to parent process*/

  errVal = bind(sockfd, temp->ai_addr, temp->ai_addrlen);
 
  if(errVal < 0)
  {
	close(sockfd);
	perror("DirectoryServer : bind");
	return 1;
  }
struct sockaddr_in* pa = (struct sockaddr_in*)(temp->ai_addr);
host=gethostbyname("nunki.usc.edu");
if(host==NULL)
{
 perror("Error in gethostbyname(): ");
 exit(1);
}
len = sizeof(sin);
if (getsockname(sockfd, (struct sockaddr *)&sin, &len) < 0)
    perror("getsockname");

printf("Phase %d: The Directory Server has UDP port number %d and IP address %s\n",(discv>0)?1:2,/*pa->sin_port*/ntohs(sin.sin_port),/*inet_ntoa(pa->sin_addr)*/inet_ntoa(*((struct in_addr *)host->h_addr)));

 freeaddrinfo(local_srv_info);/*free the str, no further usage*/


/*************************** UDP server setup complete **************************************************************/
 addr_len = sizeof(client_addr);

 int endcount1 =0,endcount2 =0;/*Exit counters*/
 /*Wait for UDP requests*/
 while (1) 
 {
	 memset(recv_buf,0,BUFLEN);
	 int numbytes = recvfrom(sockfd, recv_buf, BUFLEN-1 , 0,(struct sockaddr*)&client_addr, &addr_len);
	 struct sockaddr_in dest_address;
	 int addr_size = sizeof(dest_address);
	 if(numbytes < 0)
	 {
		printf("Error in recvfrom in Directory Server\n");
		continue;
	 }
	 else
	 {
		 /*For Registration discv =1 */
		if(discv)
		{
			if((errVal = registration_request_handler(recv_buf)) < 0)
			{
				printf("Error reading client data\n");
			}
			endcount1++;/*3 file servers*/
			if(endcount1 == 3)
			{
				printf("Phase 1: The directory.txt file has been created.\nEnd of Phase 1 for the Directory Server\n");
				break;
		
			}
		}
		else
		{
			/*Discover request*/		
			char data[BUFLEN];
			char clientNo[BUFLEN];
			memset(&clientNo,0,sizeof(clientNo));
			discover_request_handler(recv_buf,data,clientNo);
			sendto(sockfd, data, (strlen(data)+1), 0,(struct sockaddr*)&client_addr, addr_size);
			
		printf("Phase 2: File server details has been sent to %s\n",clientNo);
			endcount2++; /*2 clients-so max is 2*/
			if(endcount2 == 2)
			{	printf("Phase 2: End of Phase 2 for the Directory Server\n");
			exit(0);/*exit child after servicing 2 clients*/
			}
		}
	 }
 }
close(sockfd);
waitpid(discv,NULL,0);/*Wait for child to complete and return*/
exit(0);

}


/********************************************************************************************************************************************************************
 * registration_request_handler
 * Writes the buf into directory.txt file as a line. If file already exists, then appends. 
 * returns 0 on success
 * *******************************************************************************************************************************************************************/
int registration_request_handler(char * buf)
{
	FILE *fp;
	fp=fopen("directory.txt", "a+w");
	if(fp != NULL)
	{
		int i = fprintf(fp,"%s\n",buf);
		if(i<=0)
			printf("error\n");
		char temp[BUFLEN];
		sscanf(buf,"%s",temp);	
		printf("Phase 1: The Directory Server has received request from %s\n",temp);

	fclose(fp);	

	}
	else
	  return -1;	
	return 0;
	
}
/********************************************************************************************************************************************************************
 * discover_request_handler
 * Searches resources.txt for the requested docNumber and fills top struct 
 * If more than 1 file servers contain the docNumber, then calc_cost() gives the least cost FileServer
 * get_dir_info() gives the TCP port of this Fileserver, all combined in data.
 * returns by updating data
 * *******************************************************************************************************************************************************************/

int discover_request_handler(char* buf, char data[BUFLEN],char* clientNo)
{
 char clientNumber[BUFLEN];
 char docNumber[BUFLEN];
 memset(docNumber,0,sizeof(docNumber));
 /*Extract client name and document name from buf*/
 char *p = strtok(buf," ");
 strncpy(clientNumber,p,strlen(p));
 clientNumber[strlen(p)] = '\0';
 p = strtok(NULL," ");
 strncpy(docNumber,p,strlen(p));
 docNumber[strlen(p)] = '\0';
 strcpy(clientNo,clientNumber);
 printf("Phase 2: The Directory Server has received request from %s\n",clientNumber);
 FILE *fp = fopen("resource.txt","r");
 if(fp ==NULL)
 {
	 printf("Try again later- Waiting to connect to File Servers\n");
	 return -1;
 }
 int i =0,flag =0; //for common str
 while(NULL != fgets(buf,BUFLEN,fp))
 {
	char temp[BUFLEN];
	strncpy(temp,buf,strlen(buf));
	temp[strlen(buf)-1] = '\0';
	char* ptr = strtok(temp," ");
	if(ptr == NULL)
		continue;	
	char fileName[BUFLEN];
	int howMany;
	strcpy(fileName,ptr);
	ptr = strtok(NULL, " ");
	if(ptr == NULL)
		continue;
	howMany = atoi(ptr);
	
	while(howMany)
	{
		ptr = strtok(NULL, " ");
		if(ptr == NULL)
			break;
	
		if(strcmp(ptr,docNumber) == 0)
		{
			//found.populate top str
			strcpy(top[i].FileServer,fileName);
			strcpy(top[i].DocName,docNumber);
			top[i].pres =1;
			i++;

			if(i > 1)
				flag =1;	
			if(i > 3)
				return -1;
		}
		howMany--;
	}
 }
char fileServer[BUFLEN];
/*If more than 1 FS has the doc, then calculate the cost*/
 if(flag)
 {	 
	 calc_cost(top, clientNumber,fileServer);//gives the fileServer name of least cost
 }
 else
	 strcpy(fileServer,top[0].FileServer);
/*Get the TCP port of the FS*/
  int port = get_directoryInfo(fileServer);
  fclose(fp);
  sprintf(data,"%s %d",fileServer,port);
  return 0;

}
/********************************************************************************************************************************************************************
 * get_dirInfo() gives the TCP port of Fileserver
 *
 * *******************************************************************************************************************************************************************/

int get_directoryInfo(char* fileServer)
{
	FILE* fp = fopen("directory.txt","r");
	char name[BUFLEN],buf[100];
	int port,flag = 0;
	while(NULL != fgets(buf,100,fp))
	{
		buf[strlen(buf)] = '\0';
		sscanf(buf,"%s %d",name,&port);
		if(strcmp(fileServer,name) == 0)
		{
			flag =1;
			break;
		}

	}	
if(flag)
	return port;
printf("Error\n");
return -1;

}
/********************************************************************************************************************************************************************
 * Calculates the least cost path based on topology.txt
 * returns least cost fileServer name
 * *******************************************************************************************************************************************************************/

int calc_cost(top_t* top,char* clientNumber, char* fileServer)
{
	FILE *fp = fopen("topology.txt","r");
	char buf[BUFLEN];
	/*Read 1st line if Client1 else read the 2nd line for client2*/
	if(strcmp(clientNumber,"Client1")==0)
		fgets(buf,BUFLEN,fp);
	else if(strcmp(clientNumber,"Client2")==0)
	{
		fgets(buf,BUFLEN,fp);
		memset(buf,0,sizeof(buf));
		fgets(buf,BUFLEN,fp);
	}
	buf[strlen(buf)-1] = '\0';
	int cFS1=0,cFS2=0,cFS3=0,i=0,found =0;
	char* least[3];
	/*Get the costs*/
	sscanf(buf,"%d %d %d", &cFS1,&cFS2,&cFS3);

	/*Arrange the File Servers in increasing order of cost in least[]. least[0] holds the least cost*/
	if((cFS1<cFS2))
	{
		if(cFS1<cFS3)
		{
			(least[0]="File_Server1");

			if(cFS2 > cFS3)
			{
				(least[1]="File_Server3");
				(least[2]="File_Server2");
			}
			else
			{
				(least[1]="File_Server2");
				(least[2]="File_Server3");
			
			}	

		}
		else
		{
			least[0]="File_Server3";
			least[1]="File_Server1";
			least[2]="File_Server2";
		}
	}
	else
	{
		if(cFS2<cFS3)
		{
			(least[0]="File_Server2");

			if(cFS1 > cFS3)
			{
				(least[1]="File_Server3");
				(least[2]="File_Server1");
			}
			else
			{
				(least[1]="File_Server1");
				(least[2]="File_Server3");
			
			}	

		}
		else
		{
			/*When all 3 FS costs are equal case also hits this*/
			least[0]="File_Server3";
			least[1]="File_Server2";
			least[2]="File_Server1";
		}

	}
	// found the server which has least cost
	
	/*Returns the least cost server among the top list*/
	/*Starting from least[0], FileServer names in top (iterated through i) are checked if same to return the least cost server*/
	int j = 0;
	while(j<3)
	{
		i =0;	
		while(i<3)
		{
				
			if(top[i].pres)
			{
			
				if(!strcmp(top[i].FileServer,least[j]))
				{
					found =1;
					break;
				}
			}
			i++;
		}
		if(found == 1)
			break;
		j++;
	}	

	if(found != 1)
	{
		//least cost FS not registered.
		return -1;

	}
	fclose(fp);
	strncpy(fileServer,least[j],BUFLEN);
	return 0;
}

