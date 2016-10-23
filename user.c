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
#include "share.h"


void INThandler(int sig); // for the ctrl^C
void TimeHandler(int sig); // for alarm (z)
void sigDie(int sig); //This is for the sigtstp so the user can send signal back to queue and die
int random_number(int min_num, int max_num);
void releaseMem(); //detaching for shared memory

clock_share *ossClock; // for the child to view the clock
message_buf critMsg; //critical message send and recieve
size_t buf_length; 

int shareID; // this id is for the shared time clock
int critMsgID; // critical queue
int dieMsgID; //death message is queue'd here


int main(int argc, char *argv[])
{
  signal(SIGTERM, TimeHandler); // getting is alarm is activated
  signal(SIGQUIT,INThandler); //getting if ctrl^c called
  signal(SIGINT, INThandler); 
  signal(SIGUSR1, sigDie); //catching death (stop signal) signal from the parent so the child 
  //this way the child dies and I can control the death by releasing memeory
  //and by sending criticla messages gack to the queue
  
  key_t key = atoi(argv[0]); // making key for access 
  key_t dieMsgKey = atoi(argv[1]); // Key for the messages
  key_t criticalMsgKey = atoi(argv[2]);  //Key for the children to have a critical section
  
  if((shareID = shmget(key, sizeof(ossClock),0666)) < 0)  //attaching to shared memeory 
  {
    perror("shmget in parent");
    exit(1);
  }
  ossClock = (clock_share*)shmat(shareID, (void *)0, 0);
 
  int startTime = ossClock->nanoSec;
  int life = 0;
  int checkLife = 0;
  if (startTime >= 80000000) // if about to go to 1 seconds lowring the wait time beacuse nanoSec will start from 0 agian
  {
	  life = random_number(1,1000000000);
	  checkLife = life+90000;
  }
  else
  {
	  life = random_number(startTime,10000000);
	  checkLife = life + startTime;
  }
  printf("User %d | Check Life %d\n", getpid(), checkLife);
  
  if ((critMsgID = msgget(criticalMsgKey, 0666)) < 0) //For mutual exlusion queue
  {
    perror("Error in msgeet Master ");
    exit(1);
  }
  
  if ((dieMsgID = msgget(dieMsgKey, 0666)) < 0) //user die queue
  {
    perror("Error in msgeet Master ");
    exit(1);
  }
  
  msgQueue usrMsg;
  while(1)
  {
    msgrcv(critMsgID, &critMsg, sizeof(critMsg), 0, 0); // waiting for the critical section
       
    if(ossClock->nanoSec >= checkLife) //if my time is up
    {
	  usrMsg.dieFlag = 1;
	  usrMsg.myPid = getpid();
	  usrMsg.death_Time.nanoSec = ossClock->nanoSec;
	  usrMsg.death_Time.second = ossClock->second;
	  // printf("User %d | Dying message		\n", getpid());
	  msgsnd(dieMsgID, &usrMsg, sizeof(usrMsg),0); // I send a message to the death queue
    }
    else
    {
    	 msgsnd(critMsgID, &critMsg, sizeof(critMsg), 0); //I send a message to the 
    }
  } 
  return 0;
}

void INThandler(int sig)
{ 
  signal(sig, SIG_IGN); // ignoring any signal passed to the INThandler
  fprintf(stderr, "\nCtrl^C Called, Process Exiting\n");
  releaseMem();
  msgsnd(critMsgID, &critMsg, sizeof(critMsg), 0);
  kill(getpid(), SIGKILL);
}
void TimeHandler(int sig)
{
  releaseMem();
  //shmctl(shmid, IPC_RMID, NULL); //mark shared memory for deletion
  fprintf(stderr, "\nOut of Time, Process %d Exiting\n", getpid());
  kill(getpid(), SIGKILL);
  //exit(0);
}
void sigDie(int sig)
{ 
  // printf("I'm Dying\n");	
  msgsnd(critMsgID, &critMsg, sizeof(critMsg), 0);
  releaseMem();
  exit(1);
}
int random_number(int min_num, int max_num)
{
  int result =0,low_num=0,hi_num=0;
  if(min_num < max_num)
  {
    low_num=min_num;
    hi_num=max_num+1;
  }
  else
  {
    low_num=max_num+1;
    hi_num=min_num;
  }
  srand(time(NULL) - getpid()*2);
  result = (rand()%(hi_num-low_num))+low_num;
  return result;
}
void releaseMem()
{
   if((shmdt(ossClock)) == -1) //detach from shared memory
   {
     // perror("Error in shmdt in Child:");
   }
}
