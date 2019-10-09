#include <stdio.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MAX_LEN 1024
#define TOKEN_SIZE 80
#define TOKEN_DELIM " \t\r\n\a"
#define MAX_LEN_COMMAND 5

/*
	Read user's command
	Return:
			char *userCommand: a pointer to an array containing the input
*/
char *readLine()
{
	char *userCommand = NULL;
	int bufSize = 0;

	getline(&userCommand, &bufSize, stdin);

	return userCommand;
}

/*
	Parse the command of user
	Argument:
				char *userCommand: a pointer points to stored user's command (returned value of readLine())
	Return:
				char **tokens: head pointer of an array containing pointers to tokens in user's input

*/
char **splitLine(char *userCommand)
{
	int bufSize = TOKEN_SIZE, pos = 0;
	char **tokens = (char**)malloc(bufSize * sizeof(char*));
	if(!tokens)
	{
		printf("Error in splitLine: Failed to allocate tokens.\n");
		return NULL;
	}

	char *token = NULL;
	token = strtok(userCommand, TOKEN_DELIM);
	while (token != NULL)
	{
		tokens[pos] = token;
		++pos;

		if (pos >= bufSize)
		{
			bufSize += TOKEN_SIZE;
			tokens = realloc(tokens, bufSize * sizeof(char*));
			if (!token)
			{
				printf("Error in splitLine: Failed to reallocate tokens.\n");
				return NULL;
			}
		}

		token = strtok(NULL, TOKEN_DELIM);
	}

	tokens[pos] = NULL;

	return tokens;
}

int launchProgram(char **args, int runBackground)
{
	signed int pid, wpid; // pid is process ID
	int status = 0;
	int startLoc;

	pid = fork();
	// The parent process will return 0 to its child one if the children takes the first
	// In other words, fork() will clone the parent program and the child process will
	// have pid = 0 (Both processes will run concurrently). The condition below checks if the process running is child
	if (pid == 0)
	{
		// The first arg is the name of the program and let the OS search for it
		// The latter arg is an array of string arguments

		// Tell the child process to ignore signals
		signal(SIGINT, SIG_IGN);
		signal(SIGQUIT, SIG_IGN);
		signal(SIGTSTP, SIG_IGN);
		signal(SIGTTIN, SIG_IGN);
		signal(SIGTTOU, SIG_IGN);
		signal(SIGCHLD, SIG_IGN);

		int status = execvp(args[0], args);
		if (status == -1)
		{
			perror("Invalid Command.\n");

			sleep(1);
			kill(getpid(), SIGTERM);

			exit(EXIT_FAILURE);
		}

		// In case it finished before the parent calls wait()
		sleep(.5);
		exit(EXIT_SUCCESS);
	}
	else if (pid < 0)
	{
		perror("forking Failed.\n");
		exit(EXIT_FAILURE);
	}
	else
	{
		// When a child process exits, the OS deallocates its resources yet leaves the
		// exit status remain in the PCB. PCB can only be deallocated if the parent proces
		// does wait on the child
		// The child process could 'die' before its parent waits sometimes
		if (runBackground == 0)
		{
			time_t t;
			// If the command doesn't end with &, the parent process will wait
			// waitpid waits for a process's state to change
			// Here, I wait until the parent or the child process exited or killed
			// If so, the function will return 1
			// More about waitpid: https://linux.die.net/man/3/waitpid, https://www.ibm.com/support/knowledgecenter/en/SSLTBW_2.2.0/com.ibm.zos.v2r2.bpxbd00/rtwaip.htm
			// https://linux.die.net/man/2/waitpid
			// Actually, returned value of waitpid will be stored in startLoc
			do
			{
				wpid = waitpid(pid, &startLoc, WNOHANG);
				if (wpid == -1) {
					perror("wait() failed");
				}
				else if (wpid == 0)
				{
					time(&t);
					//printf("Child process is still running at %s", ctime(&t));
					sleep(0.5);
				}
				else
				{
					if (WIFEXITED(startLoc)) {
						//printf("Child proces terminated with status of %d\n", WEXITSTATUS(startLoc));
					}
					else {
						//puts("Child process not exited successfully");
					}
				}
			} while(wpid == 0);
			/*
			WIFEXISTED returns non-zero value if the child process terminated normally with exit function
			WIFSIGNALED returns non-zero value if the child process terminated 'cause it received a signal which
						was not handled.
			More about signal: https://www.gnu.org/software/libc/manual/html_node/Signal-Handling.html#Signal-Handling
			*/
		}

		return 1;
	}
}

