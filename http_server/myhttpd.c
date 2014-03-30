#define		BUF_LEN	8192
#define 	BUF_SIZE 512
#include	<time.h>
#include	<stdio.h> 
#include	<stdlib.h>
#include	<string.h>
#include	<ctype.h>
#include	<sys/types.h>
#include	<sys/socket.h>
#include	<netdb.h>
#include	<netinet/in.h>
#include	<inttypes.h>
#include	<sys/stat.h>
#include 	<pthread.h>
#include 	<fcntl.h>
#include 	<unistd.h>
#include	<arpa/inet.h>
#include	<dirent.h>

/*declaring global variables*/
char *progname;
char buf[BUF_LEN];
char *root_dir="./";
char *logfile=NULL;
void usage();
void read_file(int sock,char filename[]);
int setup_client();
int setup_server();
int s, sock, ch, done, bytes, aflg;
int server=1;
int soctype = SOCK_STREAM;
char *host = NULL;
char *port = "8080";
extern char *optarg;
pthread_mutex_t lock1;
pthread_mutex_t lock2;
extern int optind;
char ip_ad[INET_ADDRSTRLEN];
void *schedule_function( void *ptr );
void directory_listing(void *data,int sock);

/*--FLAGS--*/
int SJF=0;
int FCFS=0;
int LF=0;  //enable logginf
int d=0;   //debugging mode
int t=60;	//time of sleep in schedule_function
int n=4;	//number of worker threads
int r=0;	//flag to check root directory change as per user
int dir_list=0;

typedef struct Node 
{  
	char Data[200];  
	int sock_id;
	char request_type[25];
	int f_size;
	char l_modi[50];
	char l_modi_GMT[50];
	char ip[25];
	time_t time_queue;
	time_t time_excute;
	char status[3];
	char first_line[200];
	struct Node *next;  
}Node;

typedef struct
{
	Node *head;
	Node *tail;
}queue;

queue *abcd;   /*1st queue-takes all the request as they come*/ 
queue *q2;	/*2nd queue*/

void *dequeue(queue * abcd);
void display(queue * abcd,int sock);
void add(char *data,int sock,char *request_type,char *ip_ad,char *first_line);
void add_to_sec(Node *n);
void create_list();
Node *get_smallest_node();
Node *get_first();
void daemonize(void);

/*function to get the smallest node for SJF*/
Node *get_smallest_node()
{
    Node *j;
    Node *i;
    Node *small;
    i=abcd->head;
    small=abcd->head;
    if(i->next==NULL)
      {
	small=i;
	abcd->head=NULL;
	abcd->tail=NULL;
	return small;
      }  
    else  
	{
		j=i->next;
		while(j!=NULL)
		  {
		      if(j->f_size <= small->f_size)
			small=j;
		      j=j->next;
		  } 
		  if(small!= i)
		  {      
		      while(i->next!=small)
		      {
			i=i->next;
		      }
		  }
	      i->next=small->next;
	}
	if(abcd->tail==small){
	  abcd->tail=i;
	}
	if(abcd->head==small)
	{
	  abcd->head=i->next;
	}
    small->next=NULL;
    return small;
}

void create_list()
{
	abcd->head=NULL;
	abcd->tail=NULL;
	q2->head=NULL;
	q2->tail=NULL;
}


/*function to add request from 1st q to 2nd queue*/
void add_to_sec(Node * n)
{
    pthread_mutex_lock(&lock2);
	if(q2->head==NULL)
	{
		q2->head=n;
		q2->tail=n;
	}
	else
	{
		q2->tail->next=n;
		q2->tail=q2->tail->next;	
	}
    pthread_mutex_unlock(&lock2);
}


