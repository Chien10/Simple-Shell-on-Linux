#include <stdio.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#define MAX_LEN 1024
#define TOKEN_SIZE 80
#define TOKEN_DELIM " \t\r\n\a"



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

		// Tell the child process to ignore SIGINT
		signal(SIGINT, SIG_IGN);
		
		int status = execvp(args[0], args);
		if (status == -1)
		{
			perror("Invalid Command.\n");
			kill(getpid(), SIGTERM);
		}
		exit(EXIT_FAILURE);
	}
	else if (pid < 0) {
		perror("forking Failed.\n");
	}
	
	if (runBackground == 0)
	{
		do {
			// If the command doesn't end with &, the parent process will wait
			// waitpid waits for a process's state to change
			// Here, I wait until the parent or the child process exited or killed
			// If so, the function will return 1
			// More about waitpid: https://linux.die.net/man/3/waitpid, https://www.ibm.com/support/knowledgecenter/en/SSLTBW_2.2.0/com.ibm.zos.v2r2.bpxbd00/rtwaip.htm
			// https://linux.die.net/man/2/waitpid
			// Actually, returned value of waitpid will be stored in startLoc
			wpid = waitpid(pid, &startLoc, WUNTRACED);
		} while (!WIFEXITED(startLoc) & !WIFSIGNALED(startLoc));
		/*
		WIFEXISTED returns non-zero value if the child process terminated normally with exit function
		WIFSIGNALED returns non-zero value if the child process terminated 'cause it received a signal which
					was not handled. More about signal: https://www.gnu.org/software/libc/manual/html_node/Signal-Handling.html#Signal-Handling
		*/
	}

	return 1;
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

void outputRedirect(char *args[], char *outputFile)
{
	int fd, status;
	signed int pid;

	pid = fork();
	if (pid == 0)
	{
		fd = open(outputFile, O_CREAT | O_TRUNC | O_WRONLY, 600);
		dup2(fd, STDOUT_FILENO);
		close(fd);

		status = execvp(args[0], args);
		if (status == -1)
		{
			perror("Invalid command.\n");
			// A gentleman way to kill a process
			kill(getpid(), SIGTERM);
		}
	}
	// Since start_loc is NULL, waitpid waits without caring about the child's return values
	waitpid(pid, NULL, 0);
}

void inputRedirect(char *args[], char *inputFile)
{
	int fd, status;
	signed int pid;

	pid = fork();
	if (pid == 0)
	{
		fd = open(inputFile, O_RDONLY, 0600);
		dup2(fd, STDIN_FILENO);
		close(fd);

		status = execvp(args[0], args);
		if (status == -1)
		{
			perror("Invalid command.\n");
			// A gentleman way to kill a process
			kill(getpid(), SIGTERM);
		}
	}

	waitpid(pid, NULL, 0);
}

int execute(char **args)
{
	int i = 0, runBackground = 0;
	int commandSize = sizeof args / sizeof(char*);

	// If a command entered is empty
	if (args[0] == NULL) {
		return 1;
	}

	// If user typed built-in commands
	for (; i < numBuiltins(); i++)
	{
		if (strcmp(args[0], builtinCommands[i]) == 0) {
			return (*builtinFunctions[i])(args);
		}
	}

	// Otherwise
	char *new_args[5];
	i = 0;
	while (args[i] != NULL)
	{
		if ((strcmp(args[i], ">") == 0) || (strcmp(args[i], "<") == 0)
					|| (strcmp(args[i], "&") == 0)) {
			break;
		}

		new_args[i++] = args[i++];
	}

	while (args[i] != NULL && runBackground == 0)
	{
		if ((strcmp(args[i], "&") == 0) && (i == commandSize - 1)) {
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

			outputRedirect(new_args, args[i + 1]);
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

			inputRedirect(new_args, args[i + 1]);
			return 1;
		}

		i++;
	}

	new_args[i] = NULL;
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

	do
	{
		printf("Type help if you need any helps.\n");
		printf("> ");

		line = readLine();
		args = splitLine(line);
		shouldRun = execute(args);

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
