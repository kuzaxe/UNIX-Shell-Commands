/*
 * fsh.c - the Feeble SHell.
 */

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>

#include "fsh.h"
#include "parse.h"
#include "error.h"

int showprompt = 1;
int laststatus = 0;  /* set by anything which runs a command */
int invalid = 0;
int statval; 

extern char **environ;
extern void find(char *buf, struct pipeline *p);
extern int builtin_exit();
extern int builtin_cd(char **argv);
extern void commander(struct parsed_line *p);
extern void executeCMD(char *buf, struct pipeline *pl);


int main()
{
    char buf[1000];
    struct parsed_line *p;
    extern void execute(struct parsed_line *p);

    while (1) {
        if (showprompt)
            printf("$ ");
        if (fgets(buf, sizeof buf, stdin) == NULL)
            break;
        if ((p = parse(buf))) {
            execute(p);
            freeparse(p);
        }
    }

    return(laststatus);
}



void execute(struct parsed_line *p)
{
    struct parsed_line *cmd;

    if (p->pl != NULL && strcmp(p->pl->argv[0], "exit") == 0) {
        laststatus = builtin_exit();
    }
    commander(p);
    for (cmd = p; cmd; cmd = cmd->next) {
        
        if (cmd != p) {
            invalid = 0;
            switch (cmd->conntype) {
            case CONN_SEQ:
                commander(cmd);
                break;
            case CONN_AND:
                if (laststatus == 0)
                    commander(cmd);
                break;
            case CONN_OR:
                if (laststatus == 1)
                    commander(cmd);
                break;
            }
        }
    }
    
}

void commander(struct parsed_line *p) 
{
    int pass = 0;
    
    
    // buf stores '/bin/ /usr/bin..' paths
    char buf1[1000] = {0};
    char buf2[1000] = {0};
    if (p->pl == NULL) {
        pass = 1;
        laststatus = 0;
    } else {
        invalid = 0;
        if (p->pl != NULL) {
            if (strcmp(p->pl->argv[0], "exit") == 0) {
                laststatus = builtin_exit();
            }
            find(buf1, p->pl);
        }
        if (p->pl->next != NULL) {
            find(buf2, p->pl->next);
        }
    }
    int in, out;

    while (pass == 0 && invalid != 1) {
        int x;
        switch ((x = fork())) {
            case -1:
                perror("fork");
                pass = 1;
                continue;
            case 0:  /* CHILD */
                if (p->outputfile) {
                    close(out);
                    if ((out = open(p->outputfile, O_WRONLY|O_CREAT|O_TRUNC, 0777)) < 0) {
                        perror(p->outputfile);
                        laststatus = 1;
                        pass = 1;
                        continue;
                    }
                    dup2(out, 1);
                }
    
                if (p->inputfile) {
                    close(in);
                    if ((in = open(p->inputfile, O_RDONLY, 0666)) < 0) {
                        perror(p->inputfile);
                        laststatus = 1;
                        pass = 1;
                        continue;
                    }
                    dup2(in, 0);
                }
    
                if (p->pl->next != NULL) {
                    int pipefd[2];
    
                    if (pipe(pipefd)) {
                        perror("pipe");
                        laststatus = 1;
                        pass = 1;
                        continue;
                    }
    
                    switch(fork()) {
                        case -1:
                            perror("pipe");
                            laststatus = 1;
                            pass = 1;
                            continue;
                        case 0:
    
                            close(pipefd[0]);
                            dup2(pipefd[1], 1);
                            close(pipefd[1]);
                            if (strcmp(p->pl->argv[0], "cd") == 0) {
                                laststatus = builtin_cd(p->pl->argv);
                            } else {
                                executeCMD(buf1, p->pl);
                                pass = 1;
                                continue;
                            }
                        default:
                            close(pipefd[1]);
                            dup2(pipefd[0], 0);
                            close(pipefd[0]);
                            if (strcmp(p->pl->argv[0], "cd") == 0) {
                                laststatus = builtin_cd(p->pl->argv);
                            } else {
                                executeCMD(buf2, p->pl->next);
                                pass = 1;
                                continue;
                            }
                    }
                } else {
                    if (strcmp(p->pl->argv[0], "cd") == 0) {
                        laststatus = builtin_cd(p->pl->argv);
                    } else {
                        executeCMD(buf1, p->pl);
                    }
                }
    
    
            default: /* PARENT */
                wait(&statval);
                if(WIFEXITED(statval)) {
                    if (WEXITSTATUS(statval) == 0) {
                        laststatus = 0;
                    } else {
                        laststatus = 1;
                    }
                }
        }
        pass = 1;
    }

    
}


void find(char *buf, struct pipeline *pl)
{
    struct stat statbuf;

    const char* arr[] = {"/bin/", "/usr/bin/", "./"};
    

    // cnt is a check whether to CONTINUE the while loop or not.
    // cnt turns 1 when a valid command is found.
    int cnt = 0;
    int i = -1;
    if  (strchr(pl->argv[0], '/') == NULL) {
        while (cnt == 0 && i < 3) {
            i++;
            if (i == 3) 
                continue;

            // check if complete path name does not exceed string array length
            if (strlen(arr[i]) + strlen(pl->argv[0]) + 1 > 1000) {
                continue;
            }
            strcpy(buf, arr[i]);
            strcat(buf, pl->argv[0]);

            // check if path is a valid command
            if (stat(buf, &statbuf) == 0) {
                cnt = 1;        // command found
                continue;
            }

        }
        if (cnt == 0 && i == 3 && strcmp(pl->argv[0], "cd") != 0) {
            printf("%s: Command not found\n", pl->argv[0]);
            invalid = 1;
            laststatus = 1;
            pl = NULL;
        }

    } else {
        // for cases where a slash is present in the given command './cat'
        strcpy(buf, pl->argv[0]);
    }
}

void executeCMD(char *buf, struct pipeline *pl) {
    invalid = 0;
    if (execve(buf, pl->argv, environ) != 0) {
        perror(buf);
        laststatus = 1;
        invalid = 1;
    }
}
