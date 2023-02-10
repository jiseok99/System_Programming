#include "myshell.h"
#include<errno.h>
void eval(char *cmdline); 
void pipe_eval(char *cmdline);
int parseline(char *buf, char **argv);
int builtin_command(char **argv); 
#define MAXARGS   128

void unix_error(char *msg){ /* Unix-style error */
    fprintf(stderr, "%s: %s\n", msg, strerror(errno));
    exit(0);
}

void sighandler(int sig)// when ctrl-c is entered
{
  printf("\nCSE4100-SP-P#1>\n");
}

int main(){
    char cmdline[MAXLINE];
    
    while(1){
        signal(SIGINT, sighandler);
        printf("CSE4100-SP-P#1>");
        fgets(cmdline, MAXLINE, stdin); 

        if (feof(stdin))
	        exit(0);

        if(strchr(cmdline, '|')) //if there is pipe in string
            pipe_eval(cmdline);
        else
            eval(cmdline);
    }
    return 0;
}

void eval(char *cmdline) 
{
    char *argv[MAXARGS]; /* Argument list execve() */
    char buf[MAXLINE];   /* Holds modified command line */
    int bg;              /* Should the job run in bg or fg? */
    pid_t pid;           /* Process id */

    strcpy(buf, cmdline);
    bg = parseline(buf, argv); 
    if (argv[0] == NULL)  
	    return;   /* Ignore empty lines */
    if (!builtin_command(argv)) { 
        if((pid = fork()) == 0){
           if (execvp(argv[0], argv) < 0){
                printf("%s: Command not found.\n", argv[0]);
                exit(0);
            }
        }
	/* Parent waits for foreground job to terminate */
	if (!bg){ 
	    int status;
        if (waitpid(pid, &status, 0) < 0) 
            unix_error("waitfg: waitpid error");
	}
	else//when there is backgrount process!
	    printf("%d %s", pid, cmdline);
    }
    return;
}

void pipe_eval(char *cmdline){
    char *argv[MAXARGS]; /* Argument list execve() */
    char *each_command[MAXARGS]; // each command seprated by pipe('|')
    char buf[MAXLINE];   /* Holds modified command line */
    int bg;              /* Should the job run in bg or fg? */
    pid_t pid;           /* Process id */
    int prev_des[2], curr_des[2]; // previous & current file descriptor

    strcpy(buf, cmdline);
    bg = parseline(buf, argv); 
    if (argv[0] == NULL)  
	    return;   /* Ignore empty lines */

    int argv_idx = 0, each_idx = 0; //index of argv & each_command
    while (argv[argv_idx] != NULL) {

        while (argv_idx < MAXARGS) {   // for all argv
            if (argv[argv_idx] == NULL) {  // if end of the string
                each_command[each_idx] = NULL;
                each_idx = 0;
                break;
            }
            else if (!strcmp("|", argv[argv_idx])) {   //if pipe found
                each_command[each_idx] = NULL;
                each_idx = 0;
                argv_idx++;
                break;
            }
            
            else if(strchr(argv[argv_idx], '|')){ //if pipe(|) is included in the string
                each_command[each_idx] = (char*)malloc(sizeof(char) * MAXARGS); //allocate array

                if(argv[argv_idx][0] != '|'){ //when pipe is at middle of the string
                    strcpy(each_command[each_idx], strtok(argv[argv_idx],"|")); //seperate pipe from string
                    argv[argv_idx] = strtok(NULL,"|");
                    each_idx++;
                    
                }

                else //when pipe is at begining of the string
                    argv[argv_idx] = strtok(argv[argv_idx],"|"); //seperate pipe from string

                each_command[each_idx] = NULL;
                each_idx = 0;
                break;
               
            }
            else {  // copy string to each_command
                each_command[each_idx] = (char*)malloc(sizeof(char) * MAXARGS); //allocate array
                strcpy(each_command[each_idx], argv[argv_idx]);
                each_idx++;
            }
            argv_idx++;
        }

        if (pipe(curr_des) < 0) {   // create pipe
            unix_error("Pipe error");
        }
        if (!builtin_command(each_command)) { //quit -> exit(0), & -> ignore, other -> run
            if((pid = fork()) == 0){
                dup2(prev_des[0], 0); // get saved input from previous file descriptor(read)

               if (argv[argv_idx] != NULL) 
                    dup2(curr_des[1], 1); // if it is not end of the string, 'write' to current file descriptor

                close(curr_des[0]); //close unusing file descriptor
                if (execvp(each_command[0], each_command) < 0){
                    printf("%s: Command not found.\n", argv[0]);
                    exit(0);
                }
            }

               
    
            /* Parent waits for foreground job to terminate */
            if (!bg) {
                int status;
                if (waitpid(pid, &status, 0) < 0)
                     unix_error("waitfg: waitpid error");     

                close(curr_des[1]); //close unusing file descriptor
                // save current file descriptor in previous file descriptor
                prev_des[0] = curr_des[0]; //read
                prev_des[1] = curr_des[1]; //write
            }
            else //when there is backgrount process!
                printf("%d %s", pid, cmdline);
        }
    }
    return;

}

/* If first arg is a builtin command, run it and return true */
int builtin_command(char **argv) 
{
    if (!strcmp(argv[0], "exit")) /* exit command */
	    exit(0);  
    if(!strcmp(argv[0], "cd")) { /* cd command */
        if (argv[1] == NULL)  // when only 'cd' is entered
            chdir(getenv("HOME"));

        else {
            if(chdir(argv[1]) < 0)
                printf("%s: Wrong direction\n", argv[1]);
        }
        return 1;
    }
    if (!strcmp(argv[0], "&"))    /* Ignore singleton & */
	    return 1;

    return 0;                     /* Not a builtin command */
}

int parseline(char *buf, char **argv) 
{
    char *delim;         /* Points to first space delimiter */
    int argc;            /* Number of args */
    int bg;              /* Background job? */

    buf[strlen(buf)-1] = ' ';  /* Replace trailing '\n' with space */
    while (*buf && (*buf == ' ')) /* Ignore leading spaces */
	buf++;

    /* Build the argv list */
    argc = 0;
     while ((delim = strchr(buf, ' '))) {
        if(*buf == '"' || *buf == '\''){ //if " or ' is found
            char temp = *buf;
            buf++;
            delim = strchr(buf, temp); //delete them from the string
        }
    
	    argv[argc++] = buf;
	    *delim = '\0';
	    buf = delim + 1;
        while (*buf && (*buf == ' ')) /* Ignore spaces */
			buf++;
    }
    argv[argc] = NULL;
    
    if (argc == 0)  /* Ignore blank line */
	return 1;

    /* Should the job run in the background? */
    if ((bg = (*argv[argc-1] == '&')) != 0)
	argv[--argc] = NULL;

    return bg;
}