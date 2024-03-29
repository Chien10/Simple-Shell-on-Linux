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
	Compute length of a 2 pointer array
	Arguments:
				char **args: a 2 pointer array
	Return:
				int len: the lenght of the array
*/
int argslen(char ** args)
{
	int len = 0;
	for (int i = 0; args[i] != 0; i++) {
		len++;
	}

	return len;
}

/*
	Read user's command from keyboard
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
	Parse the command of user to eliminate redundant characters
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

/*
	Call system functions to execute input command
	Arguments:
				char **args: return value of splitLine()
				int runBackground: indicates whether the child process running in background
	Return:
				1
*/
int launchProgram(char **args, int runBackground)
{
	signed int pid, wpid; // pid is process ID
	int status = 0;
	int startLoc;
	//printf("Enter launchProgram with: %s\n", args[0]);
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
			} while (wpid == 0);
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

/*
	Print utilities of this shell
*/
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

/*
	Exit the shell
*/
int quit(char **args) {
	return 0;
}

char *builtinCommands[] = {"help", "exit"};
int (*builtinFunctions[]) (char**) = {&help, &quit};

/*
	Return the number of built-in functions
*/
int numBuiltins() {
	return sizeof builtinCommands / sizeof(char *);
}

/*
	Perform output redirection
	Arguments:
				char *args[]: the part before '>'
				char *outputFile: the name of the file 
				int runBackground: indication of running in background or foreground
*/
void outputRedirect(char *args[], char *outputFile, int runBackground)
{
	int fd, status;
	signed int pid, wpid;
	int startLoc;

	/*printf("Command before output redirection: ");
	for(int t=0; args[t]!=NULL; t++) {
		printf("%s", args[t]);
	}
	printf("\n");

	printf("File to open: ");
	printf("%s\n", outputFile);*/

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

			char cwd[40];
			if (getcwd(cwd, sizeof cwd) != NULL) {
				printf("Current working directory of process: %s", cwd);
			}

			status = execvp(args[0], args);
			if (status == -1)
			{
				perror("Invalid command before output redirection.\n");
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
		} while (wpid == 0);
	}

	wait(NULL);
}

/*
	Perform output redirection
	Arguments:
				char *args[]: the characters before '<'
				char *outputFile: the name of the file 
				int runBackground: indication of child process running in background or foreground
*/
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

/*
	Execute built-in functions
*/
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

/*
	Execute command when pipe is introduced
	Arguments:
				char **args: value returned from splitLine()
				char **new_args: characters before '|'
				int i: position of '|' in value returned from splitLine()
				int runBackground: child process running background (or not)
*/
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

/*
	Read latest command saved in 'history' file
	Argument:
				int latestCommandLen: length of the saved command (without preprocessing)
*/
char *getLatestCommand(int latestCommandLen)
{
	char *buffer = (char*)malloc(latestCommandLen * sizeof(char));
	int fd = open("history", O_RDWR,
						S_IRUSR | S_IRGRP | S_IWGRP | S_IWUSR);
	read(fd, buffer, latestCommandLen);
	close(fd);
	//printf("Command read from file: %s", buffer);
	//printf(", with length of: %d", strlen(buffer));

	return buffer;
}

/*
	Main function that execute command from keyboard
	Arguments:
				char **args: value returned by splitLine()
				int latestCommandExist: whether lastest command exists
				int latestCommandLen: length of the latest command
*/
int execute(char **args, int latestCommandExist, int latestCommandLen)
{
	int i = 0, runBackground = 0;
	int argsLen = 0;

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
	char *new_args[MAX_LEN_COMMAND * 2];
	//printf("new_args: ");
	for (i = 0; args[i] != NULL; i++)
	{
		if (strcmp(args[i], ">") == 0 || strcmp(args[i], "<") == 0 ||
			strcmp(args[i], "&") == 0 || strcmp(args[i], "|") == 0) {
					break;
		}

		new_args[i] = args[i];
		//printf("%s", new_args[i]);
	}
	new_args[i] = NULL;
	//printf("\n");

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
				char *echoCommand = getLatestCommand(latestCommandLen);
				printf("%s\n", echoCommand);

				char **latestCommand = splitLine(echoCommand);
				/*printf("After slitting: ");
				for(int t=0; latestCommand[t] != NULL; t++) {
					printf("latestCommand[%d]: %s", t, latestCommand[t]);
					printf(" ");
				}
				printf("\n");*/

				execute(latestCommand, latestCommandExist, latestCommandLen);

				free(echoCommand);
				free(latestCommand);

				return 1;
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

			argsLen = argslen(args);
			if (strcmp(args[argsLen - 1], "&") == 0)
			{
				runBackground = 1;
				args[argsLen - 1] = NULL;
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

			argsLen = argslen(args);
			if (strcmp(args[argsLen - 1], "&") == 0)
			{
				runBackground = 1;
				args[argsLen - 1] = NULL;
			}

			inputRedirect(new_args, args[i + 1], runBackground);
			return 1;
		}
		else if (strcmp(args[i], "|") == 0)
		{
			argsLen = argslen(args);
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


/*
	General framework to run shell. Basically, a shelL:
		1. Read command typed from keyboard
		2. Parse the command and preprocess it
		3. Execute the command
		4. Save latest executed command
*/
void mainLoop()
{
	/*
	Four basic task of a shell:
		1. Read
		2. Parse
		3. Execute
		4. Save
	*/
	char *line;
	char *tempLine;
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
		puts("Type help if you need any helps.");
		fputs("> ", stdout);

		/*	Read */
		line = readLine();
		int lineLen = strlen(line);
		/*printf("line: ");
		for(int i=0;line[i] != '\0'; i++) {
			printf("%c", line[i]);
		}
		printf("\n");*/

		tempLine = (char*)malloc((strlen(line) + 1)* sizeof(char));
		strncpy(tempLine, line, (strlen(line) + 1) * sizeof(char));
		//printf("tempLine replicated from line: %s\n", tempLine);

		/*	Parse	*/
		args = splitLine(line);

		/*	Execute	*/
		shouldRun = execute(args, latestCommandExist, latestCommandLen);

		// Save command that was executed
		latestCommandLen = strlen(tempLine);
		/*printf("tempLine: ");
		for(int i=0;tempLine[i] != '\0'; i++) {
			printf("%c", tempLine[i]);
		}
		printf("\n");*/

		//printf("\n");
		//printf("Is latestCommandLen equal strlen(line): %d, %d\n", latestCommandLen, lineLen);
		/*	Save	*/
		if ( (args[0] != NULL) && (strcmp(args[0], "!!") != 0) )
		{
			fd = open("history", O_CREAT | O_TRUNC | O_RDWR,
								S_IRUSR | S_IRGRP | S_IWGRP | S_IWUSR);
			write(fd, tempLine, latestCommandLen);
			close(fd);

			latestCommandExist = 1;
		}

		free(line);
		free(tempLine);
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
