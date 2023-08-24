# Small Shell

Smallsh: Custom Shell Implementation in C

## Introduction:
Smallsh is a self-made shell implemented in C, designed to emulate the behavior of popular shells like bash. It offers a command-line interface that includes functionalities such as input parsing, parameter expansion, built-in commands, redirection operators, background process execution, and signal handling.

## Features:
> **Interactive Prompt:** Presents an interactive prompt for user input, providing a familiar command-line environment.
> 
> **Command Parsing:** Divides command line input into meaningful tokens, splitting words based on whitespace and managing special characters.
>
> **Parameter Expansion:** Implements parameter expansion, supporting variables like $$ (process ID), $?, and $!.
>
> **Built-in Commands:** Supports built-in commands such as 'exit' for shell termination and 'cd' for changing the working directory.
>
> **External Command Execution:** Executes external commands using appropriate EXEC functions, searching for executables in the system's PATH.
>
> **Redirection Operators:** Implements redirection operators (<, >, >>) to enable input and output stream redirection, enhancing command flexibility.
>
> **Background Execution:** Facilitates background execution of commands using the '&' operator, enabling concurrent execution of multiple processes.
>
> **Signal Handling:** Manages SIGINT (Ctrl+C) and SIGTSTP (Ctrl+Z) signals effectively, ensuring a responsive and user-friendly shell experience.

## Program Functionality:
### Smallsh operates within an infinite loop, performing the following steps:

> Input: Reads user input from stdin or a script file, supporting both interactive and non-interactive modes.
> 
> Word Splitting: Divides input lines into words, accounting for whitespace characters and handling escape sequences.
> 
> Expansion: Expands variables like '$$', '$?', '$!', and '${parameter}' within words.
> 
> Parsing: Parses words into tokens, identifying redirection operators ('>', '<', '>>') and the background operator ('&').
> 
> Execution: Executes built-in commands like 'exit' and 'cd', as well as external commands in new child processes.
> 
> Waiting: Waits for foreground child processes to complete, updating the '$?' variable with exit status, and manages background processes.

### Signal Handling:
> SIGTSTP signal is disregarded, allowing background processes to continue running without interruption.
> SIGINT signal is managed differently based on the context: ignored during input reading but triggers an informative message and prompt re-display in other cases.

## Usage
### Smallsh can be run with or without arguments:

> Interactive Mode: Smallsh presents a prompt and awaits user input.
> Non-Interactive Mode: Smallsh reads commands from a specified script file.

**Error Handling:**
> Smallsh robustly manages errors arising from standard library functions and issues informative error messages when needed.

**Note:**
> This shell project is focused on recreating fundamental shell functionalities and provides a fundamental understanding of process management, signal handling, and I/O redirection in Unix-like systems. It emphasizes C programming and standard system calls for achieving the desired features.