/*function to add request to the 1st queue*/
void add(char *str1, int sock,char *request_type,char *ip_ad,char *first_line)
{
	int ret=0;
	Node *n;
	time_t now = time(NULL);
	n=malloc(sizeof(Node));
	memset(n->first_line,0,sizeof(n->first_line));
	memset(n->Data,0,sizeof(n->Data));
	strcpy(n->Data,str1);
	struct stat buf;
	ret = stat(str1, &buf); 		/*check if file present*/
	if(ret==0)
	  {
		  n->f_size=buf.st_size;
		  strcpy(n->l_modi,ctime(&(buf.st_mtime)));	
		  strcpy(n->l_modi_GMT,asctime(gmtime(&(buf.st_mtime))));
	  }
	else
	  {
		n->f_size=5;			/*arbitary size if file not present*/
	  }
        n->time_queue=now;	 
	n->sock_id=sock;
	strcpy(n->ip,ip_ad);
	strcpy(n->first_line,first_line);
	strcpy(n->request_type,request_type);

	if(n->request_type=="HEAD")
	    n->f_size=1;

	n->next=NULL;
	
pthread_mutex_lock(&lock1);

	if(abcd->head==NULL)
	  {
		  abcd->head=n;
		  abcd->tail=n;
	  }
	else
	  {
		  abcd->tail->next=n;
		  abcd->tail=abcd->tail->next;	
	  }
	pthread_mutex_unlock(&lock1);
}


