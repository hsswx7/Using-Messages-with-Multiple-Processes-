#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <time.h>
#include <signal.h>
#include <error.h>
#include <assert.h>
#include <sys/msg.h>
#include <errno.h>
#include "share.h"


#define  NANOSECOND    1000000000 

void comOptions (int argc, char **argv,int *x,int *z, char **filename);
void displayHelpMesg();
void validate(int *x,int temp,char y);
void test(int x, int z,char*file);  // this is just to print out put
void savelog(char *filename, msgQueue userMsg, clock_share *sharedTime); //saving Master's logs
void INThandler(int sig);  // handeling Ctrl^C signal
void on_alarm(int signal);  // handeling the alarm(z) signal
void releaseMem(); //one function to delet the shared memory and message queue's 

pid_t pidArr[100];
int psNumber = 0;
int x = 5; //Number of slaves 
int userRemaning  = 0;
int z = 20; //The Seconds time limit on the children running 
int shareID;  //I made this global so INThandler
int critMsgID; //This msgID is for the slaves to enter their critical for mutual exlusion
int dieMsgID;  //Tis msgID is or the master to recieve and print to file and kill user

 key_t key = 1994; // making key for access 
 key_t dieMsgKey = 1991; // Key for the messages
 key_t criticalMsgKey = 1989;  //Key for the children to have a critical section

