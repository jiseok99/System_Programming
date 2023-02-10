#include "myshell.h"

void eval(char *cmdline);
void pipe_eval(char *cmdline);
int parseline(char *buf, char **argv);
int builtin_command(char **argv); 
void sigint_handler(int sig);
void sigchld_handler(int sig);
void sigtstp_handler(int sig);
#define MAXARGS   128

//job state
#define default 0	 // default
#define foreground 1 // run in foreground 
#define background 2 // run in background 
#define stop 3		 // stopped 
#define done 4		 // done 
#define KILL 5		 // killed

typedef struct job {		
	pid_t pid;				// job pid 
	int id;					// job id 
	int state;				// job state
	char cmdline[MAXLINE];	// command line
}job;

int job_idx = 0;
job job_list[MAXARGS];

void insert_job(pid_t pid, int state, char* cmdline);

void unix_error(char *msg){ /* Unix-style error */
    fprintf(stderr, "%s: %s\n", msg, strerror(errno));
    exit(0);
}

void sigint_handler(int sig) {// when ctrl-c is entered
  printf("\nCSE4100-SP-P#1>\n");
}

void sigchld_handler(int sig){ 
    pid_t pid;
    int status;

    while (1){
        pid = waitpid(-1, &status, WNOHANG | WCONTINUED);
        if(pid <= 0) break;
        if (WIFSIGNALED(status)) {
            for (int i = 0; i < MAXARGS; i++) {
		        if (job_list[i].pid == pid) {
			        job_list[i].state = done;
		        }
	        }
        }
    }
    return;
}

void sigtstp_handler(int sig) {// when ctrl-z is entered  
   for (int i = 0; i < MAXARGS; i++) {
       if(job_list[i].state == foreground){
           job_list[i].state = stop;
           kill(-job_list[i].pid, SIGTSTP);
       }
   }
   return;
}

int main(){
    char cmdline[MAXLINE];
   
    while(1){
        signal(SIGINT, sigint_handler);
        signal(SIGCHLD, sigchld_handler);
        signal(SIGTSTP, sigtstp_handler);

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

void insert_job(pid_t pid, int state, char* cmdline){//insert the job
	if(pid > 0){
			// add information to empty array 
			if(job_list[job_idx].state == default){
				job_list[job_idx].pid = pid;		
				job_list[job_idx].id = job_idx + 1;
                job_list[job_idx].state = state;
				strcpy(job_list[job_idx].cmdline, cmdline);
				job_idx++;
                return;
			}	
	}
	
	return;
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
    insert_job(pid, bg + 1, cmdline);
	if (!bg){ 
	    int status;
        if (waitpid(pid, &status, 0) < 0) 
            unix_error("waitfg: waitpid error");
        else{
            for(int i = 0; i < MAXARGS; i++){
                if(job_list[i].state == foreground && job_list[i].pid != pid){
                    job_list[i].state = default;
                    job_list[i].pid = 0;
                    job_list[i].id = 0;
                    strcpy(job_list[i].cmdline, "\0");
                }

            }
        }
	}
        
    }
    return;
}

void pipe_eval(char *cmdline){
    char *argv[MAXARGS]; /* Argument list execve() */
    char* each_command[MAXARGS]; // each command seprated by pipe('|')
    char buf[MAXLINE];   /* Holds modified command line */
    int bg;              /* Should the job run in bg or fg? */
    pid_t pid;           /* Process id */
    int prev_des[2], curr_des[2]; // previous & current file descriptor

    strcpy(buf, cmdline);
    bg = parseline(buf, argv); 
    if (argv[0] == NULL)  
	    return;   /* Ignore empty lines */

    int argv_idx = 0, each_idx = 0; //index of argv & each_command
    int insert_count = 0;
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
            exit(0);
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
              close(curr_des[1]); //close unusing file descriptor
                // save current file descriptor in previous file descriptor
                prev_des[0] = curr_des[0]; //read
                prev_des[1] = curr_des[1]; //write
            
             if(insert_count == 0){
                insert_job(pid,bg+1,cmdline);   
                insert_count++;

                
                for(int i = 0; i < MAXARGS; i++){
                    if(job_list[i].state == foreground && job_list[i].pid != pid){
                        job_list[i].state = default;
                        job_list[i].pid = 0;
                        job_list[i].id = 0;
                        strcpy(job_list[i].cmdline, "\0");
                        }
                    }
               }       
    
            /* Parent waits for foreground job to terminate */
            if (!bg) {
                int status;
                if (waitpid(pid, &status, 0) < 0)
                     unix_error("waitfg: waitpid error");
            }
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
    if (!strcmp(argv[0], "jobs")){//job command
         for(int i = 0; i< MAXARGS; i++){
            if(job_list[i].state == background ){ // || job_list[i].state == foreground
                    printf("[%d] running %s\n", job_list[i].id, strtok(job_list[i].cmdline, "&"));
             }
            else if(job_list[i].state == stop){
                 if(!strchr(job_list[i].cmdline,'&'))
                    printf("[%d] suspended %s", job_list[i].id, job_list[i].cmdline);
                  else
                    printf("[%d] suspended %s\n", job_list[i].id, strtok(job_list[i].cmdline, "&"));
                }
         }
         return 1;
     }
      if (!strcmp(argv[0], "kill")){//kill command
        int kill_count = 0;
        for(int i = 0; i< MAXARGS; i++){
            if(job_list[i].id == atoi(&argv[1][1])){
            kill(-job_list[i].pid, SIGKILL);
	        job_list[i].state = KILL;
            kill_count++;
            }
        }
    if(kill_count == 0)
     printf("No Such Job\n");
           return 1;
      }
      if (!strcmp(argv[0], "bg")){//background command
          int bg_count  = 0;
           for(int i = 0; i< MAXARGS; i++){
               if(job_list[i].id == atoi(&argv[1][1])){
                    kill(job_list[i].pid, SIGCONT);
                    job_list[i].state = 2;
                    bg_count++;
                    if(!strchr(job_list[i].cmdline,'&'))
                        printf("[%d] running %s", job_list[i].id, job_list[i].cmdline);
                    else
                        printf("[%d] running %s\n", job_list[i].id, strtok(job_list[i].cmdline, "&"));
               }
           }
        if(bg_count == 0)
            printf("No Such Job\n");

           return 1;
      }
      if (!strcmp(argv[0], "fg")){//foreground command
          int fg_count = 0;
           for(int i = 0; i< MAXARGS; i++){
               if(job_list[i].id == atoi(&argv[1][1])){
                    kill(job_list[i].pid, SIGCONT);
                    job_list[i].state = 1;
                    fg_count++;
                    if(!strchr(job_list[i].cmdline,'&'))
                        printf("[%d] running %s", job_list[i].id, job_list[i].cmdline);
                    else
                        printf("[%d] running %s\n", job_list[i].id, strtok(job_list[i].cmdline, "&"));
               }
           }
          if(fg_count == 0)
            printf("No Such Job\n");

           return 1;
      }

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