void *worker_function( void *ptr )
{
	char *str1, *filename, *request_type, str2[bytes], *con_type, *readBuf = NULL;
	FILE*  fp1;
	FILE *fp2;
	int    ret,a;

	char status[16]="HTTP/1.0 200 OK\n";
	char c_time[30];
	char server_name[18]="Server:myhttp 1.0\n";
	char l_modified[40];
	char c_type[25];
	char h_size[30];
	char ch;
	
	char q_time[50];		 
	char q_execution[50];
	char f_line[100];
	char log_status[10];
	char f_l_size[10];
	
	time_t now;
	char header[250];

	char req_type[25];
	char h_request[10];
	size_t read;
	void * data;
	int sock;

	Node * temp = q2->head;
	readBuf = (char*) malloc( sizeof(char) * BUF_SIZE );
  while(1)
  {
	pthread_mutex_lock(&lock2);
	if(q2->head!=NULL)
	  {
	      now = time(NULL);
	      q2->head->time_excute = now;
	      memset(readBuf,0, sizeof(char) * BUF_SIZE );
	      memset(q_time,0, sizeof(char) * 50 );
	      memset(q_execution,0, sizeof(char) * 50 );
	      memset(c_time,0,sizeof(char)*30);	      
	      memset(l_modified,0,sizeof(char)*40);
	      memset(c_type,0,sizeof(char)*25);
	      memset(h_size,0, sizeof(char)*30);
	      memset(header,0,sizeof(char)*200);
	      memset(f_line,0,sizeof(char)*100);
	      memset(f_l_size,0,sizeof(char)*10);

	      strcpy(l_modified,"Last-Modified:");
	      strcpy(c_type,"Content-Type:");
	      
	      data=q2->head->Data;
	      sock=q2->head->sock_id;

	      strcpy(req_type,q2->head->request_type);
	      strcpy(f_line,q2->head->first_line);
	      strncat(q_time,asctime( gmtime(&q2->head->time_queue)),strlen(asctime( gmtime(&q2->head->time_queue)))-1);
	      strcpy(log_status,q2->head->status);
	      strncat(q_execution,asctime(gmtime(&q2->head->time_excute)),strlen(asctime(gmtime(&q2->head->time_excute)))-1);
	      sprintf(f_l_size,"%d",q2->head->f_size);
	      sprintf(h_size,"Content-Length:%d bytes\n",q2->head->f_size);
	      strcat(c_time,"DATE:");
	      strcat(c_time,asctime( gmtime((const time_t*)&now)));	      
	      strcat(l_modified,q2->head->l_modi_GMT);
	      strcpy(h_request,"HEAD");	      

	      a=strcmp(req_type,h_request);	/*check if request is of type Head*/
	      
	      if(strstr(q2->head->Data,".html")!=NULL || strstr(q2->head->Data,".txt")!=NULL)
		  strcat(c_type,"text/html");
	      
	      if(strstr(q2->head->Data,".gif")!=NULL ||strstr(q2->head->Data,".jpg")!=NULL)
		  strcat(c_type,"image/gif");

	      strcat(h_size,"\n");
	      strcat(c_type,"\n");

/*creating header protocol*/

	      strcat(header,status);
	      strcat(header,c_time);
	      strcat(header,server_name);
	      strcat(header,l_modified);
	      strcat(header,c_type);
	      strcat(header,h_size);

	      if(a==0)
	      {  
		  send(sock, header, sizeof(char)*200, 0);
	      }
	      else
	      {
		fp1 = fopen(data, "r" );
		if (fp1==NULL)
		  {
		    if(dir_list==1)
		    {
		      directory_listing(data,sock);
		      dir_list=0;
		    }
		    else
		    {
		      strcpy(q2->head->status,"404");		      
		      perror(NULL);
		      send(sock, "404: REQUEST NOT FOUND\n", sizeof(char)*4, 0);
		    }
		  }
		else
		  {		    
			send(sock, header, sizeof(char)*200, 0); 
			while (0 < (read = fread( (void*) readBuf, sizeof(char), BUF_SIZE - 1, fp1 )) )
			{
				readBuf[read] = '\0'; 
				send(sock, readBuf, read, 0);	
			}
			fclose(fp1);
		  strcpy(q2->head->status,"200");
		  }
		  
	  if(d==0)
	  {
	    /*Logging file if flag enabled*/
	      if(LF==1)
	      {
		
		    fp2 = fopen( logfile , "a" );
		    
		    fputs(q2->head->ip, fp2 );
		    fputs(" ", fp2 );
		    fputs("-", fp2 );
		    fputs(" ", fp2 );
		    fputs("[",fp2);
		    fputs(q_time, fp2 );
		    fputs("] ",fp2);
		    fputs("[",fp2);
		    fputs(q_execution, fp2 );
		    fputs("] ",fp2);
		    fputs(f_line, fp2 );
		    fputs(" ", fp2 );
		    fputs(q2->head->status, fp2 );
		    fputs(" ", fp2 );
		    fputs(f_l_size, fp2 );
		    fputs("\n",fp2);
		    fputs("\n",fp2);
		      fclose(fp2);
	      }
	  }
	  else
	      printf("%s - [%s] [%s] %s %s %s \n \n",q2->head->ip,q_time,q_execution,f_line,q2->head->status,f_l_size);
	}

	      close(sock);
	if(q2->head->next==NULL)
	  {
	    q2->head=NULL;
	    q2->tail=NULL;
	  }
	  else
	  {
	    q2->head=q2->head->next;
	  }
	  free(temp);
    }
      pthread_mutex_unlock(&lock2);
  }
}


/*fucntion to get the first request in FCFS*/
Node *get_first()
{
	Node *i=abcd->head;
	abcd->head=abcd->head->next;
	if(i->next=NULL)
	  abcd->tail=NULL;
	i->next=NULL;
	return i;
}

void *schedule_function( void *ptr )
{
	char *filename, *request_type, *con_type, *readBuf = NULL;
	char *str1;
	FILE*  fp1;
	size_t read;	
	char *data;
	int sock;
	int fsize;
	Node * temp;
	
	data=malloc(sizeof(char)*200);
	memset(data,0,sizeof(char)*200);
	sleep(t);
	while(1)
	{
pthread_mutex_lock(&lock1);
	    if(abcd->head!=NULL)
	    {
		if(SJF==1)
		  {
		    temp = get_smallest_node();
		  }
		else
		{
		  temp = get_first();	
		}
		add_to_sec(temp);
		pthread_mutex_unlock(&lock1);	      
	    }
	    else
	      {	      
		pthread_mutex_unlock(&lock1);
	      }
	}
}

