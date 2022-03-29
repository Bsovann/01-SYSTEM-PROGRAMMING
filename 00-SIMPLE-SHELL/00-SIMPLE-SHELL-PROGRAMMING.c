#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200809L
#define NUM_CMD_HISTORY 101
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>

// Global Variable
int MAX_ARGS = 400;
int countCmd = 0;

// Function Prototypes
char *arrOfCmdHistory[NUM_CMD_HISTORY];
char *userInput();
void err_exit(char *msg);
void clearArgs(char **args);
void addHistory(char *cmdInput);
void parseSpace(char *stringInput, char **parsedArgs);
void history(char *args);
void execArgs(char **parsedArgs);
void execArgsWithPipe(char **parsedArgsPipe);
void createProcess(int inFd, int outFd, char **myargs);
void commandExecution(char* input);
int stringProcessHandling(char *strInput, char **parsedArgs, char **parsedArgsPipe);
int parsePipe(char *stringInput, char **strPipe);
int builtInCommands(char **args);
int length(char **arrOfString);
/*
	Algorithm
	1. Get string from the user
	2. stringProcess
		- if the string include "|", process "execArgsWithPipe"
		- else process "execArgs"
*/
int main(int argc, char *argv[])
{
	
	char *stringInput = NULL;
	while (1)
	{
		stringInput = userInput();
		if (strcmp(stringInput, "\0") == 0)
			continue;
		commandExecution(stringInput);
	}
}
// Command Execution process
void commandExecution(char* input){

	char *myargs[MAX_ARGS];
	char *myargsPiped[MAX_ARGS];

	int flag = stringProcessHandling(input, myargs, myargsPiped);

		if (flag == 1){
			if(builtInCommands(myargs) != 0)
				return;
			else
				execArgs(myargs);
		}

		if (flag == 2)
			execArgsWithPipe(myargsPiped);
}

// Get user input (establish terminal) as sish> and check for error
char *userInput()
{
	size_t len = NULL;
	ssize_t read = NULL;
	char *input = NULL;

	printf("sish> ");

	read = getline(&input, &len, stdin);
	if (read == -1)
		err_exit("Error at input");

	if (len >= MAX_ARGS)
		MAX_ARGS *= 2;
	
	if (len == 0)
			return input; 

	input[strlen(input) - 1] = '\0';
	// Adds user input commands to history
	addHistory(input);


	return input;
}

// String process to check whether the argument has a pipe or not
int stringProcessHandling(char *strInput, char **parsedArgs, char **parsedArgsPipe)
{
	int pipeFlag = parsePipe(strInput, parsedArgsPipe);
	if (pipeFlag == 0)
	{
		parseSpace(strInput, parsedArgs);
		return 1;
	}
	else
		return pipeFlag + 1;
}
// Process for tokenizing the string by space
void parseSpace(char *stringInput, char **parsedArgs)
{

	char *strTemp;
	char *command = NULL;
	int i;

	for (i = 0, strTemp = stringInput;; i++, strTemp = NULL)
	{
		command = strtok(strTemp, " ");
		if (command == NULL)
		{
			parsedArgs[i] = NULL;
			break;
		}
		parsedArgs[i] = command;
	}
}

// Tokenize the string by pipe symbol "|""
int parsePipe(char *stringInput, char **strPipe)
{
	char *strTemp;
	char *command = NULL;
	int i;

	for (i = 0, strTemp = stringInput;; i++, strTemp = NULL)
	{
		command = strtok(strTemp, "|");
		if (command == NULL)
		{
			strPipe[i] = NULL;
			break;
		}
		strPipe[i] = command;
	} 
		return (strPipe[1] != NULL);
}

