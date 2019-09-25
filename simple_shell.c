#include <stdio.h>
#include <sys/wait.h>
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
	char **tokens = (char*)malloc(bufSize * sizeof(char*));
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

int launchProgram(char **args)
{
	signed int pid, wpic; // pid is process ID
	int status = 0;

	pid = fork();
	// The parent process will return 0 to its child one if the children takes the first
	if (pid == 0)
	{	
		// The first arg is the name of the program and let the OS search for it
		// The latter arg is an array of string arguments
		if (execvp(args[0], args) == -1) {
			perror("pid <> 0.\n");
		}
		exit(EXIT_FAILURE);
	}
	else if (pid < 0) {
		perror("forking Failed.\n");
	}
	// fork succeeded
	else
	{
		do {
			// waitpid waits for a process's state to change
			// Here, I wait until the parent or the child process exited or killed
			// If so, the function will return 1
			wpid = waitpid(pid, &status, WUNTRACED);
		} while (!WIFEXISTED(status) & !WIFSIGNALED(status));
		/*
		WIFEXISTED returns non-zero value if the child process terminated normally with exit function
		WIFSIGNALED returns non-zero value if the child process terminated 'cause it received a signal which 
					was not handled. More about signal: https://www.gnu.org/software/libc/manual/html_node/Signal-Handling.html#Signal-Handling 
		*/
	}
}

// Built-in commands for the shell
char *builtinCommands[] = {"help", "exit"};
int (*builtinFunctions[]) (char**) = {&help, &exit};
int numBuiltins() {
	return sizeof builtinCommands / sizeof(char *);
}

int help(char **args)
{
	int i;

	printf("\t-----A Shell implemented by Minh Chien and Thanh Dat-----\n");
	printf("Features support by this shell:\n");
	print("\t1. Type '&' at the end of your command to run concurrently.\n")
	printf("\t2. Type '!!' at the beginning of your command to execute the most current command.\n")
	printf("\t You'll may receive 'No command in history' response.\n");
	printf("\t3. Using '>' or '<' to redirect I/O.\n");
	printf("\t4. Using '|' to pipe.\n");

	return 1;
}

int exit(char **args) {
	return 1;
}

int execute(char **agrs)
{
	int i = 0;

	// If a command entered is empty
	if (args[0] == NULL) {
		return 1;
	}	

	// If user typed built-in commands
	for (; i < numBuiltins(); i++)
	{
		if (strcmp(args[0], builtinCommands[i]) == 0) {
			return (*builtinCommands[i])(args);
		}
	}

	return launchProgram(args);
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
		printf("Type 'help' if you need any helps.\n");
		printf("> ");

		line = readLine()
		args = splitLine(line);
		shouldRun = execute(args);

		free(line);
		free(args);
	} while(shouldRun);
}

int main(int argc, char **argv)
{
	// Load config files (optional)

	main_loop();

	// Perform any shutdown/ cleanup

	return 0;
}