/*FUNCTION to parse the request to extract parameters */
void parse(int sock, void* buf, int bytes,char *ip_ad)
{
	int i=0;
	int j=0;
	char *check_tilda;
	char copy_request[200];
	char user_folder[30];
	char extension[50];
	char str1[100], *filename, first_line[200], request_type[20], str2[100], con_type[25], *readBuf = NULL;
	FILE*  fp1;
	size_t read;
	if (bytes > 0)
	{	
		memset(str1,0,sizeof(char)*50);
		memset(str2,0,sizeof(char)*50);
		memset(extension,0,sizeof(char)*50);
		str1[0]='.';
		if(r==1)
		  strcpy(str1,root_dir);
		check_tilda=strstr(buf,"~");
		if(check_tilda!=NULL)
		{
		    sscanf( buf, "%s %s %s", request_type, copy_request, con_type);
		    sscanf(copy_request+2,"%[^,'/']%s ",user_folder,extension);
		    
		      if(extension[0]=='\0' || extension[1]=='\0')
		      {
			      strcpy(str1,"/home/");
			      strcat(str1,user_folder);
			      strcat(str1,"/myhttp");
			      strcat(str1,"/index.html");	    
		      }
		      else
		      {	
			      strcpy(str1,"/home/");
			      strcat(str1,user_folder);
			      strcat(str1,extension);

			      if(strstr(str1,".")==NULL)
			      {
				strcat(str1,"index.html");
				dir_list=1;
			      }
		      }
		    
		    strcpy(first_line,"\"");
		    strcat(first_line,request_type);
		    strcat(first_line," ");
// 		    strcpy(first_line,"/~");
// 		    strcat(first_line,user_folder);
 		    strcat(first_line,copy_request);
		    strcat(first_line," ");
		    strcat(first_line,con_type);
		    strcat(first_line,"\"");
		}
		else
		{
		    sscanf( buf, "%s %s %s", request_type, str2, con_type);

		    strcat(str1, str2);

		    strcpy(first_line,"\"");
		    strcat(first_line,request_type);
		    strcat(first_line,str2);
		    strcat(first_line," ");
		    strcat(first_line,con_type);
		    strcat(first_line,"\"");
		}
		add(str1,sock,request_type,ip_ad,first_line);		/*adding file information in the queue*/
		buf=NULL;
	}
}