int main(int argc, char **argv)
{
  signal(SIGALRM, on_alarm); // getting is alarm is activated
  signal(SIGQUIT,INThandler); //Handels SIGQUIT
  signal(SIGINT, INThandler);  // Catching crtl^c
  

  char *filename = "test.out";  // file name for to be written to
  if (argc<2) //Cheking if the user is running the program with arguments 
  {
     printf("Running Program without any Commands, default options will be apply.\n");
  }
  else //calling commmand options if user inputs commands 
  {
    comOptions(argc,argv,&x,&z,&filename);
  }
  test(x,z,filename);
  
  
  /* Making Shared memeory for the NanoSeconds and Seconds */
  clock_share *sharedTime;
  if((shareID = shmget(key, sizeof(clock_share), IPC_CREAT | 0666)) < 0)  // creating the shared memory 
  {
    perror("shmget in parent");
    exit(1);
  }
   
  sharedTime = (clock_share*)shmat(shareID, NULL, 0);
  if (sharedTime->nanoSec ==(int)-1) // Now we attach the segment to our data space.
  {
    perror("Shmat error in Main");
    exit(1);
  }
  sharedTime->nanoSec = 0;  // Starting off with 0
  sharedTime->second = 0; //Starting with 0`
  
    /* Making Queue for the Messages */
  
  if ((critMsgID = msgget(criticalMsgKey, IPC_CREAT | 0666)) < 0) //For mutual exlusion queue
  {
    perror("Error in msgeet Master ");
	exit(1);
  }
  if ((dieMsgID = msgget(dieMsgKey, IPC_CREAT | 0666)) < 0) //user die queue
  {
    perror("Error in msgeet Master ");
	exit(1);
  }
  
  msgQueue mymsg; // setting up my own message for the user's
  mymsg.myPid = getpid(); // i'll be sending them my pid so they know its ther oss
  //putting message in the queue for mutual exlusion
  if (msgsnd(critMsgID, &mymsg, sizeof(mymsg), 0) < 0)//if (msgsnd(critMsgID, &sharedTime, sizeof(sharedTime), 0) < 0) 
  {
    perror("critMsgID Parent : msgsnd");
	releaseMem(); //getting rid of shared memory
	exit(1);
  }
  
  
  /* converting Int's to Strings so I can pass variables to the children */
  char keypass[32]; // this ID will Allow the children to access and view the seconds 
  sprintf(keypass, "%d",key);
  char dieMsgKeyPass[32];  // This will Allow the children to recieve the Critical message Id
  sprintf(dieMsgKeyPass, "%d" , dieMsgKey);
  char criticalMsgKeyPass[32];   // This will Allow the children to send message upon death
  sprintf(criticalMsgKeyPass, "%d" , criticalMsgKey); 
  
  
  int i = 0; // counter for the for loop
  for (i = 0; i < x; i++) // setting up initial X (default 5) children
  { 
    pid_t pid;
    pidArr[i] = pid;
    pid = fork();

    if (pid < 0)
    {
      perror("Fork() Failed"); 
      exit(1); 
    }
    else if (pid == 0)
    {
      printf("Running User #%d Pid: %d\n\n", i,pidArr[i]);
      execl("./user",keypass,dieMsgKeyPass,criticalMsgKeyPass,NULL);
	  
      perror("Child failed to execl");
    }
  }
  
  int runningUsers = x;
  userRemaning = x; // going to count this up to 100 that that's the max
  extern int errno;
  msgQueue usrMsg;
  // msgctl(dieMsgID,IPC_STAT,&userMsg);
  int count = 0;
  struct msqid_ds buf;
  
    alarm(z);  //Setting the Alarm for the time of Z (default 20)
  
  while (1)
   {
	  usrMsg.dieFlag = 0;   
	  sharedTime->nanoSec += 10; // starting and counting with 10000 
      if(sharedTime->nanoSec == 1000000000) //if nanoSec hits 1000000000
      {
        sharedTime->nanoSec = 0; //Resetting the Nanoseconds
		printf("sharedTime->nanoSec %d\n",sharedTime->nanoSec);
        sharedTime->second++; //counting up the seconds`	
        printf("sharedTime->second %d\n",sharedTime->second);		
      }

	 
	  
      if( msgrcv(dieMsgID, &usrMsg, sizeof(usrMsg), 0,IPC_NOWAIT)) // Taking in any user message
	  {
		if (usrMsg.dieFlag == 1)
		{	
	      savelog(filename, usrMsg, sharedTime); //logging ther user's data
	      printf("User ID %d | User time %d.%010d\n", usrMsg.myPid, usrMsg.death_Time.second, usrMsg.death_Time.nanoSec);
	      kill(usrMsg.myPid, SIGUSR1);
	      wait(NULL);
		  runningUsers -= 1;
		  
		  if (userRemaning < 100) //going up to 100
          {
            runningUsers +=1;
            userRemaning +=1;
 
            pid_t pid;
            pidArr[userRemaning]=pid; //storing pid's to kill any left behind zombie user's
            pid = fork();

            if (pid < 0)
            {
               perror("Fork() Failed."); 
               exit(1); 
            }
      
            if (pid == 0)
            {
               printf("Running User #%d pid: %d\n\n", userRemaning,pidArr[userRemaning]);
               execl("./user",keypass,dieMsgKeyPass,criticalMsgKeyPass,NULL);
               perror("Child failed to execl");
            }	   
		 }  
		 if(runningUsers  == 0)
         {
            break;
         }

	   }
	 }
	 if (sharedTime->second == 2)
       {
         printf("User has run out of oss time, the clock is %d.%010d\n", sharedTime->second,sharedTime->nanoSec);
         // releaseMem();
		 kill(getpid(),SIGALRM);
         // exit(1);
       }
	}
  
  printf("Program Finished.\n");
  wait(NULL);
  // Removing Share Data
  releaseMem();
  
  return 0;
}


