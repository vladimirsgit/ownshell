#include <stdio.h>
#include <unistd.h>
#include <errno.h>	
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <limits.h>


#define MAX_ALLOWED_PROCESSES 20
#define MAX_ALLOWED_ARGS 10
#define MAX_TOKENS 300
#define MAX_LENGTH_OF_TOKENS 32

#define READFD 0
#define WRITEFD 1

#define STDIN 0
#define STDOUT 1

int tokenizeCommand(char *command, char tokens[MAX_TOKENS][MAX_LENGTH_OF_TOKENS]);
int createProcessesList(int noOfTokens, char tokens[MAX_TOKENS][MAX_LENGTH_OF_TOKENS], char *processes[MAX_ALLOWED_PROCESSES][MAX_ALLOWED_ARGS]);
void initializeProcessesPointerList(char *processes[MAX_ALLOWED_PROCESSES][MAX_ALLOWED_ARGS]);

void runOnlyOneProcess(char *processes[MAX_ALLOWED_PROCESSES][MAX_ALLOWED_ARGS]);
void runMultipleProcesses(char *process[MAX_ALLOWED_PROCESSES][MAX_ALLOWED_ARGS], int processesLength);

void manageChildProcesses(char *processes[MAX_ALLOWED_PROCESSES][MAX_ALLOWED_ARGS], int pipefd[][2], int i, int processesLength, int processNr);
void execvpAndCheck(char *processes[MAX_ALLOWED_PROCESSES][MAX_ALLOWED_ARGS], int i);

void closeFirstProcessFd(int pipefd[][2], int noOfNecessaryPipes);
void closeLastProcessFd(int pipefd[][2], int noOfNecessaryPipes);
void closeMiddleProcessFd(int pipefd[][2], int noOfNecessaryPipes, int processNr);

int main(int argc, char *argv[]){	
	printf("HI! Welcome to my shell implementation. **work in progress**\ncurrent features: -- all linux path commands -- -- creating pipelines --\n");
	while(1){
		char cwd[PATH_MAX];
		getcwd(cwd, sizeof(cwd));
		printf("\033[1;34m%s\033[0m$ ", cwd);
	
		char command[128];
		fgets(command, sizeof(command), stdin);
		command[strcspn(command, "\n")] = '\0';
		
		//if command is exit, it means we can exit directly
		if(strcmp(command, "exit") == 0){
			return 0;
		}
		if(strcmp(command, "") == 0){
			continue;
		}
		
		//if not, we create child process
		pid_t pid = fork();
		
		if(pid < 0){
			return errno;
		} else if(pid == 0){
			
			//here we manage child process
			//we split by spaces the command and find out the no of tokens
			char tokens[MAX_TOKENS][MAX_LENGTH_OF_TOKENS] = {0};
			int noOfTokens = tokenizeCommand(command, tokens);
			
			char *processes[MAX_ALLOWED_PROCESSES][MAX_ALLOWED_ARGS];
				
			int processesLength = createProcessesList(noOfTokens, tokens, processes);
			//the processesLength will be the number of programs + the number of operators between them, so we will have n - 1 operators
			//here we have an array processes of type: PROCESS p1: args p1 a1 a2 a3, PROCESS p2: args p2 a1 a2 a3
			//so for each process we have an array holding its data

			//this way of storing will come in handy when we want to do the actual piping like this: p[1] == "|" ==> pipe(p[0], p[2]);
			if(processesLength == 1){
				runOnlyOneProcess(processes);
			} else if(processesLength > 1){
				runMultipleProcesses(processes, processesLength);
			} else {
				exit(-1);
			}
			
		} else {
			wait(NULL);
		}
	}
	return 0;
}

int tokenizeCommand(char *command, char tokens[MAX_TOKENS][MAX_LENGTH_OF_TOKENS]){
	char *word = strtok(command, " ");
	int i = 0;
	//primeste de exemplu: 'ls | sort', pe urma tokens va avea tokens[0] = ls, tokens[1] = | , tokens[2] = sort
	while(word != NULL){
		if(strlen(word) > MAX_LENGTH_OF_TOKENS){
			printf("MAX LENGTH OF A TOKEN IS %d\n", MAX_LENGTH_OF_TOKENS);
			exit(0);
		}
		

		strcpy(tokens[i++], word);
		
		word = strtok(NULL, " ");

		if(i > MAX_TOKENS){
			printf("MAX NUMBER OF TOKENS IS %d\n", MAX_TOKENS);
			exit(0);
		}
	}	
	return i;
}

int createProcessesList(int noOfTokens, char tokens[MAX_TOKENS][MAX_LENGTH_OF_TOKENS], char *processes[MAX_ALLOWED_PROCESSES][MAX_ALLOWED_ARGS]){
	initializeProcessesPointerList(processes);
	int processesLength = 0;

	for(int i = 0; i < noOfTokens; i++){
		int args = 0;

		processes[processesLength][args] = tokens[i];

		while(++i < noOfTokens){
			if(strcmp(tokens[i], "|") == 0){
				processes[processesLength+1][0] = tokens[i];
				processesLength++;
				break;
			}
			if(args == MAX_ALLOWED_ARGS){
				printf("You have reached the upper limit for arguments for process \"%s\"\n", tokens[i - args - 1]);
				return -1;
			}
			processes[processesLength][++args] = tokens[i];
		}
		processes[processesLength++][args+1] = NULL;
	}	
	return processesLength;
}
void initializeProcessesPointerList(char *processes[MAX_ALLOWED_PROCESSES][MAX_ALLOWED_ARGS]){
	for(int i = 0; i < MAX_ALLOWED_PROCESSES; i++) {
		for(int j = 0; j < MAX_ALLOWED_ARGS; j++) {
			processes[i][j] = NULL;
		}		
	}
}