// Built-in commands for the shell
int help(char **args)
{
	int i;

	printf("\t-----A Shell implemented by Minh Chien and Thanh Dat-----\n");
	printf("Features supported by this shell:\n");
	printf("\t1. Type '&' at the end of your command to run concurrently.\n");
	printf("\t2. Type '!!' at the beginning of your command to execute the most current command.\n");
	printf("\t You'll may receive 'No command in history' response.\n");
	printf("\t3. Using '>' or '<' to redirect I/O.\n");
	printf("\t4. Using '|' to pipe.\n");

	return 1;
}

int quit(char **args) {
	return 0;
}

char *builtinCommands[] = {"help", "exit"};
int (*builtinFunctions[]) (char**) = {&help, &quit};
int numBuiltins() {
	return sizeof builtinCommands / sizeof(char *);
}

void outputRedirect(char *args[], char *outputFile, int runBackground)
{
	int fd, status;
	signed int pid, wpid;
	int startLoc;

	pid = fork();
	if (pid == 0)
	{
		// https://linux.die.net/man/3/open
		mode_t mode = S_IRUSR | S_IRGRP | S_IWGRP | S_IWUSR;
		fd = open(outputFile, O_CREAT | O_TRUNC | O_RDWR, mode);
		if (fd < 0)
		{
			perror("open file for output redirection failed.");
			kill(getpid(), SIGTERM);

			exit(EXIT_FAILURE);
		}
		else
		{
			dup2(fd, STDOUT_FILENO);
			close(fd);

			status = execvp(args[0], args);
			if (status == -1)
			{
				perror("Invalid command.\n");
				// A gentleman way to kill a process
				kill(getpid(), SIGTERM);

				sleep(.5);
				exit(EXIT_FAILURE);
			}

			sleep(.5);
			exit(EXIT_SUCCESS);
		}
	}

	if (runBackground == 0)
	{
		time_t t;
		do
		{
			wpid = waitpid(pid, &startLoc, WNOHANG);
			if (wpid == -1) {
				perror("wait() failed");
			}
			else if (wpid == 0)
			{
				time(&t);
				//printf("Child process is still running at %s", ctime(&t));
				sleep(.5);
			}
			else
			{
				if (WIFEXITED(startLoc)) {
					//printf("Child proces terminated with status of %d\n", WEXITSTATUS(startLoc));
				}
				else {
					//puts("Child process not exited successfully");
				}
			}
		} while(wpid == 0);
	}

	wait(NULL);
}

void inputRedirect(char *args[], char *inputFile, int runBackground)
{
	int fd, status;
	signed int pid, wpid;
	int startLoc;

	pid = fork();
	if (pid == 0)
	{
		fd = open(inputFile, O_RDWR, 0600);
		if (fd < 0)
		{
			printf("Failed to open file for input redirection.\n");
			kill(getpid(), SIGTERM);
		}
		else
		{
			dup2(fd, STDIN_FILENO);
			close(fd);

			status = execvp(args[0], args);
			if (status == -1)
			{
				perror("Invalid command.\n");
				// A gentleman way to kill a process
				kill(getpid(), SIGTERM);

				sleep(.5);
				exit(EXIT_FAILURE);
			}

			sleep(0.5);
			exit(EXIT_SUCCESS);
		}
	}

	if (runBackground == 0)
	{
		time_t t;
		do
		{
			wpid = waitpid(pid, &startLoc, WNOHANG);
			if (wpid == -1) {
				perror("wait() failed");
			}
			else if (wpid == 0)
			{
				time(&t);
				//printf("Child process is still running at %s", ctime(&t));
				sleep(.5);
			}
			else
			{
				if (WIFEXITED(startLoc)) {
					//printf("Child proces terminated with status of %d\n", WEXITSTATUS(startLoc));
				}
				else {
					puts("Child process not exited successfully");
				}
			}
		} while(wpid == 0);
	}

	wait(NULL);
}

