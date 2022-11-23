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
#include <stdbool.h>


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
int allocated[21];
int terminated;
int blocked;
};

//Struct for keeping track of the allocated and total available for each resource.
struct resources{
int release;
int requested;
int allocated;
int totalAvailable;
};

//Variables for queue
int intArray[18];
int front = 0;
int rear = -1;
int itemCount = 0;

static void myhandler(int s);

int safe();

bool isEmpty();

bool isFull();

int size();

void enqueue(int procIndex);

int dequeue();

int main (int argc, char** argv){

//Var for total number of processes created so that we can terminate at 40.
int totalProcesses;

int nextSecond = 0;

//Set 5 second timer
alarm(5);

//Handle the sigalarm
signal(SIGALRM, myhandler);
signal(SIGINT, myhandler);

//Set up file stuff

int fileLines = 0;

FILE *filePtr = fopen("output.txt", "w");

if(filePtr == NULL){
perror("file open error");
}

unsigned int increment = 2000; //Amount we will increment each loop iteration for now.

//Attach to the shared memory for processes

int shmid1 = shmget(PROCESS_SHMKEY, PROC_BUF, 0777 | IPC_CREAT);

if(shmid1 == -1){

	perror("in oss shmget for processes");
	exit(1);

}

struct process * procs = (struct process*)(shmat(shmid1, 0, 0));

//Default everything in the processes to be 0.
int i;
int k;
for(i = 0; i < 18; i++){
	procs[i].resRequest = 0;
	procs[i].terminated = 0;
	procs[i].blocked = 0;
	for(k = 1; k < 21; k++){
		procs[i].claims[k] = 0;
		procs[i].allocated[k] = 0;
	}
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

int nextProcessIndex = 0;

int previousBlocked = 50;

totalProcesses = 0;
//Loop looking for a requested resource.
while(1){

	//If we pass 1 billion nanoseconds then increment seconds and roll over nanoseconds.
	if(clock[1] >= 1000000000){
		clock[0] += 1;
		clock[1] = clock[1] - 1000000000;
	}
	//If we have reached 40 total processes terminate
	if(totalProcesses >= 40){
		myhandler(1);
		break;
	}

	//Every 2 seconds generate a new process.
if(clock[0] == nextSecond){
		
		//Count how many running processes are in the system
		int runningProcs = 0;
        	for(x = 0; x < 18; x++){
                	if(procs[x].terminated == 2){
                        	runningProcs++;
                	}
        	}
		

	//Only generate a new resource if there are less than 18 currently running.
	if(runningProcs < 18){
		if(fileLines < 10000){
			fprintf(filePtr, "Total processes = %d launching new process\n", totalProcesses);
			fileLines++;
		}	
		char charIndex[1];
		
		//If we have filled all 18 process spots then we cant rely on our nexProcessIndex any longer we need to find a finished process and take over its spot in shared memory.
		if(totalProcesses >= 18){	
			int x;
			for(x = 0; x < 18; x++){
				if(procs[x].terminated == 1){
					nextProcessIndex = x;
					break;
				}

			}

		}

		sprintf(charIndex, "%d", nextProcessIndex);

		pid_t childPid = fork();

		if(childPid == -1){
        	perror("failed to fork in oss");
        	exit(1);
		}

		if(childPid == 0){
        		char* args[] = {"./user", charIndex, 0};
        		execvp(args[0], args);
		}

		totalProcesses++;
		nextProcessIndex++;
		nextSecond++;
	}
}

	int posRes;
	//Loop through all the processes seeing if any have requested
	for(x = 0; x < 18; x++){
		//If there is a request handle it here		
		if(procs[x].resRequest != 30 && procs[x].resRequest > 0 && procs[x].blocked != 1 && procs[x].terminated != 1){
			
			if(fileLines < 10000){
				fprintf(filePtr,"Process %d has requested %d of resource %d\nTime of request %d seconds, %d nanoseconds\n", x, procs[x].claims[procs[x].resRequest] , procs[x].resRequest, clock[0], clock[1]);
				fileLines++;
			}

			//If our algorithm has determined we are in a safe state then carry out the allocation.
			if(safe()|| procs[x].claims[procs[x].resRequest] == 0){
				
				if(fileLines < 10000){
					fprintf(filePtr,"State is safe. Allocating resources.\n");
					fileLines++;
				}

				//Add the request to the allocation for the resource
				res[procs[x].resRequest].allocated += procs[x].claims[procs[x].resRequest];
				//Add the amount requested to the allocation for that resource in the process.
				procs[x].allocated[procs[x].resRequest] += procs[x].claims[procs[x].resRequest];
				//Set flag for user process.
				procs[x].resRequest = 30;			
			}
			else{ //Here we would be in an unsafe state, so we wont grant the request, and we will place the resource in the wait queue.
				
				if(fileLines < 10000){
					fprintf(filePtr,"Cannot grant %d of resource %d to process %d. Placing process in wait q\n", procs[x].claims[procs[x].resRequest], procs[x].resRequest, x);		
					fileLines++;
				}

				procs[x].blocked = 1;		
				enqueue(x);
			}
			
		}
		
		//If a processes has released a resource we want to handle it here. At the point of release of a resource, we would want to check to wait queue to see if there are any processes that could be fulfilled.
		if(procs[x].resRequest != 30 && procs[x].resRequest < 0){
			
			posRes = procs[x].resRequest * - 1;
			//Set this back to 0 so that this will not continue firing this if statement after releasing.
			procs[x].resRequest = 0;

			//Since a resource is being released, remove that amount from the allocation of that resource 
			res[posRes].allocated -= procs[x].claims[posRes];
			
			//Similarly we are releasing a resource so this would just be set to 0.
			procs[x].allocated[posRes] = 0;

			if(fileLines < 10000){
				fprintf(filePtr,"Resource %d being released by process %d\n", posRes, x);
				fileLines++;
			}

			int loop = 0;
			//Only do this when there is something in the queue
			if(size() > 0){
				while(loop <= size()){
					int tempIndex = dequeue(); //Index of a process placed in the wait queue.
			
				//If the request for the process can be granted by current availability then grant the request, if not place back in queue and continue loop		
					if(procs[tempIndex].claims[procs[tempIndex].resRequest] <= res[procs[tempIndex].resRequest].totalAvailable - res[procs[tempIndex].resRequest].allocated){ 			
			
						if(fileLines < 10000){
							fprintf(filePtr,"Process %d removed from queue, removing blocked flag.\n", tempIndex, procs[tempIndex].resRequest);
							fileLines++;
						}

						dequeue(tempIndex);
						procs[tempIndex].blocked = 0;
						break;

					}
					else{ //If this fires then there still isn't enonugh resources place that index back in queue and try again.
						enqueue(tempIndex);
					}
			
					loop++; //Do this process for each item in the queue. It will either be placed back in or removed and fulfilled.
					if(loop == size() && fileLines < 10000){
						fprintf(filePtr,"No processes in queue were able to be granted resources.\n");
						fileLines++;
						break;
					}
				}
			}	
		}
	}

	if(posRes == 20)
		break;

clock[1] += increment;
}

shmctl(shmid1, IPC_RMID, NULL);
shmctl(shmid2, IPC_RMID, NULL);
shmctl(shmid3, IPC_RMID, NULL);

if(fileLines < 10000){
	fprintf(filePtr,"Program will now terminate\n");
	fileLines++;
}

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



//Function for determining safe state

int safe (){

int currentAvailable[21];

int possible = 1;

int found = 0;

int x;


//Attach to shared memory for processes

int shmid1 = shmget(PROCESS_SHMKEY, PROC_BUF, 0777 | IPC_CREAT);

if(shmid1 == -1){

        perror("in oss shmget for processes\n");
        exit(1);

}

struct process * procs = (struct process*)(shmat(shmid1, 0, 0));

//Get temp claims array so that we can modify it and elminate processes without messing up actual
int tempClaims[18][21];

int j;
for(x = 0; x < 18; x++){
	for(j = 1; j < 21; j++){
		tempClaims[x][j] = procs[x].claims[j];
	}
}

//Attach to shared memory for resources

int shmid2 = shmget(RES_SHMKEY, RES_BUF, 0777 | IPC_CREAT);

if(shmid2 == -1){

        perror("error in oss shmget for resources\n");
        exit(1);


}

struct resources * res = (struct resources*)(shmat(shmid2, 0, 0));

//Now set up currentAvailable vector.

x;
for(x = 1; x < 21; x++){
currentAvailable[x] = res[x].totalAvailable - res[x].allocated;
}

while(possible){

//Find a process thats max claims could be satisfied by current availability of resources.

int flag = 0;
int i; 
int index;

for(x = 0; x < 18; x++){
	
	for(i = 1; i < 21; i++){
		
		if(tempClaims[x][i] - procs[x].allocated[i] <= currentAvailable[i]){
			flag++;
		}

	}
	//After the above for loop if we have found a process that can be satisfied by the current availability of each process then we set found and break the loop.
	if(flag == 20){
		found = 1;
		index = x;		
		break;
	}
}

//If we did find a process above that can be finished by current resource availability then we will simulate releasing its resources and set its values so that it can't possibly fulfill the requirements again 
	if(found){
		//We are setting the index above of which process is able to fulfill. Here we add its current allocation to our availability. Then just set its tempClaims super high so it wont work again 
		for(x = 1; x < 21; x++){
			currentAvailable[x] += procs[index].allocated[x];
			tempClaims[index][x] = 5000;

		}
		
		//If we found that every process including 17 can finish, and we havent set possible to 0 and broken the loop yet, we need this to break the loop if we are in a safe state. So break the loop 
		if(index == 17){
			break;
		}

	}
	else{		
		possible = 0;
	}


}//End while

//If the loop has been broken and possible was set to 0, that means no processes were found that could be completed with current resources. If it was broken if(index == 17) then possible will still be 1 and it means that its safe.
return possible; 
}

//Functions for queue

bool isEmpty(){
return itemCount == 0;
}

bool isFull(){
return itemCount == 18;
}

int size() {
return itemCount;
}

void enqueue(int procIndex){
	
	if(!isFull()){
		if(rear == 17){
			rear = -1;
		}
	
		intArray[++rear] = procIndex;
		itemCount++;
	}

}

int dequeue(){

int index = intArray[front++];

	if(front == 18){
		front = 0;
	}
	
	itemCount--;
	return index;
}






