int main(int argc,char *argv[])
{
	pthread_t thread1;
	pthread_t *thread_array;
	fd_set ready;
	struct sockaddr_in msgfrom;
	int msgsize,i=0;
	char *policy="FCFS";
	char ipa[50];
	int policy_FCFS=1;
	int policy_SJF=1;
	int port_check;
	pid_t pid,sid;
	
	// Setup Server
	struct sockaddr_in serv, remote;
	struct servent *se;
	int newsock, len, daemon_pid;
		//End setup
	union {
		uint32_t addr;
		char bytes[4];
	} fromaddr;
	pthread_mutex_init(&lock2,NULL);
	abcd=(queue *)malloc(sizeof(queue));	
	q2=(queue *)malloc(sizeof(queue));	
	create_list();
	if ((progname = rindex(argv[0], '/')) == NULL)
		progname = argv[0];
	else
		progname++;
	while ((ch = getopt(argc, argv, "l:ds:p:ht:n:r:")) != -1)
		switch(ch) {
			case 'd':
				d=1;
				break;
			case 's':
				policy=optarg;
				policy_SJF=strcasecmp(policy,"SJF");
				policy_FCFS=strcasecmp(policy,"FCFS");
				if(policy_SJF==0)
				{  
				  SJF=1;
				}
				else if(policy_FCFS==0)
				{  
				  SJF=0;
				}
				else
				  usage();
				  exit(1);
				break;
			case 'p':
				port = optarg;
				port_check=atoi(port);
				if (port_check < 1024)
				  {
				      fprintf(stderr, "[error] Port number must be greater than or equal to 1024.\n");
				      exit(1);
				  }
				break;				
			case 't':
				t = atoi(optarg);
				if (t < 1)
				{
					fprintf(stderr, "[error] queueing time must be greater than 0.\n");
					exit(1);
				}
				break;
			case 'n':
				n = atoi(optarg);
				if (n < 1)
				{
					fprintf(stderr, "[error] number of threads must be greater than 0.\n");
					exit(1);
				}
				break;
			case 'r':
				root_dir = optarg;
				break;
			case 'l':
				LF=1;
				logfile = optarg;
				break;
			case 'h':
				usage();
				break;
			case '?':
			default:
				usage();
		}
	argc -= optind;
	if (argc != 0)
		usage();
	if (!server && (host == NULL || port == NULL))
		usage();
	if (server && host != NULL)
		usage();
	if ((s = socket(AF_INET, soctype, 0)) < 0) {
		perror("socket");
		exit(1);
	}
	
      if(d==1)
      {
	/*printf on stdout if debugging is set*/
	fprintf(stderr, "myhttpd logfile: %s\n", logfile);
        fprintf(stderr, "myhttpd port number: %s\n", port);
        fprintf(stderr, "myhttpd rootdir: %s\n", root_dir);
        fprintf(stderr, "myhttpd queueing time: %d\n", t);
        fprintf(stderr, "myhttpd number of threads: %d\n", n);
	fprintf(stderr, "myhttpd scheduling policy: %s\n", policy);
	n=1;
      }
      else
      {
	/*daemonizing the process*/
	    if((pid=fork())<0)
	    {
		exit(0);
	    }
	      if(pid>0)
	      {
		exit(EXIT_SUCCESS);
	      }
	      umask(0);
	      sid=setsid();
//	      close(0);
//	      close(1);
//	      close(2);
      }
   
	len = sizeof(remote);
	memset((void *)&serv, 0, sizeof(serv));
	serv.sin_family = AF_INET;
	if (port == NULL)
		serv.sin_port = htons(0);
	else if (isdigit(*port))
		serv.sin_port = htons(atoi(port));
	else {
		if ((se = getservbyname(port, (char *)NULL)) < (struct servent *) 0) {
			perror(port);
			exit(1);
		}
		serv.sin_port = se->s_port;
	}
	
	if (bind(s, (struct sockaddr *)&serv, sizeof(serv)) < 0) {
		perror("bind");
		exit(1);
	}
	if (getsockname(s, (struct sockaddr *) &remote, &len) < 0) {
		perror("getsockname");
		exit(1);
	}
//	fprintf(stderr, "Port number is %d\n", ntohs(remote.sin_port));
	
	pthread_create( &thread1, NULL, (void *)&schedule_function, NULL);
	thread_array=(pthread_t *)malloc(sizeof(pthread_t)*n);	
	pthread_mutex_init(&lock1,NULL);
	for(i=1;i<=n;i++)
	    {
		    pthread_create( &thread_array[i], NULL, (void *)&worker_function, NULL);
	    }
	while (1) {
		listen(s, 5);
		char *str_val;
		char needle[15] ="favicon.ico";
		newsock = s;
		if (soctype == SOCK_STREAM) {
			newsock = accept(s, (struct sockaddr *) &remote, &len);
			strcpy(ip_ad,inet_ntoa(remote.sin_addr));
		}
			if(newsock<0)
			  continue;
			  
			sock = newsock;
			FD_ZERO(&ready);
			FD_SET(sock, &ready);
			FD_SET(fileno(stdin), &ready);
			if (select((sock + 1), &ready, 0, 0, 0) < 0) {
				perror("select");
				exit(1);
			}
			memset(buf,0,sizeof(buf));
			msgsize = sizeof(msgfrom);
			if (FD_ISSET(sock, &ready)) {
				if ((bytes = recvfrom(sock, buf, BUF_LEN, 0, (struct sockaddr *)&msgfrom, &msgsize)) <= 0)
				{
				  continue;
				} else if (aflg) 
				{
					fromaddr.addr = ntohl(msgfrom.sin_addr.s_addr);
					fprintf(stderr, "%d.%d.%d.%d: ", 0xff & (unsigned int)fromaddr.bytes[0],
			    		0xff & (unsigned int)fromaddr.bytes[1],
				    	0xff & (unsigned int)fromaddr.bytes[2],
				    	0xff & (unsigned int)fromaddr.bytes[3]);
				}
				str_val=strstr(buf,needle);
				if(str_val==NULL)
				  {
					  parse(sock, buf, bytes,ip_ad);
				  }
				else
					close(sock);
			}
	}
	printf("exiting main\n");
	return(0);
}

