#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <string.h>

#define MAX_PIPE 5

int sigurs1_received = 0;

// creat pipeList to store pipefd
int pipeList[MAX_PIPE][2];
int informPipe[2];

void sigint_handler(int sig)
{
    // do nothing
}

void sigusr1_handler(int sig)
{
    sigurs1_received = 1;
}

// parse line into tokenArray
int parse_line(char *line, char *tokenArray[], char *seprator)
{
    char *token;
    int tokenCount = 0;

    // default seprator
    if (seprator == NULL)
    {
        seprator = " \n";
    }

    token = strtok(line, seprator);
    if (token == NULL)
    {
        return 0;
    }

    while (token != NULL)
    {
        tokenArray[tokenCount] = token;
        tokenCount++;
        token = strtok(NULL, seprator);
    }
    tokenArray[tokenCount] = NULL;
    return tokenCount;
}

int run_on_child(char *tokenArray[], int pipeIndex, int pipeCount)
{
    int pid = fork();

    if (pid < 0)
    {
        fprintf(stderr, "fork() Failed");
        exit(-1);
    }
    else if (pid == 0)
    {
        // Child Process
        signal(SIGUSR1, sigusr1_handler);

        // bind stdin and stdout to pipeList
        if (pipeCount == 1)
        {
            // do nothing
        }
        else if (pipeIndex == 0)
        {

            if (dup2(pipeList[pipeIndex][1], STDOUT_FILENO) == -1)
            {
                perror("dup2 1");
                exit(-1);
            }
            close(pipeList[pipeIndex][0]);
        }
        else if (pipeIndex == pipeCount - 1)
        {
            if (dup2(pipeList[pipeIndex - 1][0], STDIN_FILENO) == -1)
            {
                perror("dup2 L");
                exit(-1);
            }
            close(pipeList[pipeIndex - 1][1]);
        }
        else
        {
            if (dup2(pipeList[pipeIndex - 1][0], STDIN_FILENO) == -1)
            {
                perror("dup2");
                exit(-1);
            }
            if (dup2(pipeList[pipeIndex][1], STDOUT_FILENO) == -1)
            {
                perror("dup2");
                exit(-1);
            }
            close(pipeList[pipeIndex - 1][1]);
            close(pipeList[pipeIndex][0]);
        }

        // close pipeList except pipeNum or pipeNum - 1
        for (int i = 0; i < pipeCount - 1; i++)
        {
            if (i != pipeIndex || i != pipeIndex - 1)
            {
                close(pipeList[i][0]);
                close(pipeList[i][1]);
            }
        }

        // inform parent process that child process is ready
        if (pipeIndex == 0)
        {
            close(informPipe[0]);
            write(informPipe[1], "1", 2);
            close(informPipe[1]);
        }

        while (!sigurs1_received)
        {
            pause();
        }

        // execute command
        execvp(tokenArray[0], tokenArray);

        // if execvp() returns, it must have failed
        perror(tokenArray[0]);
        exit(-1);
    }
    else
    {
        return pid;
    }
}

int main(int argc, char *argv[])
{
    // readline() variables
    char *line = NULL;
    size_t len = 0;

    // strtok() variables
    char *token;
    char *tokenArray[100];

    // parse pipe variables
    char *pipeTokenArray[MAX_PIPE];
    char *pipeToken;

    // fork() variables
    pid_t pid = -1;

    // register signal handler
    signal(SIGINT, sigint_handler);

    // main loop
main_loop:
    while (1)
    {
        // print shell prompt
        printf("## MyShell [%d] ##  ", getpid());
        fflush(stdout);

        // read input line
        if (getline(&line, &len, stdin) == -1)
        {
            perror("getline");
            exit(-1);
        }

        int pipeCount = parse_line(line, pipeTokenArray, "|");
        int pidCount = 0;
        int pidArray[MAX_PIPE];

        // parse pipe
        if (!pipeCount)
        {
            continue;
        }

        if (pipeCount > MAX_PIPE)
        {
            printf("pipe count exceeds maximum\n");
            continue;
        }

        // clear informPipe
        informPipe[0] = 0;
        informPipe[1] = 0;

        // initialize informPipe
        if (pipe(informPipe) == -1)
        {
            perror("pipe");
            exit(EXIT_FAILURE);
        }

        // clear pipeList
        for (int i = 0; i < pipeCount - 1; i++)
        {
            pipeList[i][0] = 0;
            pipeList[i][1] = 0;
        }

        // initialize pipeList
        for (int i = 0; i < pipeCount - 1; i++)
        {
            if (pipe(pipeList[i]) == -1)
            {
                perror("pipe");
                exit(EXIT_FAILURE);
            }
        }

        while (pidCount < pipeCount)
        {
            int tokenCount = parse_line(pipeTokenArray[pidCount], tokenArray, " \n");
            // parse command
            if (!tokenCount)
            {
                if (pidCount != 0)
                {
                    printf("Error: undefined behavior of pipe\n");
                }
                goto main_loop;
            }

            // check for exit command
            if (strcmp(tokenArray[0], "exit") == 0)
            {
                if (tokenCount == 1)
                {
                    exit(0);
                }
                else
                {
                    printf("Error: \"exit\" with other arguments!\n");
                    goto main_loop;
                }
            }

            pid = run_on_child(tokenArray, pidCount, pipeCount);

            // store pid in pidArray
            pidArray[pidCount] = pid;
            pidCount++;
        }

        // close pipeList
        for (int i = 0; i < pipeCount - 1; i++)
        {
            close(pipeList[i][0]);
            close(pipeList[i][1]);
        }

        // wait for the first child process to be ready
        char buf[2];
        close(informPipe[1]); // Close write end
        read(informPipe[0], &buf, 2);
        close(informPipe[0]); // Close read endw

        // for pid in pidArray
        for (int i = 0; i < pipeCount; i++)
        {
            // send SIGUSR1 to child process
            kill(pidArray[i], SIGUSR1);

            // wait for child process to complete
            idtype_t idtype = P_PID;

            if (waitpid(pidArray[i], NULL, 0) == -1)
            {
                perror("waitid");
                exit(-1);
            }
        }
    }
    return 0;
}