void runOnlyOneProcess(char *processes[MAX_ALLOWED_PROCESSES][MAX_ALLOWED_ARGS]){
	execvp(processes[0][0], processes[0]);
	perror("EXECVP FAILED");
	exit(-1);
}

void runMultipleProcesses(char *processes[MAX_ALLOWED_PROCESSES][MAX_ALLOWED_ARGS], int processesLength){
	if(processesLength % 2 == 0){
		printf("Not enough programs\n");
		exit(-1);
	}
	int noOfNecessaryPipes = processesLength / 2;
	int pipefd[noOfNecessaryPipes][2];

	for(int i = 0; i < noOfNecessaryPipes; i++){
		if(pipe(pipefd[i]) == -1){
			perror("ERROR WHEN TRYING TO PIPE");
			exit(errno);
		}
	}

	pid_t pids[noOfNecessaryPipes + 1];
	int noOfPids = 0;

	for(int i = 0, processNr = 0; i < processesLength; i+=2, processNr++){
		pid_t pid = fork();
		if(pid < 0){
			perror("FORK ERROR");
			exit(errno);
		} else if(pid == 0){
			manageChildProcesses(processes, pipefd, i, processesLength, processNr);
		} else {
			pids[noOfPids++] = pid;
		}
	}
	for(int j = 0; j < noOfNecessaryPipes; j++){
		if(close(pipefd[j][READFD]) == -1 || close(pipefd[j][WRITEFD]) == -1){
			perror("ERROR WHEN OPENING OR CLOSING AT THE END");
			exit(errno);
		}
	}
	for(int j = 0; j < noOfPids; j++){
		waitpid(pids[j], NULL, 0);
	}
}
void manageChildProcesses(char *processes[MAX_ALLOWED_PROCESSES][MAX_ALLOWED_ARGS], int pipefd[][2], int i, int processesLength, int processNr){
	int noOfNecessaryPipes = processesLength / 2;
	
	if(i == 0){
		//first proc
		closeFirstProcessFd(pipefd, noOfNecessaryPipes);
		//make it output to our write file descriptor
		dup2(pipefd[0][WRITEFD], STDOUT);
		execvpAndCheck(processes, i);
	} else if(i == processesLength - 1){
		//last proc
		closeLastProcessFd(pipefd, noOfNecessaryPipes);
		//make it input from the pipefd instead of STDIN
		dup2(pipefd[processNr - 1][READFD], STDIN);
		execvpAndCheck(processes, i);
	} else {
		//middle proc
		closeMiddleProcessFd(pipefd, noOfNecessaryPipes, processNr);

		dup2(pipefd[processNr - 1][READFD], STDIN);
		dup2(pipefd[processNr][WRITEFD], STDOUT);
		execvpAndCheck(processes, i);
	}
}
void execvpAndCheck(char *processes[MAX_ALLOWED_PROCESSES][MAX_ALLOWED_ARGS], int i){
	execvp(processes[i][0], processes[i]);
	perror("EXECVP FAILED");
	exit(-1);
}

void closeFirstProcessFd(int pipefd[][2], int noOfNecessaryPipes){
	//in the first command, we write only
	for(int j = 0; j < noOfNecessaryPipes; j++){
		//printf("WHEN I is 0, WE CLOSE FOR READING %d \n", j);
		if(close(pipefd[j][READFD]) == -1){
			perror("ERROR WHEN CLOSING");
			exit(errno);
		}
		//close the write for all of the other pipes
		if(j != 0){
			//printf("WHEN I is 0, WE CLOSE FOR WRITING %d \n", j);
			if(close(pipefd[j][WRITEFD]) == -1){
				perror("ERROR WHEN CLOSING");
				exit(errno);
			}
		}
	}
}
void closeLastProcessFd(int pipefd[][2], int noOfNecessaryPipes){
	// we only read
	for (int j = 0; j < noOfNecessaryPipes; j++){
		// printf("WHEN I is %d, WE CLOSE FOR WRITING %d\n", i, j);
		if (close(pipefd[j][WRITEFD]) == -1){
			perror("ERROR WHEN CLOSING");
			exit(errno);
		}
		// we close the write for all pipes and the read for all of the other pipes
		if (j != noOfNecessaryPipes - 1){
			// printf("WHEN I is %d, WE CLOSE FOR READING %d\n", i, j);
			if (close(pipefd[j][READFD]) == -1){
				perror("ERROR WHEN CLOSING");
				exit(errno);
			}
		}
	}
}
void closeMiddleProcessFd(int pipefd[][2], int noOfNecessaryPipes, int processNr){
//we both read and write, using pipefd, no STD
	//printf("WHEN I is %d WE READ FROM PIPE %d AND WRITE INTO PIPE %d\n", i, processNr - 1, processNr);
	for(int j = 0; j < noOfNecessaryPipes; j++){
		if(j != processNr - 1){
			//printf("WHEN I is %d, WE CLOSE FOR READING %d\n", i, j);
			if(close(pipefd[j][READFD]) == -1){
				perror("ERROR WHEN CLOSING");
				exit(errno);
			}
		}
		if(j != processNr){
			//printf("WHEN I is %d, WE CLOSE FOR WRITING %d\n", i, j);
			if(close(pipefd[j][WRITEFD]) == -1){
				perror("ERROR WHEN CLOSING");
				exit(errno);
			}
		}
	}	
}