int runDefaultUtils(char **args)
{
	for (int i = 0; i < numBuiltins(); i++)
	{
		if (strcmp(args[0], builtinCommands[i]) == 0) {
			return (*builtinFunctions[i])(args);
		}
	}

	return -1;
}

int executePipe(char **args, char **new_args, int i, int runBackground)
{
	if (args[i + 1] == NULL)
	{
		printf("Missed command after pipe.\n");
		return -1;
	}

	char *commandAfterPipe[MAX_LEN_COMMAND];
	int j = i + 1;
	int t = 0;
	for (; args[j] != NULL; j++)
	{
		commandAfterPipe[t] = args[j];
		t++;
	}
	commandAfterPipe[t] = NULL;

	signed int pid, wpid;
	int startLoc;

	pid = fork();
	if (pid == -1)
	{
		perror("fork failed.\n");
		kill(getpid(), SIGTERM);

		return -1;
	}

	if (pid == 0)
	{
		int pipeFds[2];

		int status = pipe(pipeFds);
		if (status == -1)
		{
			perror("pipe failed.\n");
			return -1;
		}

		signed int pId = fork();
		if (pId == -1) {
			return -1;
		}
		else if (pId == 0)
		{
			close(pipeFds[1]);
			dup2(pipeFds[0], 0);

			status = execvp(commandAfterPipe[0], commandAfterPipe);
			if (status == -1)
			{
				perror("Invalid Command After Pipe.\n");
				kill(getpid(), SIGTERM);

				return -1;
			}

			close(pipeFds[0]);
		}
		else
		{
			close(pipeFds[0]);
			dup2(pipeFds[1], 1);

			status = execvp(new_args[0], new_args);
			if (status == -1)
			{
				perror("Invalid Before After Pipe.\n");
				kill(getpid(), SIGTERM);

				return -1;
			}

			close(pipeFds[1]);

			exit(EXIT_SUCCESS);
		}
	}
	else
	{
		if (runBackground == 0)
		{
			time_t t;
			do
			{
				wpid = waitpid(pid, &startLoc, WNOHANG);
				if (wpid == -1) {
					perror("wait() failed");
				}
				else if (wpid == 0)
				{
					time(&t);
					//printf("Child process is still running at %s", ctime(&t));
					sleep(.5);
				}
				else
				{
					if (WIFEXITED(startLoc)) {
						//printf("Child proces terminated with status of %d\n", WEXITSTATUS(startLoc));
					}
					else {
						//puts("Child process not exited successfully");
					}
				}
			} while(wpid == 0);
		}

		wait(NULL);
	}

	return 1;
}

char *getLatestCommand(int latestCommandLen)
{
	int fd = open("history", O_CREAT | O_RDWR,
						S_IRUSR | S_IRGRP | S_IWGRP | S_IWUSR);
	char *buffer = (char*)malloc((latestCommandLen + 1) * sizeof(char));

	read(fd, buffer, latestCommandLen);
	buffer[latestCommandLen] = '\0';

	close(fd);

	return buffer;
}

int argslen(char ** args)
{
	int len = 0;
	for (int i = 0; args[i] != 0; i++) {
		len++;
	}

	return len;
}

