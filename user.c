#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/time.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/msg.h>
#include <string.h>

//Shared memory for clock
#define CLOCK_SHMKEY 511598

//Shared memory key for processes
#define PROCESS_SHMKEY 503689

//Shared mem key for resources
#define RES_SHMKEY 509237

//Buffer size for processes
#define PROC_BUF (sizeof(struct process) * 20)

//Buffer for resources
#define RES_BUF (sizeof(struct resources) * 21)

//Buffer for clock
#define CLOCK_BUF (sizeof(int) * 2)

//Struct for processes
struct process{
int resRequest;
int claims[21];
int allocated[21];
int terminated;
int blocked;
};

//Struct for resources
struct resources {
int release;
int requested;
int allocated;
int totalAvailable;
};

static void myhandler(int s);

int main (int argc, char** argv){

//Set which process this is
int position = atoi(argv[1]);

alarm(30);
signal(SIGALRM, myhandler);
signal(SIGINT, myhandler);

//Attach to shared memory for processes
int shmid1 = shmget(PROCESS_SHMKEY, PROC_BUF, 0777);

if(shmid1 == -1){
	perror("in user shmget for process");
	exit(1);
}

struct process * procs = (struct process *)(shmat(shmid1, 0, 0));

//Attach to shared mem for resources
int shmid2 = shmget(RES_SHMKEY, RES_BUF, 0777);

if(shmid2 == -1){
	perror("in user shmget for resources");
	exit(1);
}

struct resources * res = (struct resources*)(shmat(shmid2, 0, 0));

//Attach to shared mem for clock
int shmid3 = shmget(CLOCK_SHMKEY, CLOCK_BUF, 0777);

if(shmid3 == -1){
	perror("in user shmget for clock");
	exit(1);
}

char* paddr = (char*)(shmat(shmid3, 0, 0));
int* clock = (int*)(paddr);

//Generate random time interval in nanoseconds for user to wait for to request a resource
//Random number will be 1-500,000,000 ns range.

srand((unsigned) time(0));

int interval = (rand() % (250000000 - 2) + 1); 

//Generate max claims vector
int maxClaims [21];
int unchangedClaims[21];
int x;

//We will get a random nubmer from 0 - the totalAvailable and set it in our max claims array. This will be our max req. for each resource for this process.

for(x = 1; x < 21; x++){

	srand((unsigned) time(0));

	maxClaims[x] = (rand() % res[x].totalAvailable);

	unchangedClaims[x] = maxClaims[x];

	procs[position].claims[x] = maxClaims[x];

}


//Here set the process to terminated = 2 for running, will set to 1 when terminated.
procs[position].terminated = 2;

//Here in the loop we will request our resources from the maxClaims vector as long as our time has passed on the clock. We will request the resources in order according to the 
x = 0;
int doneFlag = 0;
int currentlyRequesting = 0;
int resRequested;
int nextSecond = clock[0];



while(1){

//Check to see if we need to terminate
	int x;

	for(x = 1; x < 21; x++){
		if(maxClaims[x] != -1){
			doneFlag = 1;
		}
	} 

	if(doneFlag == 0){
		break;
	}

	doneFlag = 0;
	
	//If we are waiting on a resource request we will hit this and check to see if its been granted each loop
	if(currentlyRequesting == 1){
		if(procs[position].resRequest == 30){ //If we grant resource in oss we set to 30
			maxClaims[resRequested] = -1;
			currentlyRequesting = 0;		
		}
	}



	//If we get to the interval on the clock then request a resource.

	if(clock[1] >= interval && currentlyRequesting == 0 && nextSecond == clock[0]){
		//Set what the next second interval will be so this doesn't fire on every pass that is greated than our interval
		nextSecond++;

		//If we have already been granted a resource then release it.
		if(procs[position].resRequest == 30){ 
			procs[position].resRequest = (resRequested * -1); //Release the resource
			continue;			
		}
		
		//Loop to request our next resource
		int j;

		for(j = 1; j < 21; j++){
			if(maxClaims[j] != -1){
			
				procs[position].resRequest = j; //Set our process to the resource we want		
				currentlyRequesting = 1; //Set flag to know we're expecting approval.
				resRequested = j;				
				break;
			}
		}	
	}//while loop
}//main


procs[position].terminated = 1;

shmctl(shmid1, IPC_RMID, NULL);
shmctl(shmid2, IPC_RMID, NULL);
shmctl(shmid3, IPC_RMID, NULL);
}


static void myhandler(int s){

int shmid2 = shmget(RES_SHMKEY, RES_BUF, 0777);
int shmid1 = shmget(PROCESS_SHMKEY, PROC_BUF, 0777);
int shmid3 = shmget(CLOCK_SHMKEY, CLOCK_BUF, 0777);

shmctl(shmid1, IPC_RMID, NULL);
shmctl(shmid2, IPC_RMID, NULL);
shmctl(shmid3, IPC_RMID, NULL);

exit(1);
}
