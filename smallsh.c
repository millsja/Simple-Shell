/*************************************************************

Filename: smallsh.c
Author: James Mills
Date: Summer 2016


Overview:

	smallsh - minimal bash shell, which features:
	 * following built-in commands - these don't support
	   i/o redirection and do not have exit status
		- exit
			- exits shell
			- kills all processes and jobs first
		- cd
			- changes HOME env variablea
			- supports one argument for either
		- status
			- prints out exit status OR terminating
			  signal of last fg process
	 * command execution, with:
	 	- redirection of standard i/o
	 	- foreground / background processes
			- bg: last arg must be &
	 	- signal handling:
	 		- CTRL-C sends SIGINT to parent process and
		  children; foreground process handles its
		  own SIGINT i.e. parent doesn't kill fg
	 
	hints and tips (for me):
	 * fflush() after each and every output of text
	 * commands are/can be:
		- max 2048 characters
		- max 512 args
		- blank lines
		- comments starting with #
	 * consider using the following structure:
		- parent process (shell) continues running
		- non-built in commands cause fork
			- this child handles i/o then...
			- calls exec
	 * note: redirection symbols and dest/source are
			 NOT passed on to children i.e. you must
			 handle that yourself
	

	Without further ado...
	
*************************************************************/

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <wait.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>

/* constant values as described in assignment */
#define CMD_BUFFER_SIZE 2048
#define MAX_ARGS 512
#define MAX_FILENAME_SIZE 256
#define CMD_DELAY 1
#define CHILD_LIST_SIZE 25
#define BUFFER_SIZE 255

/* boolean definitions */
#ifndef bool
#define bool int
#define true 1
#define false 0
#endif


/* handles cd by user:
	- changes HOME env variablea
	- supports one argument for either */
int cmdCd(char* arg){
	/* if arg is null, then cd to home */
	if( !arg ){
		char* home = getenv("HOME");
		int error = chdir( home );
		if( error == 1 ){
			printf("Change directory failed...\n");
			fflush(stdout);
		}
	}

	/* otherwise, process argument */
	else{
		int error = chdir( arg );
		if( error == 1 ){
			printf("Change directory failed...\n");
			fflush(stdout);
		}
		
	}
}

/* handles user status check:
	- prints out exit status OR terminating
	  signal of last fg process */
int cmdStatus( int lastStatus ){
	printf("exit value %d\n", lastStatus);
	fflush(stdout);
}


/* takes a command from the user, parses and 
   reacts accordingly */