// Execute the user input commands without pipes
void execArgs(char **parsedArgs)
{
	int cpid;
	cpid = fork();

	if (cpid < 0)
		err_exit("child proccess failed in \"execArgs\"");

	if (cpid == 0)
	{
		execvp(parsedArgs[0], parsedArgs);
		err_exit("Command line cannot be found ");
	}
	else
	{
		waitpid(cpid, NULL, 0);
		clearArgs(parsedArgs);
	}
}
// Execute commands involving a pipe
// Send output of the previous command to be read by the next pipe
void execArgsWithPipe(char **parsedArgsPipe)
{
	int i, n;
	int len = length(parsedArgsPipe);
	char *temp[MAX_ARGS];
	int fd[2]; 
	

	// Pipe = (N of cmd) - 1;
	n = STDIN_FILENO;
	for (i = 0; i < len - 1; i++)
	{
		parseSpace(parsedArgsPipe[i], temp);
		// create a pipe
		if (pipe2(fd, O_CLOEXEC) < 0)
			err_exit("Pipe error");
		// create a new process
		createProcess(n, fd[1], temp);

		// close write-end fd of current pipe
		close(fd[1]); 
		// save read-end fd of current pipe
		n = fd[0]; 
	}

	// Execute the last command
	if (n != STDIN_FILENO)
	{
		pid_t lastChild = fork();

		if (lastChild < 0)
			err_exit("child process faild in the last command");
		// Child process
		else if (lastChild == 0)
		{
			if (dup2(n, STDIN_FILENO) == -1)
				err_exit("dup2 failed in the last command");
			close(n);
			parseSpace(parsedArgsPipe[i], temp);
			execvp(temp[0], temp);
			err_exit("cmd line cannot be found in the last command");
		}
		// Parent process
		close(fd[0]);
		close(fd[1]);
		waitpid(lastChild, NULL, 0);
		clearArgs(parsedArgsPipe);
	}
}

// Create a new process
void createProcess(int inFd, int outFd, char **myargs)
{

	pid_t cpid;

	cpid = fork();

	if (cpid < 0)
		err_exit("child process faild");
	// Child process
	else if (cpid == 0)
	{

		if (inFd != STDIN_FILENO)
		{
			if (dup2(inFd, STDIN_FILENO) == -1)
				err_exit("dup2 failed in child process");
			close(inFd);
		}

		if (outFd != STDOUT_FILENO)
		{
			if (dup2(outFd, STDOUT_FILENO) == -1)
				err_exit("dup2 failed in child process");
			close(outFd);
		}
		execvp(myargs[0], myargs);
		err_exit("execvp failed in child process");
	}
	// Parent process
	waitpid(cpid, NULL, 0);
}

// Built in commands
int builtInCommands(char **args)
{
	int numOfCommands = 3;
	int switchArgs = 0;
	int i;
	char *command[numOfCommands];

	// Assigns the commands
	command[0] = "exit";
	command[1] = "cd";
	command[2] = "history";

	for (i = 0; i < numOfCommands; i++)
	{
		if (strcmp(args[0], command[i]) == 0)
		{
			switchArgs = i + 1;
			break;
		}
	}
	// Switch case for exit, cd, and history
	switch (switchArgs)
	{
	case 1:
		printf("Shell successfully terminated. Bye Bye!\n");
		exit(0);
	case 2:
		if (args[1] == NULL)
		{
			printf("Error. No argument for \"cd\"\n");
			return 0;
		}
		else
		{
			chdir(args[1]);
			return 1;
		}
	case 3:
		history(args[1]);
		return 1;
	default:
		break;
	}

	return 0;
}

// "History" function
void history(char *args)
{

	int i;
	// History without arguments
	if (args == NULL)
	{
		for (i = 0; i < length(arrOfCmdHistory); i++)
			printf("%d %s\n", i, arrOfCmdHistory[i]);
	}
	// History -c
	else if (strcmp(args, "-c") == 0)
	{
		clearArgs(arrOfCmdHistory);
		countCmd = 0;
	}
	// History offset
	else
	{
		int x = atoi(args);
		if (x >= countCmd || x < 0)
		{
			printf("Invalid Offet! Not Exist!\n");
			return;
		}
		else
			commandExecution(arrOfCmdHistory[x]);
	}
}
// Add an input command to an array
void addHistory(char *cmdInput)
{
	if (countCmd < NUM_CMD_HISTORY)
	{
		arrOfCmdHistory[countCmd++] = strdup(cmdInput);
	}
	else
	{
		for (int i = 0; i < countCmd - 1; i++)
			arrOfCmdHistory[i] = arrOfCmdHistory[i + 1];
		arrOfCmdHistory[countCmd - 1] = strdup(cmdInput);
	}
}

// Helper method (exit the program with message)
void err_exit(char *msg)
{
	perror(msg);
	exit(EXIT_FAILURE);
}
// Helper method (clear array of string)
void clearArgs(char **args)
{
	int len = length(args);

	for (int i = 0; i < len; i++)
		args[i] = NULL;
}

// Helper method (find length of the array of string)
int length(char **arrOfString)
{
	int len = 0;
	while (arrOfString[len] != NULL)
		len++;
	return len;
}
