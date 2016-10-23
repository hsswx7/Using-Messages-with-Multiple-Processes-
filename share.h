#ifndef SHARE_H
#define SHARE_H

typedef struct Clock
{
  int nanoSec; // nanoseconds will count up to 1000000000
  int second; 
}clock_share;

#define MSGSZ     500
typedef struct msgbuf //I am using the smae typedef from the website in README
{
  long    mtype;
  char    mtext[MSGSZ];
} message_buf;

typedef struct Queue
{
  char     stype;
  int      dieFlag;  // 0 will be sent if user has not reached death time
                 // 1 will be sent if the slave has eached death time and wishes to die
  pid_t    myPid;
  clock_share   death_Time;

} msgQueue;


#endif