int cmdHandler( char* command, int* lastExit, char* fifo ){
	/* make a copy of our user input. not sure whether
	   this is necessary but since we're gonna carve it
	   up with strtok, it just seems like the kosher 
	   thing to do... */
	char cmdCopy[ CMD_BUFFER_SIZE ];
	strcpy( cmdCopy, command );
	
	/* check the first token for commands, comment, etc */
	char* chunk = strtok( cmdCopy, " \n");

	/* ignore if # */
	if( !chunk || chunk[0] == '#' ) return 0;
	
	/* exit */
	else if( strcmp( chunk, "exit" ) == 0 ){
		return -1;
	}

	/* change working directory */
	else if( strcmp( chunk, "cd" ) == 0 ){
		chunk = strtok( NULL, " \n");
		cmdCd( chunk );	
	}

	/* exit status of last command */
	else if( strcmp( chunk, "status" ) == 0 ){
		cmdStatus( *lastExit );	
	}

	/* otherwise try and execute */
	else{

		/* use strtok to pull the rest of our
		   arguments into an array */
		char* args[ MAX_ARGS ];
		int x;
		memset( args, '\0', MAX_ARGS * sizeof(char)); /* zero out array */
		char* stdOut = NULL;
		char* stdIn = NULL;
		bool runInBG = false;
		for(x = 0; chunk != NULL; ){

			/* check for and redirect standard input */
			if( strcmp(chunk, "<") == 0 ){
				/* pull out the next chunk. this will
				   be where stdin comes from */
				chunk = strtok( NULL, " \n" );	

				/* if chunk exists, then copy this to outDest */
				if( chunk != NULL ){
					/* malloc some space for the next arg based on
					   the size of this chunk of user input */
					stdIn = malloc( sizeof(char) * strlen( chunk ) );
					strcpy( stdIn, chunk );
				}
			}

			/* check for and redirect standard output */
			else if( strcmp(chunk, ">") == 0 ){
				/* pull out the next chunk. this will
				   be where stdout goes */
				chunk = strtok( NULL, " \n" );	
				
				/* if chunk exists, then copy this to outDest */
				if( chunk != NULL ){
					/* malloc some space for the next arg based on
					   the size of this chunk of user input */
					stdOut = malloc( sizeof(char) * strlen( chunk ) );
					strcpy( stdOut, chunk );
				}
			}

			/* otherwise business as usual */
			else{
				/* malloc some space for the next arg based on
				   the size of this chunk of user input */
				args[x] = malloc( sizeof(char) * strlen( chunk ) );

				/* copy the new chunk into our args array */
				strcpy( args[x], chunk);

				x++;

			}

			/* pull out the next chunk */
			if( chunk != NULL ){
				chunk = strtok( NULL, " \n" );	
			}
		}

		/* check for call to run new process in background */
		if( strcmp( args[x - 1], "&" ) == 0 ){
			runInBG = true;
			free( args[x - 1] );
			args[ x - 1 ] = 0;
			
		}

		/* start by forking */	
		int spawn = fork();
		int fd, fd2;
		int waitOption = 0;
		int exitStatus = 0; /* this will catch the child process's status */
		switch(spawn){
			case 0:
				/* check for stdio redirection and then redirect with
				   dup2() */
				if( stdOut ){
					fd = open( stdOut, O_WRONLY|O_CREAT|O_TRUNC, 0644 );
					if( fd == -1 ){
						perror("redirect error");
						exit(1);
					}	
					fd2 = dup2(fd, 1);
					if( fd2 == -1 ){
						perror("dup error");
						exit(1);	
					}	
				}
				if( stdIn ){
					fd = open( stdIn, O_RDONLY );
					if( fd == -1 ){
						perror("redirect error");
						exit(1);
					}	
					fd2 = dup2(fd, 0);
					if( fd2 == -1 ){
						perror("dup error");
						exit(1);	
					}	
				}

				/* if runInBG is set, then redirect output */
				if( runInBG == true ){
					if( stdOut == NULL ){
						fd = open( "/dev/null", O_WRONLY|O_CREAT|O_TRUNC, 0644 );
						if( fd == -1 ){
							perror("redirect error");
							exit(1);
						}	
						fd2 = dup2(fd, 1);
						if( fd2 == -1 ){
							perror("dup error");
							exit(1);	
						}	
					}
					if( stdIn == NULL ){
						fd = open( "/dev/null", O_RDONLY );
						if( fd == -1 ){
							perror("redirect error");
							exit(1);
						}	
						fd2 = dup2(fd, 0);
						if( fd2 == -1 ){
							perror("dup error");
							exit(1);	
						}	

					}
				}
				
				/* if we're running in fg mode, then reset
				   our signal handling so it can be killed */
				if( runInBG == false ) resetSigIgn();

				/* fork again so we can execute */
				spawn = fork();
				switch(spawn){

					case 0:
						/* attempt to execute command, otherwise
						   exit with exit code returned by exec */
						execvp( args[0], args );
						perror("exec error");	
						exit(EXIT_FAILURE);	
						break;

					default:
						if( runInBG == true){
							passPid( fifo, spawn);
						}	
						wait( &exitStatus );
						break;
				}

				/* process our errors */
				if( WIFEXITED( exitStatus ) ){
					exitStatus = WEXITSTATUS( exitStatus);
				}
				else if( WIFSIGNALED( exitStatus )){
					exitStatus = WTERMSIG( exitStatus );
				}

				/* pass errors on to parent process */
				exit( exitStatus );	

				break;
				
			default:

				/* if runInBG set, then set WNOHANG */
				if( runInBG == true ) waitOption = WNOHANG;
 
				/* wait for our child process and store exit status
				   in exitStatus */
				waitpid( spawn, &exitStatus, waitOption );

				/* check for error and pass to lastExit.
				   we need the status macros to process our
				   8-bit error code. otherwise -1 becomes
				   255...  */
				*lastExit = 0;
				if( WIFEXITED( exitStatus ) ){
					*lastExit = WEXITSTATUS( exitStatus );
				}


				break;	
		}

		/* clean up our args array */
		for(x = 0; args[x] != 0 ; x++){
			free(args[x]);
		}

		/* if this is a bg process, then we need
		   to send it to the process handler */
		if( runInBG == true){
			printf("new background process %d\n", spawn);
			fflush(stdout);
			return spawn;	
		}

	}

	return 0;
}


