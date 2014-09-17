#include <unistd.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <errno.h>
#include <sys/shm.h>
#include <fcntl.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <signal.h>
#define PERM (S_IRUSR | S_IWUSR)

int f_read; 
int f_write; //for file read and write
char *share_buf; //share buf
int *share_count;	//count for data in shared memory
struct timeval *share_time; // shared time interval
int childpid; // the pid needed for signaling
int end_write=0;
int end_read=0; // record the finish of file reading and writing for stop
long total_time=0;// the total time data contained


void producer(int signo){
	// producer wait for SIGUSR2, and load into memory
	*share_count=read(f_read, share_buf,1024); //load the first data into memory
	gettimeofday(share_time,NULL); //write time into the shared memory
	if(*share_count==0) end_read=1; //we have finished writing files
	kill(childpid,SIGUSR1); // send signal to consumer to process file
}

void cousumer(int signo){
	// cosumer wait for SIGUSR1, then write memory into out file
	struct timeval current;
	if(*share_count==0) end_write=1; //finishe wrting files
	else{
		gettimeofday(&current,NULL);
		total_time+=((current.tv_sec) * 1000000 + current.tv_usec)-((share_time->tv_sec * 1000000)+ share_time->tv_usec);
		//calculate the time
		write(f_write,share_buf,*share_count); // write count data into files 
		kill(getppid(),SIGUSR2); // send signal to producer to read files
	}
}


int detachandremove(int shmid, void *shmaddr) {
	//detach and remove the shared memory
	int error = 0;
	if (shmdt(shmaddr) == -1)
		error = errno;
	if ((shmctl(shmid, IPC_RMID, NULL) == -1) && !error)
		error = errno;
	if (!error)
		return 0;
	errno = error;
	return -1;
}

int main(int argc, char *argv[]) {
	int id_buf;
	int id_count; // shared memory id for buffer and count
	int id_time; // shared memory id for time
	signal(SIGUSR2,producer);
	signal(SIGUSR1,cousumer);
	// change SIGUSR to read and write file

	if(argc!=2){
		fprintf(stderr, "Usage: %s filename\n", argv[0]);
		return 1;
	}
	if ((id_buf= shmget(IPC_PRIVATE, sizeof(char)*1024, PERM)) == -1) {
		perror("Failed to create shared memory segment");
		return 1;
	}

	if ((id_count= shmget(IPC_PRIVATE, sizeof(int), PERM)) == -1) {
		perror("Failed to create shared memory count");
		return 1;
	}	//the two if above give the shared memory segment

	if ((id_time= shmget(IPC_PRIVATE, sizeof(struct timeval), PERM)) == -1) {
		perror("Failed to create shared memory for time");
		return 1;
	}	//the two if above give the shared memory segment

	if ((share_buf = (char *)shmat(id_buf, NULL, 0)) == (char *)-1) {
		perror("Failed to attach shared memory segment");
		if (shmctl(id_buf, IPC_RMID, NULL) == -1)
			perror("Failed to remove memory segment");
		return 1;
	}	//attatch the shared buffer to the process

	if ((share_count = (int *)shmat(id_count, NULL, 0)) == (int *)-1) {
		perror("Failed to attach shared memory segment");
		if (shmctl(id_count, IPC_RMID, NULL) == -1)
			perror("Failed to remove memory segment");
		return 1;
	}	//attatch the shared count to the process

	if ((share_time = (struct timeval *)shmat(id_time, NULL, 0)) == (struct timeval *)-1) {
		perror("Failed to attach shared memory segment");
		if (shmctl(id_time, IPC_RMID, NULL) == -1)
			perror("Failed to remove memory segment");
		return 1;
	}	//attatch the shared count to the process

	if ((childpid = fork()) == -1) {
		perror("Failed to create child process");
		detachandremove(id_count, share_count);
		detachandremove(id_buf,share_buf); //we need to detach if wrong 
		detachandremove(id_time,share_time);
		return 1;
	}

	if(childpid>0){ // parent process
		if((f_read=open(argv[1],O_RDONLY))==-1){
			perror("Failed to open input file");
			return 1;
		}//open read file
		sleep(1); // wait for child process to wait
		*share_count=read(f_read, share_buf,1024); //load the first data into memory
		gettimeofday(share_time,NULL); //write time into the shared memory
		if(*share_count==0) end_read=1; //we have finished writing files
		kill(childpid,SIGUSR1); // send signal to consumer to process file
		while(!end_read){
			pause();
		} //wait until finish reading file
		close(f_read);
		return 1;
	}

	else { // child process
		if((f_write=open("out",O_RDWR | O_CREAT))==-1){
			perror("Failed to open output file");
			return 1;
		}// open write file

		while(!end_write){
			pause();
		} // wait until finish writing file
		kill(getppid(),SIGUSR2); //send signal to stop parent process
		fprintf(stderr,"The total time for data in shared memory is %ld microseconds\n", total_time);
		detachandremove(id_count, share_count);
		detachandremove(id_buf,share_buf);
		detachandremove(id_time,share_time);
		close(f_write);
		return 1;
	} 
}