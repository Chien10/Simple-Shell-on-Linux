# Simple-Shell-on-Linux
A C program to serve as a shell interface accepts user commands then executes each of them in separate process.

## Shell Functions
- Support I/O redirection.
- Provide pipe as a form of IPC between a pair of commands.

## Required System Calls for this Project
Five UNIX system calls: fork(), exec(), wait(), dup2() and pipe().

## What do My Team Do in This Lab?
- Creating a child process and executing a user's command within the process.
- Providing a history feature to execute the most recent command.
- Supporting I/O redirection.
- Implementing pipe to let parent and child processes communicate.

## Overview
- Basically, to implement a shell interface, a coder usually create a parent process to read a user's command. Then, a seperate child process will be created to execute the command. The parent process must wait for the child one finishes to resume its action.
- UNIX system allows the two processes to run concurrently by including an ampersand (&) at the end of the command.
- **fork()** system call is crucial to create a child process while functions in the **exec() family** are important to execute the user's command.
- In this shell, to execute the most recent command entered, user can enter *!!* (It's similar to press upper arrow in normal shell). Entering *!!* could raise *No commands in history" if there's no recent command*.
- Redirecting I/O using *>* and *<* operators is a common functionality provided by any shells. In this lab, my team will only implement the case when the command consists of two operands and one operators. **dup2()** function is used for this implementation.
- Allowing different commands to communicate with each other is also a familiar functionality. The most straightforward way to implement this functionality is to have a parent process create a chid process to execute the first command. Next, the child process performs two tasks: create a new child process to execute the second process, establishes a pipe between itself and its child process. UNIX **pipe()** and **dup2()** should be used for this implementation. (Command can contain only one pipe character and no redirection operators will appear).