/* usage - print usage string and exit- code given by TA*/
void usage()
{
    fprintf(stderr, "Usage: myhttpd [−d] [−h] [−l file] [−p port] [−r dir] [−t time] [−n thread_num]  [−s sched]\n");
 
    fprintf(stderr,
            "\t−d :Enter the debugging mode. That is, do not daemonize, only accept\n"
            "\tone connection at a time and enable logging to stdout. Without\n"
            "\tthis option, the web server will run as a daemon process in the\n"
            "\tbackground.\n"
            "\t−h :Print a usage summary with all options and exit.\n"
            "\t−l file :Log all requests to the given file. See LOGGING for\n"
            "\tdetails.\n"
            "\t−p port : Listen on the given port. If not provided, myhttpd will\n"
            "\tlisten on port 8080.\n"
            "\t−r dir : Set the root directory for the http server to dir.\n"
            "\t−t time : Set the queuing time to time seconds. The default will\n"
            "\tbe 60 seconds.\n"
            "\t−n thread_num : Set number of threads waiting ready in the execution thread pool to\n"
            "\tthreadnum.  The default will be 4 execution threads.\n"
            "\t−s sched : Set the scheduling policy. It can be either FCFS or SJF.\n"
            "\tThe default will be FCFS.\n"
            );
    exit(1);
}

void directory_listing(void *data,int sock)
{
    struct dirent **namelist;
    struct stat buf;
    int i,n,ret=0,j=0,f_size=0;

    char dir_req[100],list[200],l_modi_GMT[75],l_modi_GMT_2[75],*ptr,*ptr2,file_size[30];
    strcpy(dir_req,data);

    ptr=strstr(dir_req,"index.html");

    if(ptr!=NULL)
    {
	while(ptr[j]!='\0')
	{
	  ptr[j]='\0';
	  j++;
	}
    }

	n = scandir(dir_req, &namelist, 0, alphasort);
	if (n <= 0)
	  {
//	    perror("scandir");
	      send(sock, "NOT FOUND", sizeof(char)*9, 0);
	  }
	else 
	{    
	  for (i = 0; i < n; i++) 
	    {
	      if((namelist[i]->d_name)[0]=='.')
		continue;
	      
	      memset(list,0,sizeof(char)*200);
	      memset(file_size,0,sizeof(char)*30);

	      strcpy(list,namelist[i]->d_name);
	      ptr2=namelist[i]->d_name;
	      ret = stat(ptr2, &buf); 		//check if file present
	      if(ret==0)
		{	
			f_size=buf.st_size;
			sprintf(file_size ,"%d",f_size);
			printf("file_size is %s ",file_size);
			strcpy(l_modi_GMT,asctime(gmtime(&(buf.st_mtime))));
			strncpy(l_modi_GMT_2,l_modi_GMT,strlen(l_modi_GMT)-5);
		}
		
		strcat(list,"\t");
		strcat(list,l_modi_GMT_2);
		strcat(list,"\t");
		strcat(list,file_size);
		strcat(list,"\0");

		send(sock, list, sizeof(char)*strlen(list), 0);
		send(sock, "\n", sizeof(char)*1, 0);
		  free(namelist[i]);
	    }
	  strcpy(q2->head->status,"200");        
	free(namelist);
	}
}
