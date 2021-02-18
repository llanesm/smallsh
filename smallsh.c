/* Matthew Llanes
* Project 3 - smallsh (portfolio project)
* 2/1/2021
*/


#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <stdbool.h>
#include <signal.h>

#define LINELENGTH 2048
#define LINEARGS 512

bool blockBackground = false;	// changed with SIGTSTP, used with getCommandLine

// struct to hold data from command line
struct inputData {
	char* command;
	char* args[LINEARGS];
	char* inputFile;
	char* outputFile;
	bool backgroudProcess;
};


// prompts user for command, parses command, and tokenizes values, returns inputData struct
struct inputData* getCommandLine() {

	// retreive input from user
	char line[LINELENGTH];
	printf(": ");
	fflush(stdout);
	fgets(line, LINELENGTH, stdin);

	struct inputData* commandLineData = malloc(sizeof(struct inputData));	// to store data

	// check for newline
	for (int i = 0; i <= LINELENGTH; i++) {
		if (strcmp(&line[i], "\n") == 0) {	// if new line
			strcpy(&line[i], "");	// strcpy adds null char, so just use empty char as parameter
			break;
		}
	}

	// check for empty line or comment
	if (strcmp(line, "") == 0 || line[0] == 35) {	// 35 ASCII for #
		commandLineData->command = "#";
		return commandLineData;
	}

	// expand $$
	for (int i = 0; i <= LINELENGTH; i++) {
		if (strcmp(&line[i], "$$") == 0) {	// current char is $
			sprintf(&line[i], "%d", getpid());	// put current pid into the line to replace $$
		}
	}

	char* saveptr;	// parse input line and put data in commandLine struct

	// get command first
	char* token = strtok_r(line, " ", &saveptr);
	commandLineData->command = calloc(strlen(token) + 1, sizeof(char));
	strcpy(commandLineData->command, token);

	// set first argument as command for execvp later
	commandLineData->args[0] = calloc(strlen(token) + 1, sizeof(char));
	strcpy(commandLineData->args[0], token);

	token = strtok_r(NULL, " ", &saveptr);	// move pointer

	for (int i = 1; token; i++) {	// while there is a token
		
		// check for input file
		if (strcmp(token, "<") == 0) {
			token = strtok_r(NULL, " ", &saveptr);	// skip the >
			commandLineData->inputFile = calloc(strlen(token) + 1, sizeof(char));
			strcpy(commandLineData->inputFile, token);
		}
		// check for output file
		else if (strcmp(token, ">") == 0) {
			token = strtok_r(NULL, " ", &saveptr);
			commandLineData->outputFile = calloc(strlen(token) + 1, sizeof(char));
			strcpy(commandLineData->outputFile, token);
		}
		// check if background process
		else if (strcmp(token, "&") == 0 ) {
			commandLineData->backgroudProcess = true;
		}
		// add argument to struct's string array
		else {
			commandLineData->args[i] = calloc(strlen(token) + 1, sizeof(char));
			strcpy(commandLineData->args[i], token);
		}
		token = strtok_r(NULL, " ", &saveptr);
	}

	// to handle i/o redirection for background processes when files aren't specified
	if (commandLineData->backgroudProcess == true) {
		char* devNull = "/dev/null";	// default to redirect to
		if (!commandLineData->inputFile) {
			commandLineData->inputFile = calloc(strlen(devNull) + 1, sizeof(char));
			strcpy(commandLineData->inputFile, devNull);
		}
		if (!commandLineData->outputFile) {
			commandLineData->outputFile = calloc(strlen(devNull) + 1, sizeof(char));
			strcpy(commandLineData->outputFile, devNull);
		}
	}

	// if in foreground only mode, backgroundProcess for new commands must always be false
	if (blockBackground == true) {
		commandLineData->backgroudProcess = false;
	}

	return commandLineData;
}


