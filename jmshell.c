#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <signal.h>

#define MAX_LINE 80 /* 80 chars per line, per command, should be enough. */
#define HISTORY_SIZE 10

char history[HISTORY_SIZE][MAX_LINE];
int history_count = 0;

/**
 * setup() reads in the next command line, separating it into distinct tokens
 * using whitespace as delimiters. setup() sets the args parameter as a
 * null-terminated string.
 */

void setup(char inputBuffer[], char *args[],int *background)
{
    int length, /* # of characters in the command line */
        i,      /* loop index for accessing inputBuffer array */
        start,  /* index where beginning of next command parameter is */
        ct;     /* index of where to place the next parameter into args[] */
   
    ct = 0;

    /* read what the user enters on the command line */
    length = read(STDIN_FILENO, inputBuffer, MAX_LINE); 

    start = -1;
    if (length == 0)
        exit(0);            /* ^d was entered, end of user command stream */
    if (length < 0){
        perror("error reading the command");
        exit(-1);           /* terminate with error code of -1 */
    }

    /* examine every character in the inputBuffer */
    for (i=0;i<length;i++) {
        switch (inputBuffer[i]){
          case ' ':
          case '\t' :               /* argument separators */
            if(start != -1){
                args[ct] = &inputBuffer[start];    /* set up pointer */
                ct++;
            }
            inputBuffer[i] = '\0'; /* add a null char; make a C string */
            start = -1;
            break;
          case '\n':                 /* should be the final char examined */
            if (start != -1){
                args[ct] = &inputBuffer[start];    
                ct++;
            }
                inputBuffer[i] = '\0';
                args[ct] = NULL; /* no more arguments to this command */
            break;
          default :             /* some other character */
            if (start == -1)
                start = i;
            if (inputBuffer[i] == '&'){
                *background  = 1;
                start = -1;
                inputBuffer[i] = '\0';
            }
          }
     }   
     args[ct] = NULL; /* just in case the input line was > 80 */
}

void exit_command() {
    // Print statss
    fflush(stdout);
    char command[256];
    sprintf(command, "ps -o pid,ppid,%%cpu,%%mem,etime,user,command --pid %d", getpid());
    system(command);
    exit(0);
}

void yell_command(char inputBuffer[]) {
    char *command = strtok(inputBuffer, " ");
    command = strtok(NULL, "\n");
    if (command != NULL) {
        for (int i = 0; command[i]; i++) {
            command[i] = toupper(command[i]);
        }
        printf("%s\n", command);
    } else {
        printf("Invalid 'yell' command.\n");
    }
}

void sigtstp_handler(int signo) {
    // Signal handler for Control-z (SIGTSTP)
    printf("\nShell History:\n");
    for (int i = 0; i < HISTORY_SIZE; i++) {
        if (strlen(history[i]) > 0) {
            printf("%d: %s\n", i + 1, history[i]);
        }
    }
    fflush(stdout);
}


int main(void) {
    pid_t shell_pid = getpid();  // Get the PID of the shell
    printf("Welcome to jmshell. My pid is %d.\n", shell_pid);

    struct sigaction sa;
    sa.sa_handler = sigtstp_handler;
    sa.sa_flags = SA_RESTART; // Restart read() after the interrupt
    sigaction(SIGTSTP, &sa, NULL);

    char inputBuffer[MAX_LINE];       // Buffer to hold the command entered
    int background = 0;              // Equals 1 if a command is followed by '&'
    char *args[MAX_LINE / 2 + 1];   // Command line arguments
    int prompt_count = 1; 

    while (1) {
        background = 0;
        printf("jmshell[%d]: ",  prompt_count);
        fflush(stdout);
        setup(inputBuffer, args, &background);

        // If the command is "exit," exit the shell
        if (strcmp(args[0], "exit") == 0) {
            exit_command();
        }

        if (strcmp(args[0], "yell") == 0) {
            yell_command(inputBuffer);
            continue;
        }

	// History
        if (args[0][0] == 'r' && args[0][1] == '\0') {
            // User wants to execute the most recently executed command
            if (history_count > 0) {
                strcpy(inputBuffer, history[history_count - 1]);
                setup(inputBuffer, args, &background);
            } else {
                printf("No history available.\n");
                continue;
            }
        } else if (args[0][0] == 'r' && args[0][1] >= '0' && args[0][1] <= '9') {
            // User wants to execute a specific command from history
            int index = args[0][1] - '0';
            if (index >= 1 && index <= history_count) {
                strcpy(inputBuffer, history[index - 1]);
                setup(inputBuffer, args, &background);
            } else {
                printf("Invalid history index.\n");
                continue;
            }
        }

        // Store the command in history
        if (history_count < HISTORY_SIZE) {
            strcpy(history[history_count], inputBuffer);
            history_count++;
        } else {
            // If history is full, shift commands to make space for the new command
            for (int i = 0; i < HISTORY_SIZE - 1; i++) {
                strcpy(history[i], history[i + 1]);
            }
            strcpy(history[HISTORY_SIZE - 1], inputBuffer);
        }

        pid_t pid = fork(); // Create a child process

        if (pid < 0) {
            perror("Fork failed");
            exit(1);
        } else if (pid == 0) {
            // Child process
            if (execvp(args[0], args) == -1) {
                perror("Execution failed");
                exit(1);
            }
        } else {
            // Parent process
            printf("[Child pid = %d, background = %s]\n", pid, background ? "TRUE" : "FALSE");

            if (background == 0) {
                // Wait for the child to complete, unless it's a background process
                int status;
                waitpid(pid, &status, 0);
                printf("Child process complete\n");
 		prompt_count++;
            } else {
                // Immediately offer a new prompt for background commands
                prompt_count++;
            }
        }
    }
    return 0;
}