int execute(char **args, int latestCommandExist, int latestCommandLen)
{
	int i = 0, runBackground = 0;

	int commandSize = 0;
	for(; args[i] != NULL; i++) {
		commandSize++;
	}

	// If a command entered is empty
	if (args[0] == NULL) {
		return 1;
	}

	// If user typed built-in commands
	int isBuiltinUtils = runDefaultUtils(args);
	if (isBuiltinUtils != -1) {
		return isBuiltinUtils;
	}

	// Otherwise
	char *new_args[MAX_LEN_COMMAND];
	for (i = 0; args[i] != NULL; i++)
	{
		if (strcmp(args[i], ">") == 0 || strcmp(args[i], "<") == 0 ||
			strcmp(args[i], "&") == 0 || strcmp(args[i], "|") == 0) {
					break;
		}

		new_args[i] = args[i];
	}
	new_args[i] = NULL;

	i = 0;
	while (args[i] != NULL && runBackground == 0)
	{
		if (strcmp(args[i], "!!") == 0)
		{
			if (latestCommandExist == 0)
			{
				printf("No commands in history\n");
				return -1;
			}
			else
			{
				// Echo the latest command on the shell's screen
				char *echoCommand = echoCommandToScreen(latestCommandLen);
				printf("%s\n", echoCommand);

				char **latestCommand = splitLine(echoCommand);

				execute(latestCommand, latestCommandExist, latestCommandLen);

				free(echoCommand);
				free(latestCommand)
			}
		}
		else if ((strcmp(args[i], "&") == 0) && (i == commandSize - 1)) {
			runBackground = 1;
		}
		else if (strcmp(args[i], ">") == 0)
		{
			if (args[i + 1] == NULL)
			{
				printf("Missed output source.\n");
				return -1;
			}
			else if ((i == 0) || (i > 0 && args[i - 1] == NULL))
			{
				printf("Missed action before output redirection.\n");
				return -1;
			}

			outputRedirect(new_args, args[i + 1], runBackground);
			return 1;
		}
		else if (strcmp(args[i], "<") == 0)
		{
			if ((i > 0 && args[i - 1] == NULL) || (i == 0))
			{
				printf("Missed action after input redirection.\n");
				return -1;
			}
			else if (args[i + 1] == NULL)
			{
				printf("Missed input source.\n");
				return -1;
			}


			inputRedirect(new_args, args[i + 1], runBackground);
			return 1;
		}
		else if (strcmp(args[i], "|") == 0)
		{
			int argsLen = argslen(args);
			if (strcmp(args[argsLen - 1], "&") == 0)
			{
				runBackground = 1;
				args[argsLen - 1] = NULL;
			}

			return executePipe(args, new_args, i, runBackground);
		}

		i++;
	}

	return launchProgram(new_args, runBackground);
}

void mainLoop()
{
	/*
	Three basic task of a shell:
		1. Read
		2. Parse
		3. Execute
	*/
	char *line;
	char **args;
	int shouldRun;

	int latestCommandExist = 0;
	int latestCommandLen = 0;
	int fd;
	fd = open("history", O_CREAT | O_RDWR,
						S_IRUSR | S_IRGRP | S_IWGRP | S_IWUSR);
	close(fd);

	do
	{
		puts("Type help if you need any helps.\n");
		puts("> ");

		line = readLine();
		latestCommandLen = strlen(line);
		if (strcmp(line[0], "\n") != 0) 
		{
			fd = open("history", O_CREAT | O_TRUNC | O_RDWR,
								S_IRUSR | S_IRGRP | S_IWGRP | S_IWUSR);
			write(fd, line, latestCommandLen);
			close(fd);

			latestCommandExist = 1;
		}

		args = splitLine(line);
		
		shouldRun = execute(args, latestCommandExist, latestCommandLen);

		free(line);
		free(args);

	} while(shouldRun);
}

int main(int argc, char **argv)
{
	// Load config files (optional)

	mainLoop();

	// Perform any shutdown/ cleanup

	return 0;
}