/* takes the filename of our fifo and a process id
   and writes that pid to the fifo, where it will be
   handles by the process handler. 

   note: pid is not necessary a process id, though it
   normally is. this is also a way to send commands to
   the process handler */
int passPid(char* fifo, pid_t pid){
	int len;
	char buffer[ BUFFER_SIZE ];
	memset(buffer, 0, BUFFER_SIZE);	

	/* open our fifo in wronly mode */
	int fd = open(fifo, O_WRONLY);

	/* handle any errors opening the fifo */
	if(fd == -1) perror("fifo error");

	/* turn our pid into a string */
	sprintf(buffer, "%d", pid);
	len = strlen(buffer);

	/* send to our process handler */
	write(fd, buffer, len);

	/* close up shop and go home */
	close( fd );
	return 0;

}


/* runs through the list of background children, checking to see
   whether any of them have finished */
int checkPidList( pid_t* childArray, int sizeOfChildList){
	int x, changeOfState, exitStatus;
	/* iterate through the list of processes */
	for(x = 0; x < sizeOfChildList; ){
		/* check that process's exit status */
		changeOfState = waitpid( childArray[x], &exitStatus, WNOHANG );

		/* if it exited, ... */
		if( changeOfState > 0 ){
			if( WIFEXITED( exitStatus ) ){
				/* then print that it did so and its status */
				printf("process %d exited with status %d\n",
					childArray[x], WEXITSTATUS(exitStatus));
				fflush(stdout);
			}
			else if( WIFSIGNALED( exitStatus )){
				printf("process %d exited with signal %d\n",
					childArray[x], WTERMSIG(exitStatus));
				fflush(stdout);

			}
			/* then delete it from the childArray */
			childArray[x] = childArray[ sizeOfChildList - 1 ];
			sizeOfChildList--;	
				
		}
		else{
			if( changeOfState == -1 ){
				perror("waitpid");
			}
			x++;
		}
	}

	return sizeOfChildList;
}


/* takes an array of processes and iterates through them, giving
   them one last chance to finish before trying an escalating series
   of signales. used in final cleanup.

   note: this doesn't modify the process list. so it's pretty
   much useless when the function returns. this is an end of program
   life function only */
int killPidList( pid_t* childArray, int sizeOfChildList){

	/* misc variables */
	int x, changeOfState, exitStatus;

	for(x = 0; x < sizeOfChildList; x++){
		/* check that process's exit status */
		changeOfState = waitpid( childArray[x], &exitStatus, WNOHANG );

		/* if it exited, ... */
		if( changeOfState > 0 ){
			/* then print that it did so and its status */
			printf("process %d exited with status %d\n",
				childArray[x], WEXITSTATUS(exitStatus));
			fflush(stdout);

		}	
		else{
			/* otherwise, if process still exists... */
			if(kill(childArray[x], 0) == 0){
				/* kill it gracefully */
				if(kill(childArray[x], SIGTERM) == 0){
					printf("%d killed by signal %d...\n", childArray[x], SIGTERM);
					fflush(stdout);
				}
				else if(kill(childArray[x], SIGKILL) == 0){
					printf("%d killed by %d...\n", childArray[x], SIGKILL);
					fflush(stdout);
				}
				else{
					printf("unable to kill %d...\n", childArray[x]);
					fflush(stdout);
				}
			}
		}
	}

}


/* takes the newest member of the process family and adds it to
   our child list, so we can kill it later */
int addPid(pid_t* childArray, int sizeOfChildList, pid_t pid){

	/* we're limiting the number of bg children tracked
	   to CHILD_LIST_SIZE */
	if( sizeOfChildList >= CHILD_LIST_SIZE ){
		printf("Unfortunately, you have reached the limit for ");
		fflush(stdout);
		printf("tracking new child processes. You can continue ");
		fflush(stdout);
		printf("call them. However, they will not be tracked till ");
		fflush(stdout);
		printf("others close.\n");
		fflush(stdout);
	}

	/* if we've got space, add it to the end of our 
	   list and increase list size */
	else{
		childArray[ sizeOfChildList ] = pid;
		sizeOfChildList++;
	}

	return sizeOfChildList;
}