// execute any command other than the built in commands
void execute(struct inputData* userData, int* status, int* backgroundProcesses, struct sigaction* sigint_action) {

	// fork a child
	pid_t spawnPid = fork();
	switch (spawnPid) {
	// fork error
	case -1:	// from Exploration: Process API
		perror("fork() failed!");
		exit(1);
		break;
	// child process
	case 0:
		// allowing ctrl-c to terminate foreground processes
		if (userData->backgroudProcess == false) {
			sigint_action->sa_handler = SIG_DFL;
			sigaction(SIGINT, sigint_action, NULL);
		}
		// handle input
		if (userData->inputFile) {
			// open input file, from Exploration: Processes and I/O
			int inputFD = open(userData->inputFile, O_RDONLY);
			if (inputFD == -1) {
				perror("cannot open input file");
				exit(1);
			}
			// redirect input file, from Exploration: Processes and I/O
			int result = dup2(inputFD, 0);
			if (result == -1) {
				perror("cannot redirect input file");
				exit(2);
			}
			fcntl(inputFD, F_SETFD, FD_CLOEXEC);	// close file, from Exploration: Processes and I/O
		}

		// handle output
		if (userData->outputFile) {
			// open output file, from Exploration: Processes and I/O
			int outputFD = open(userData->outputFile, O_WRONLY | O_CREAT | O_TRUNC, 0644);	//644 mentioned in piazza
			if (outputFD == -1) {
				perror("cannot open output file");
				exit(1);
			}
			// redirect output file, from Exploration: Processes and I/O
			int result = dup2(outputFD, 1);
			if (result == -1) {
				perror("cannot redirect output file");
				exit(2);
			}
			fcntl(outputFD, F_SETFD, FD_CLOEXEC);	// close file, from Exploration: Processes and I/O
		}

		// execute command
		execvp(userData->command, userData->args);
		perror("execvp error");	// from Exploration: Process API
		exit(2);
		break;

	// parent process
	default:
		// background process
		if (userData->backgroudProcess == true) {
			printf("background pid is %d\n", spawnPid);
			fflush(stdout);
			spawnPid = waitpid(spawnPid, status, WNOHANG);

			// add pid to backgroud pid list
			int i = 0;
			while (backgroundProcesses[i] != NULL) {	// loop until empty spot is found on the list
				i++;
			}
			// then insert new process pid into that spot in the array
			backgroundProcesses[i] = spawnPid;
		}
		// foreground process
		else {
			spawnPid = waitpid(spawnPid, status, 0);

			// print termination message if sent signal
			if (WTERMSIG(*status)) {
				printf("terminated by signal %d\n", WTERMSIG(*status));
				fflush(stdout);
			}

			// reset signal handler for SIGINT to ignore ctrl-c
			sigint_action->sa_handler = SIG_IGN;
			sigaction(SIGINT, sigint_action, NULL);
		}

		// check for terminated background processes
		while ((spawnPid = waitpid(-1, status, WNOHANG)) > 0) {
			// alert user process has finished, along with its exit value
			if (WIFEXITED(*status)) {
				printf("background pid %d is done: exit value %d\n", spawnPid, WEXITSTATUS(*status));
				fflush(stdout);
			}
			else {
				printf("background pid %d is done: terminated by signal %d\n", spawnPid, WTERMSIG(*status));
				fflush(stdout);
			}
			
			// remove pid from running processes list
			for (int i = 0; i < 200; i++) {
				if (backgroundProcesses[i] == spawnPid) {
					backgroundProcesses[i] = NULL;
				}
			}
		}
	}
}


// function to enter and exit foreground-only mode when user types ctrl-z
void handle_SIGTSTP(int signo) {
	// enter foreground mode
	if (blockBackground == false) {
		char* message = "Entering foreground-only mode (& is now ignored)\n";
		write(STDOUT_FILENO, message, 49);
		blockBackground = true;
	}
	else {
		char* message = "Exiting foreground-only mode\n";
		write(STDOUT_FILENO, message, 29);
		blockBackground = false;
	}
}


int main() {
	bool run = true;
	int status = 0;
	int runningChildren[200];

	// signal handlers, some code from Exploration: Signal Handling API
	//SIGINT
	struct sigaction SIGINT_action = { 0 };
	SIGINT_action.sa_handler = SIG_IGN;	// default to ignore ctrl-c
	sigfillset(&SIGINT_action.sa_mask);	// block other signals
	SIGINT_action.sa_flags = 0;	// no flags
	sigaction(SIGINT, &SIGINT_action, NULL);

	//SIGTSTP
	struct sigaction SIGTSTP_action = { 0 };
	SIGTSTP_action.sa_handler = handle_SIGTSTP;	// set handler function
	sigfillset(&SIGTSTP_action.sa_mask);	// block other signals
	SIGTSTP_action.sa_flags = 0;	// no flags
	sigaction(SIGTSTP, &SIGTSTP_action, NULL);


	while (run == true) {

		// get user data from command line
		struct inputData* userData = getCommandLine(blockBackground);

		// check for blank line or comment
		if (strcmp(userData->command, "#") == 0) {
			continue;
		}
		// exit - built in command
		else if (strcmp(userData->command, "exit") == 0) {
			run = false;
			for (int i = 0; i < 200; i++) {	// iterate through running process list
				if (runningChildren[i]) {
					kill(runningChildren[i], SIGTERM);	// kill child process
					runningChildren[i] = waitpid(runningChildren[i], &status, WNOHANG);	// collect return status
				}
			}
		}
		// cd - built in command
		else if (strcmp(userData->command, "cd") == 0) {
			if (userData->args[1] != NULL) {	// checks if user included an argument
				chdir(userData->args[1]);	// if so, changes directory to that one
			}
			else {
				chdir(getenv("HOME"));	// if not, goes to home directory
			}
		}
		// status - built in command
		else if (strcmp(userData->command, "status") == 0) {
			if (WIFEXITED(status)) {	// if normal exit
				printf("exit value %d\n", WEXITSTATUS(status));
				fflush(stdout);
			}
			else {	// if abnormal signal termination
				printf("terminated by signal %d\n", WTERMSIG(status));
				fflush(stdout);
			}
		}
		// execute any other (not built in) command
		else {
			execute(userData, &status, runningChildren, &SIGINT_action);
		}
	}

	return 0;
}