//Functions
void comOptions (int argc, char **argv , int *x, int *z, char **filename)
{ 
  int c = 0; //This is for the switch statement
  int temp = 0;
  static struct option long_options[] = 
  { 
    {"help", no_argument, 0, 'h'},
    { 0,     0          , 0,  0 } 
  };
  int long_index = 0;

  while((c = getopt_long_only(argc, argv, "hs:t:l:", long_options, &long_index)) != -1)
  {
    switch (c)
    {
      case 'h':  // -h
        displayHelpMesg();
      break;
	  
      case 's':
       temp = *x;
       *x = atoi(optarg);
        if (*x > 20)
	{
          printf("Inputed: %d is to big. (Limit 20). Reverting back to default 5.\n", *x);
          *x = temp;
        }
	validate(x,temp,'x');
      break;

      case 't':
	 temp = *z;
	 *z = atoi(optarg);
	 validate(z,temp,'z');
      break;

      case 'l':
        if (optopt == 'n')
        {
          printf("Please enter a valid filename.");
          return;
        }
        *filename = optarg;
      break;
      
      case '?':
        if (optopt == 'l')
        {
          printf("Command -l requires filename. Ex: -lfilename.txt | -l filename.txt.\n");
	  exit(0);
        }
        else if (optopt == 's')
        {
          printf("Commands -s requires int value. Ex: -s213| -s 2132\n");
	  exit(0);
        }
	else if (optopt == 'i')
	{
	  printf("Command -y requires int value. Ex: -i213| -i 2132\n");
	  exit(0);
	}
	else if (optopt == 't')
	{
	  printf("Command -z requires int value. Ex: -t13| -t 2132\n");
	  exit(0);	
	}
        else
        {
          printf("You have used an invalid command, please use -h or -help for command options, closing program.\n"); 
	  exit(0);
        }
      return;
	  
      default :
        if (optopt == 'l')
        {
          printf ("Please enter filename after -l \n");
          exit(0);
        }
	else if (optopt == 'n')
        { 
          printf ("Please enter integer x after -n \n");
		  exit(1);
        }
        printf("Running Program without Commands.\n");
      break;
    }
  }
}
void validate(int *x,int temp,char y)
{
  char *print;
  char *print2;
  if (y == 'z')
  {
    print = "z";
    print2 = "-t";	  
  }
  else if (y == 'x')
  {
    print = "x";
    print2 = "-s";	  
  }
  
  
  if (*x == 0)
  {
    printf("Intput invalid for %s changing %s back or default.\n",print2,print);
    *x = temp;
  }
  else if (*x < 0)
  {
    printf("Intput invalid for %s changing %s back or default.\n",print2,print);
    *x = temp; 
  }
}
void displayHelpMesg()
{
  printf ("---------------------------------------------------------------------------------------------------------\n");
  printf ("Please run oss or oss -arguemtns.\n");
  printf ("----------Arguments---------------------------------------------\n");
  printf (" -h or -help  : shows steps on how to use the program \n");
  printf (" -s x         : x is the maximum number of slave processes spawned (default 5) \n");
  printf (" -l filename  : change the log file name \n");
  printf (" -t z         : parameter z is the time in seconds when the master will terminate itself (default 20) \n"); 
  printf ("---------------------------------------------------------------------------------------------------------\n");
  printf ("\nClosing Program.............\n\n");
  exit(0);
}
void test (int x,int z, char *file)
{
  printf ("--------------------------------\n");
  printf ("Number of Slaves (x): %d\n", x);
  printf ("Time limit       (z): %d\n", z);
  printf ("Filename            : %s\n", file);
  printf ("--------------------------------\n\n");
  printf("Running Program.\n");
}
void savelog(char *filename, msgQueue userMsg, clock_share *sharedTime)
{
  FILE  *log = fopen(filename, "a");
  if (log == NULL)
  {
    perror ("File did not open: ");
    releaseMem();
    exit(1);
  }
  fprintf(log, "#%d Master(oss): %d is terminating(user) at %d.%010d because it reached %d.%010d\n", psNumber,userMsg.myPid, sharedTime->second, sharedTime->nanoSec, userMsg.death_Time.second, userMsg.death_Time.nanoSec);
 
   psNumber +=1;
  fclose(log);
}
void INThandler(int sig)
{ 
  signal(sig, SIG_IGN);
  printf("\nCtrl^C Called. Closing All Process.\n");
  fflush(stdout);
  releaseMem();
  int i =0;
  for (i=0; i<100;i++)
  {
    kill(pidArr[x], SIGQUIT); // killing em child by children
  }

  exit(0);
}
void on_alarm(int signal)
{
  printf("Out of time killing all slave processes.\n", z);
  releaseMem();
  int i = 0;
  for (i=0; i<100;i++)
  {
    kill(pidArr[x], SIGTERM); // killing em child by children
  }
    exit(0);
}
void releaseMem()
{
  if((shmctl(shareID, IPC_RMID, NULL)) == -1) //detach from shared memory
  {
    perror("Error in shmdt in Parent:");
  }
  if((msgctl(critMsgID, IPC_RMID, NULL)) == -1)//mark shared memory for deletion
  { 
    printf("Error in shmclt"); 
  }	
  if ((msgctl(dieMsgID, IPC_RMID, NULL) == -1))
  {
    perror("Erorr in msgctl ");
  }	
}
