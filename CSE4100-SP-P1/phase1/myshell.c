#include "myshell.h"

void eval(char *cmdline);
int parseline(char *buf, char **argv);
int builtin_command(char **argv); 
#define MAXARGS   128

void unix_error(char *msg){ /* Unix-style error */
    fprintf(stderr, "%s: %s\n", msg, strerror(errno));
    exit(0);
}

void sighandler(int sig)
{
  printf("\nCSE4100-SP-P#1>\n");
}

int main(){
    char cmdline[MAXLINE];
    signal(SIGINT, sighandler);
    while(1){
        printf("CSE4100-SP-P#1>");
        fgets(cmdline, MAXLINE, stdin);
        
        if (feof(stdin))
	        exit(0);

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
    char *delim;         // Points to first space delimiter 
    int argc;            // Number of args 
    int bg;              // Background job? 

    buf[strlen(buf)-1] = ' ';  // Replace trailing '\n' with space 
    while (*buf && (*buf == ' ')) // Ignore leading spaces 
    buf++;

    // Build the argv list 
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
    
    if (argc == 0)  // Ignore blank line 
	return 1;

    // Should the job run in the background? 
    if ((bg = (*argv[argc-1] == '&')) != 0)
	argv[--argc] = NULL;

    return bg;
}  
