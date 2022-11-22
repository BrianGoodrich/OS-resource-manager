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

//Buffer size for our 18 processes, 0-17 will be for our 18 potential processes.
#define PROC_BUF (sizeof(struct process) * 20)

//Buffer size for resources
#define RES_BUF (sizeof(struct resources) * 21)

//Buffer size for clock
#define CLOCK_BUF (sizeof(int) * 2)

//Struct for processes requests and their max claims.
struct process{
int resRequest;
int claims[21];
};

//Struct for keeping track of the allocated and total available for each resource.
struct resources{
int release;
int requested;
int allocated;
int totalAvailable;
};

//Struct for determining safe state
struct state{
int available[18];
int alloc[20][1];
};

static void myhandler(int s);

int main (int argc, char** argv){

int waitQ [18];

//Set 5 second timer

alarm(5);

//Handle the sigalarm

signal(SIGALRM, myhandler);
signal(SIGINT, myhandler);

unsigned int increment = 10000000; //Amount we will increment each loop iteration for now.

//Attach to the shared memory for processes

int shmid1 = shmget(PROCESS_SHMKEY, PROC_BUF, 0777 | IPC_CREAT);

if(shmid1 == -1){

	perror("in oss shmget for processes");
	exit(1);

}

struct process * procs = (struct process*)(shmat(shmid1, 0, 0));

int i;
for(i = 0; i < 18; i++){
	procs[i].resRequest = 0;
}

//Attach to shared memory for resources

int shmid2 = shmget(RES_SHMKEY, RES_BUF, 0777 | IPC_CREAT);

if(shmid2 == -1){

	perror("error in oss shmget for resources");
	exit(1);


}

struct resources * res = (struct resources*)(shmat(shmid2, 0, 0));

//Attach to shared memory for clock

int shmid3 = shmget(CLOCK_SHMKEY, CLOCK_BUF, 0777 | IPC_CREAT);

if(shmid3 == -1){
	perror("error in oss shmget for clock");
	exit(1);
}

char* paddr = (char*)(shmat(shmid3, 0, 0));
int* clock = (int*)(paddr);

clock[0] = 0;
clock[1] = 1;


//Now generate values 0-9 for populating our 20 resources.

int x;

for(x = 1; x < 21; x++){

res[x].totalAvailable = (rand() % (10-2) + 1); //Generate random number 1-10 for our resource

}

pid_t childPid = fork();

if(childPid == -1){
	perror("failed to fork in oss");
	exit(1);
}

if(childPid == 0){
	char* args[] = {"./user", "0", 0};
	execvp(args[0], args);
}

//Loop looking for a requested resource.
while(1){

	//If we pass 1 billion nanoseconds then increment seconds and roll over nanoseconds.
	if(clock[1] >= 1000000000){
		clock[0] += 1;
		clock[1] = clock[1] - 1000000000;
	}

	int posRes;
	//Loop through all the processes seeing if any have requested
	for(x = 0; x < 18; x++){
		//If there is a request handle it here		
		if(procs[x].resRequest != 30 && procs[x].resRequest > 0){
			printf("Process %d has requested resource %d\nTime of request %d seconds, %d nanoseconds\n", x, procs[x], clock[0], clock[1]);
			
			//If request + allocation > totalAvailable deny request
			if(res[procs[x].resRequest].allocated + res[procs[x].resRequest].requested > res[procs[x].resRequest].totalAvailable)
			{
				waitQ[x] = procs[x].resRequest; 	
				printf("Allocation not possible process waiting\n");
					
			}
			
			procs[x].resRequest = 30; //Set this flag to say we granted the request.
		}
		
		//If a processes has released a resource we want to handle it here
		if(procs[x].resRequest != 30 && procs[x].resRequest < 0){
			posRes = procs[x].resRequest * -1;
			res[posRes].allocated -= res[posRes].release;
			
		}
	}

	if(posRes == 20)
		break;

clock[1] += increment;
}

shmctl(shmid1, IPC_RMID, NULL);
shmctl(shmid2, IPC_RMID, NULL);
shmctl(shmid3, IPC_RMID, NULL);

printf("Program will now terminate\n");


}

static void myhandler(int s){

	int shmid1 = shmget(PROCESS_SHMKEY, PROC_BUF, 0777);
	int shmid2 = shmget(RES_SHMKEY, RES_BUF, 0777);
	int shmid3 = shmget(CLOCK_SHMKEY, CLOCK_BUF, 0777);
	
	shmctl(shmid1, IPC_RMID, NULL);
	shmctl(shmid2, IPC_RMID, NULL);
	shmctl(shmid3, IPC_RMID, NULL);

	printf("Program will now terminate from handler.\n");	
	
	exit(1);
}

