int initShell(){
	/* first thing we do is fork into the shell and the
	   exec process handler, which will communicate using a
	   fifo */

   /* buffer for passing text */
    char buffer[255];

    /* create our fifo's filename */
    int pid  = getpid();
    char fifoFilename[255];
	char temp[255];

	if( getcwd(fifoFilename, 255) == 0) printf("failed...\n");	
    sprintf(temp, "/tmp_%d", pid);
	strcat(fifoFilename, temp);

    /* create our pipe */
    int fifo = mkfifo(fifoFilename, 0664);
    int fd = -1;

	/* set up our list of bg child processes - this will
	   be used for two different things based on the process.
	   in the parent process, it will be used to track bg 
	   processes. in the child process, it will track 
	   exec processes. */
	pid_t childArray[ CHILD_LIST_SIZE ];
	memset(childArray, 0, 25);	

	/* misc variables for our fifo */
    int x, len;
    int read_len;
	int incoming; 
	int sizeOfChildList = 0;
	int changeOfState;
	bool exitFlag = false;

	/* fork - this will create our main branch between 
	   the signal handler and our main shell/cmd functions */
    pid_t id = fork();
    switch( id ){
		/* child process - this guy will wait for input
		   to the fifo. this data will be one of two things
		   a number greater than zero if it is a process,
		   and then a negative number for a command (e.g.
		   -1 to kill all processes)

		   the purpose of this is to capture all of the 
		   process id numbers of the executed functions,
		   so that we can come back around and send them
		   all signals when the program terminates.  */
        case 0:
            fd = open(fifoFilename, O_RDONLY);
            if(fd == -1){
				printf("fifo error...");
				fflush(stdout);
			}
            while( exitFlag == false ){

				/* clear our buffer and then read */
                memset(buffer, 0, 255);
                read_len = read(fd, buffer, 254);

				/* something is in the buffer */
                if(read_len > 0){

					/* turn buffer into integer */
					incoming = atoi( buffer );

					/* if the incoming data is a pid */
					if( incoming > 0 ){
						sizeOfChildList = addPid(
							childArray, sizeOfChildList, incoming);
					}


					/* if -2, then make a pass over the list and kill
					   anything that hasn't finished running  */
					else if( incoming == -1 ){
						killPidList(childArray, sizeOfChildList);
						exitFlag = true;

					}
                }
            }

			/* close everything up and delete our fifo */
			close( fifo );
			char* arg[] = {"rm", "-f", fifoFilename, NULL};
			execvp(arg[0], arg);
	
			exit(0);
            break;

		/* the parent process - it will pass the filename 
		   of our fifo to any function calls, allowing
		   other functions to pass process ids and commands */ 
        default:
            break;
    }
 

	/* buffer for passing commands */
    char cmdBuffer[ CMD_BUFFER_SIZE ];

	
	/* prompt loop */
	int lastExit = 0;
	while( exitFlag == false ){
		
		/* check for terminated children */
		sizeOfChildList = checkPidList(childArray, sizeOfChildList);

		/* zero out the buffer */
		memset( cmdBuffer, '\0', CMD_BUFFER_SIZE * sizeof(char));

		/* prompt the user for input */
		printf(": ");
		fflush( stdout );
		fgets( cmdBuffer, CMD_BUFFER_SIZE, stdin );
		fflush( stdin );
	
		/* pass that input to command handler.
		   returns process id of any new background
		   process and -1 for exit flag */
		pid = cmdHandler( cmdBuffer, &lastExit, fifoFilename );
		if( pid == -1) exitFlag = true;
		else if( pid > 0 ){
			sizeOfChildList = addPid( childArray, sizeOfChildList, pid );	
		}
		


	}

	/* shell is quitting, so clean us up */
	passPid(fifoFilename, -1);
	

}


/* action for sig-able processes */
void normalAction(int signo){
	printf("%d terminated by signal %d\n", getpid(), signo);
	fflush(stdout);
}

/* resets signal handling to default */
int resetSigIgn(){
	struct sigaction action;
	action.sa_handler = normalAction;
	action.sa_flags = 0;
	sigaction(SIGINT, &action, NULL);	
}

/* set ups signal ignoring */
int initSigIgn(){
	struct sigaction action;
	action.sa_flags = 0;
	action.sa_handler = SIG_IGN;
	sigaction(SIGINT, &action, NULL);	
}


/* main function */
int main(){
	initSigIgn();
	initShell();
	return 0;
}
