/* 
 * tsh - A tiny shell program with job control
 * 
 * Name: Gao Jiang
 * AndrewID: gaoj
 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <errno.h>

/* Misc manifest constants */
#define MAXLINE    1024   /* max line size */
#define MAXARGS     128   /* max args on a command line */
#define MAXJOBS      16   /* max jobs at any point in time */
#define MAXJID    1<<16   /* max job ID */

/* Job states */
#define UNDEF         0   /* undefined */
#define FG            1   /* running in foreground */
#define BG            2   /* running in background */
#define ST            3   /* stopped */

/* 
 * Jobs states: FG (foreground), BG (background), ST (stopped)
 * Job state transitions and enabling actions:
 *     FG -> ST  : ctrl-z
 *     ST -> FG  : fg command
 *     ST -> BG  : bg command
 *     BG -> FG  : fg command
 * At most 1 job can be in the FG state.
 */

/* Parsing states */
#define ST_NORMAL   0x0   /* next token is an argument */
#define ST_INFILE   0x1   /* next token is the input file */
#define ST_OUTFILE  0x2   /* next token is the output file */


/* Global variables */
extern char **environ;      /* defined in libc */
char prompt[] = "tsh> ";    /* command line prompt (DO NOT CHANGE) */
int verbose = 0;            /* if true, print additional output */
int nextjid = 1;            /* next job ID to allocate */
char sbuf[MAXLINE];         /* for composing sprintf messages */

struct job_t {              /* The job struct */
    pid_t pid;              /* job PID */
    int jid;                /* job ID [1, 2, ...] */
    int state;              /* UNDEF, BG, FG, or ST */
    char cmdline[MAXLINE];  /* command line */
};
struct job_t job_list[MAXJOBS]; /* The job list */

struct cmdline_tokens {
    int argc;               /* Number of arguments */
    char *argv[MAXARGS];    /* The arguments list */
    char *infile;           /* The input file */
    char *outfile;          /* The output file */
    enum builtins_t {       /* Indicates if argv[0] is a builtin command */
        BUILTIN_NONE,
        BUILTIN_QUIT,
        BUILTIN_JOBS,
        BUILTIN_BG,
        BUILTIN_FG} builtins;
};

/* End global variables */


/* Function prototypes */
void eval(char *cmdline);

void sigchld_handler(int sig);
void sigtstp_handler(int sig);
void sigint_handler(int sig);

/* Here are helper routines that we've provided for you */
int parseline(const char *cmdline, struct cmdline_tokens *tok); 
void sigquit_handler(int sig);

void clearjob(struct job_t *job);
void initjobs(struct job_t *job_list);
int maxjid(struct job_t *job_list); 
int addjob(struct job_t *job_list, pid_t pid, int state, char *cmdline);
int deletejob(struct job_t *job_list, pid_t pid); 
pid_t fgpid(struct job_t *job_list);
struct job_t *getjobpid(struct job_t *job_list, pid_t pid);
struct job_t *getjobjid(struct job_t *job_list, int jid); 
int pid2jid(pid_t pid); 
void listjobs(struct job_t *job_list, int output_fd);

void usage(void);
void unix_error(char *msg);
void app_error(char *msg);
typedef void handler_t(int);
handler_t *Signal(int signum, handler_t *handler);

/* I/O function copied from csapp.c */
static void sio_reverse(char s[]);
static void sio_ltoa(long v, char s[], int b); 
static size_t sio_strlen(char s[]);
ssize_t sio_puts(char s[]);
ssize_t sio_putl(long v);
void sio_error(char s[]);

/*
 * main - The shell's main routine 
 */
int main(int argc, char **argv) 
{
    char c;
    char cmdline[MAXLINE];    /* cmdline for fgets */
    int emit_prompt = 1; /* emit prompt (default) */

    /* Redirect stderr to stdout (so that driver will get all output
     * on the pipe connected to stdout) */
    dup2(1, 2);

    /* Parse the command line */
    while ((c = getopt(argc, argv, "hvp")) != EOF) {
        switch (c) {
        case 'h':             /* print help message */
            usage();
            break;
        case 'v':             /* emit additional diagnostic info */
            verbose = 1;
            break;
        case 'p':             /* don't print a prompt */
            emit_prompt = 0;  /* handy for automatic testing */
            break;
        default:
            usage();
        }
    }

    /* Install the signal handlers */

    /* These are the ones you will need to implement */
    Signal(SIGINT,  sigint_handler);   /* ctrl-c */
    Signal(SIGTSTP, sigtstp_handler);  /* ctrl-z */
    Signal(SIGCHLD, sigchld_handler);  /* Terminated or stopped child */
    Signal(SIGTTIN, SIG_IGN);
    Signal(SIGTTOU, SIG_IGN);

    /* This one provides a clean way to kill the shell */
    Signal(SIGQUIT, sigquit_handler); 

    /* Initialize the job list */
    initjobs(job_list);


    /* Execute the shell's read/eval loop */
    while (1) {

        if (emit_prompt) {
            printf("%s", prompt);
            fflush(stdout);
        }
        if ((fgets(cmdline, MAXLINE, stdin) == NULL) && ferror(stdin))
            app_error("fgets error");
        if (feof(stdin)) { 
            /* End of file (ctrl-d) */
            printf ("\n");
            fflush(stdout);
            fflush(stderr);
            exit(0);
        }
        
        /* Remove the trailing newline */
        cmdline[strlen(cmdline)-1] = '\0';
        
        /* Evaluate the command line */
		//fprintf(stderr, "got cmd:%s\n", cmdline);
        eval(cmdline);
        
        fflush(stdout);
        fflush(stdout);
    } 
    
    exit(0); /* control never reaches here */
}

/*
 * cmd_builtins - Deal with all the builtin commands
 *
 * Parameters: 
 *      token - tokens parsed from the shell input
 *
 * Returns:
 *      0 - the command is not a builtin command
 *      1 - the command is a builtin command
 *
 */
 int cmd_builtins(struct cmdline_tokens *token) {

    FILE *output_file = NULL;
    int output_fd;
    int job_id;
    struct job_t *job;
    sigset_t set;

    sigemptyset(&set); // set a empty sigset for sigsuspend

    switch (token -> builtins) {

        case BUILTIN_NONE:
            return 0; 

        case BUILTIN_QUIT:
            exit(0);

        case BUILTIN_JOBS:
			
            /*
             * Determine whether there is redirect output file
             * If so, list jobs on the output file
             * If not, list jobs on stardard system out
             */
            if (token -> outfile != NULL) { 

                output_fd = open(token -> outfile, O_RDWR); // try to get the redirect file descriptor

                /*
                 * If get the file descriptor failed, try to open the file first
                 */
                if (output_fd < 0) {

                    output_file = fopen(token -> outfile, "w");
                    output_fd = open(token -> outfile, O_RDWR); // get the file descriptor again

                }

                listjobs(job_list, output_fd); // list jobs to the redirect file

                if (output_file != NULL) {

                    fclose(output_file);

                }

                close(output_fd);

            } else {
				
                listjobs(job_list, STDOUT_FILENO); // if there is no redirect output file, print jobs on standard system out

            }

            return 1;

        case BUILTIN_BG:

            /*
             * Determine whether there is job id in the command
             * If so, get the job by job id
             * If not, get the job by process id
             */
            if (token -> argv[(token -> argc) - 1][0] == '%') {
                
				job_id = atoi((token -> argv[(token -> argc) - 1]) + 1); // get the job id, which is in the last argument except for the first %
                job = getjobjid(job_list, job_id);

                if (job == NULL) {

                    printf("Job %d doesn't exit.\n", job_id);
                    return 1;
                }

            } else {

                job = getjobpid(job_list, atoi(token -> argv[(token -> argc) - 1])); // get the job with pid

                if (job == NULL) {

                    printf("Process %s doesn't exit.\n", token -> argv[1]);
                    return 1;

                }

            }
			
			printf("[%d] (%d) %s\n", job -> jid, job -> pid, job -> cmdline); // print out the BG job presentition to the screen
            
            if ((job -> state != BG) && (job -> state != ST)) { // determine whether the job is currently a background job

                printf("Current process is not a background job.\n");

            }

            job -> state = BG; // set the job as background job
            kill(-(job -> pid), SIGCONT); // send signals to every process in the process group to continue

            return 1;

        case BUILTIN_FG:

            /*
             * Determine whether there is job id in the command
             * If so, get the job by job id
             * If not, get the job by process id
             */
            if (token -> argv[(token -> argc) - 1][0] == '%') {

                job_id = atoi((token -> argv[(token -> argc) - 1]) + 1); // get the job id, which is in the last argument except for the first %
                job = getjobjid(job_list, job_id);

                if (job == NULL) {

                    printf("Job %d doesn't exit.\n", job_id);
                    return 1;
                }

            } else {

                job = getjobpid(job_list, atoi(token -> argv[(token -> argc) - 1])); // get the job with pid

                if (job == NULL) {

                    printf("Process %s doesn't exit.\n", token -> argv[1]);
                    return 1;

                }

            }

            /*
             * Determine whether the job is stopped or in the background, which is legal to bring foreground
             */
             if ((job -> state != ST) && (job -> state != BG)) {

                printf("Current process is not a background job. Illegal to bring foreground.\n");
                return 1;

             }

             /*
              * Wait unti the foreground job finished
              */
             while (fgpid(job_list)) {

                sigsuspend(&set);

             }

             job -> state = FG; // set the job as foreground job
             kill(-(job -> pid), SIGCONT); // send signals to every other process in the process group to continue

             return 1;

        default:
            return 0;

    }
 }
/* 
 * eval - Evaluate the command line that the user has just typed in
 * 
 * If the user has requested a built-in command (quit, jobs, bg or fg)
 * then execute it immediately. Otherwise, fork a child process and
 * run the job in the context of the child. If the job is running in
 * the foreground, wait for it to terminate and then return.  Note:
 * each child process must have a unique process group ID so that our
 * background children don't receive SIGINT (SIGTSTP) from the kernel
 * when we type ctrl-c (ctrl-z) at the keyboard.  
 */
void eval(char *cmdline) 
{
    int bg; /* should the job run in bg or fg? */
    struct cmdline_tokens tok;
	pid_t pid; /* process id of each process */ 
	struct job_t *job;
    sigset_t set;
	sigset_t prev_set;
    int input_fd, output_fd; /* input and output redirect file descriptor */

    /* Parse command line */
    bg = parseline(cmdline, &tok); 

    if (bg == -1) /* parsing error */
        return;

    if (tok.argv[0] == NULL) /* ignore empty lines */
        return;
	
    /*
     * Determine whether the command is builtin command
     * If so, handled by the helper function
     * If not, fork a child process and handle it
     */
    if (!cmd_builtins(&tok)) {

        /*
         * Block signals before the parent add the child into job list to avoid race condition and ensure parent can reap child process
         */
        if (sigemptyset(&set) < 0) {

            unix_error("sigemptyset failed.\n");

        }

        if (sigaddset(&set, SIGINT) || sigaddset(&set, SIGTSTP) || sigaddset(&set, SIGCHLD)) {

            unix_error("sigaddset failed.\n");

        }

        if (sigprocmask(SIG_BLOCK, &set, &prev_set) < 0) {

            unix_error("sigprocmask failed.\n");

        }
		
		/*
         * fork a child process to run the job
         */
        if ((pid = fork()) == 0) {
 			
			/* asign unique group process to each child process 
			 * set the the group id as the process id
             */
            if (setpgid(0, 0) < 0) {

                unix_error("set process group failed.\n");
                return;

            }           
			
			// unblock the signal set for receiving signals 
            if (sigprocmask(SIG_UNBLOCK, &set, &prev_set) < 0) {

                unix_error("sigprocmask failed.\n");

            }

            // redirect the input file descriptor
            if (tok.infile != NULL) {

                input_fd = open(tok.infile, O_RDONLY);

                if (dup2(input_fd, 0) < 0) {

                    unix_error("Failed to restore stdin.\n");

                }

            }

            // redirect the output file descriptor
            if (tok.outfile != NULL) {

                output_fd = open(tok.outfile, O_WRONLY);

                if (dup2(output_fd, 1) < 0) {

                    unix_error("Failed to restore stdout.\n");

                }

            }

			// execute a non-builtin process
			if( execve(tok.argv[0], tok.argv, environ) < 0 ) {
				
				printf("%s: Command not found!\n", tok.argv[0]);
				exit(0);

			}

        }

        // parent process add the child process in the job list
        addjob (job_list, pid, bg ? BG : FG, cmdline);	
		
		/*
         * In the parent process, determine whehter the child process is a foreground process
         * If so, parent process should wait for it
         * If not, leave it as a background job
         */
		if (!bg) {

            while (fgpid(job_list)) {

                sigsuspend(&prev_set); // wait until get the signal that the foreground child process finishes

            }
            		
		 } else {
			
			job = getjobpid(job_list, pid);	
			printf("[%d] (%d) %s\n", job -> jid, job -> pid, cmdline);
			
		 }
	
		 sigprocmask(SIG_UNBLOCK, &set, &prev_set); // unblock the signal set to receive any signals

    }

}

/* 
 * parseline - Parse the command line and build the argv array.
 * 
 * Parameters:
 *   cmdline:  The command line, in the form:
 *
 *                command [arguments...] [< infile] [> oufile] [&]
 *
 *   tok:      Pointer to a cmdline_tokens structure. The elements of this
 *             structure will be populated with the parsed tokens. Characters 
 *             enclosed in single or double quotes are treated as a single
 *             argument. 
 * Returns:
 *   1:        if the user has requested a BG job
 *   0:        if the user has requested a FG job  
 *  -1:        if cmdline is incorrectly formatted
 * 
 * Note:       The string elements of tok (e.g., argv[], infile, outfile) 
 *             are statically allocated inside parseline() and will be 
 *             overwritten the next time this function is invoked.
 */
int parseline(const char *cmdline, struct cmdline_tokens *tok) 
{

    static char array[MAXLINE];          /* holds local copy of command line */
    const char delims[10] = " \t\r\n";   /* argument delimiters (white-space) */
    char *buf = array;                   /* ptr that traverses command line */
    char *next;                          /* ptr to the end of the current arg */
    char *endbuf;                        /* ptr to end of cmdline string */
    int is_bg;                           /* background job? */

    int parsing_state;                   /* indicates if the next token is the
                                            input or output file */

    if (cmdline == NULL) {
        (void) fprintf(stderr, "Error: command line is NULL\n");
        return -1;
    }

    (void) strncpy(buf, cmdline, MAXLINE);
    endbuf = buf + strlen(buf);

    tok->infile = NULL;
    tok->outfile = NULL;

    /* Build the argv list */
    parsing_state = ST_NORMAL;
    tok->argc = 0;

    while (buf < endbuf) {
        /* Skip the white-spaces */
        buf += strspn (buf, delims);
        if (buf >= endbuf) break;

        /* Check for I/O redirection specifiers */
        if (*buf == '<') {
            if (tok->infile) {
                (void) fprintf(stderr, "Error: Ambiguous I/O redirection\n");
                return -1;
            }
            parsing_state |= ST_INFILE;
            buf++;
            continue;
        }
        if (*buf == '>') {
            if (tok->outfile) {
                (void) fprintf(stderr, "Error: Ambiguous I/O redirection\n");
                return -1;
            }
            parsing_state |= ST_OUTFILE;
            buf ++;
            continue;
        }

        if (*buf == '\'' || *buf == '\"') {
            /* Detect quoted tokens */
            buf++;
            next = strchr (buf, *(buf-1));
        } else {
            /* Find next delimiter */
            next = buf + strcspn (buf, delims);
        }
        
        if (next == NULL) {
            /* Returned by strchr(); this means that the closing
               quote was not found. */
            (void) fprintf (stderr, "Error: unmatched %c.\n", *(buf-1));
            return -1;
        }

        /* Terminate the token */
        *next = '\0';

        /* Record the token as either the next argument or the i/o file */
        switch (parsing_state) {
        case ST_NORMAL:
            tok->argv[tok->argc++] = buf;
            break;
        case ST_INFILE:
            tok->infile = buf;
            break;
        case ST_OUTFILE:
            tok->outfile = buf;
            break;
        default:
            (void) fprintf(stderr, "Error: Ambiguous I/O redirection\n");
            return -1;
        }
        parsing_state = ST_NORMAL;

        /* Check if argv is full */
        if (tok->argc >= MAXARGS-1) break;

        buf = next + 1;
    }

    if (parsing_state != ST_NORMAL) {
        (void) fprintf(stderr,
                       "Error: must provide file name for redirection\n");
        return -1;
    }

    /* The argument list must end with a NULL pointer */
    tok->argv[tok->argc] = NULL;

    if (tok->argc == 0)  /* ignore blank line */
        return 1;

    if (!strcmp(tok->argv[0], "quit")) {                 /* quit command */
        tok->builtins = BUILTIN_QUIT;
    } else if (!strcmp(tok->argv[0], "jobs")) {          /* jobs command */
        tok->builtins = BUILTIN_JOBS;
    } else if (!strcmp(tok->argv[0], "bg")) {            /* bg command */
        tok->builtins = BUILTIN_BG;
    } else if (!strcmp(tok->argv[0], "fg")) {            /* fg command */
        tok->builtins = BUILTIN_FG;
    } else {
        tok->builtins = BUILTIN_NONE;
    }

    /* Should the job run in the background? */
    if ((is_bg = (*tok->argv[tok->argc-1] == '&')) != 0)
        tok->argv[--tok->argc] = NULL;

    return is_bg;
}


/*****************
 * Signal handlers
 *****************/

/* 
 * sigchld_handler - The kernel sends a SIGCHLD to the shell whenever
 *     a child job terminates (becomes a zombie), or stops because it
 *     received a SIGSTOP, SIGTSTP, SIGTTIN or SIGTTOU signal. The 
 *     handler reaps all available zombie children, but doesn't wait 
 *     for any other currently running children to terminate.  
 */
void sigchld_handler(int sig) {

    int status;
    pid_t pid;
    int olderrno = errno; // store errno in handler

    /*
     * Reap child with the pid if the child is stopped or terminated
     * If a child is terminated normally, delete the child from the job list
     * If a child is stopped by a signal, set the job status as stopped
     * If a child is terminated by a signal that was not caught, delete the child from the job list
     */
    while ((pid = waitpid(-1, &status, WNOHANG|WUNTRACED)) > 0) {

        if (WIFEXITED(status)) {

            deletejob(job_list, pid); // the child ternimated normally

        } else if (WIFSTOPPED(status)) {

            /*
             * Avoid printf() in handler, use helper function to write the output to stdout
             */
            sio_puts("Job [");
            sio_putl(pid2jid(pid));
            sio_puts("] (");
            sio_putl(pid);
            sio_puts(") stopped by signal ");
            sio_putl(WSTOPSIG(status));
            sio_puts("\n");

            getjobpid(job_list, pid) -> state = ST; // the child was stopped by a signal 

        } else if (WIFSIGNALED(status)) { // the child was terminated by a signal that was not caught

            /*
             * Avoid printf() in handler, use helper function to write the output to stdout
             */
            sio_puts("Job [");
            sio_putl(pid2jid(pid));
            sio_puts("] (");
            sio_putl(pid);
            sio_puts(") terminated by signal ");
            sio_putl(WTERMSIG(status));
            sio_puts("\n");

			deletejob(job_list, pid);

        }

    }

    errno = olderrno;
    return;
}

/* 
 * sigint_handler - The kernel sends a SIGINT to the shell whenver the
 *    user types ctrl-c at the keyboard.  Catch it and send it along
 *    to the foreground job.  
 */
void sigint_handler(int sig) {

    pid_t pid = fgpid(job_list); // get the pid of the foreground job
    int olderrno = errno; // store errno in handler

    if (pid != 0) {

        kill(-pid, SIGINT); // send signals to the process group and terminate all the process

    }

    errno = olderrno;
    return;
}

/*
 * sigtstp_handler - The kernel sends a SIGTSTP to the shell whenever
 *     the user types ctrl-z at the keyboard. Catch it and suspend the
 *     foreground job by sending it a SIGTSTP.  
 */
void sigtstp_handler(int sig) {

    pid_t pid = fgpid(job_list); // get the pid of the foreground job
    int olderrno = errno; // store errno in handler

    if (pid != 0) {

        kill(-pid, SIGTSTP); // send signals to the process group and stop all the process

    }

    errno = olderrno;
    return;
}

/*********************
 * End signal handlers
 *********************/

/***********************************************
 * Helper routines that manipulate the job list
 **********************************************/

/* clearjob - Clear the entries in a job struct */
void clearjob(struct job_t *job) {
    job->pid = 0;
    job->jid = 0;
    job->state = UNDEF;
    job->cmdline[0] = '\0';
}

/* initjobs - Initialize the job list */
void initjobs(struct job_t *job_list) {
    int i;

    for (i = 0; i < MAXJOBS; i++)
        clearjob(&job_list[i]);
}

/* maxjid - Returns largest allocated job ID */
int maxjid(struct job_t *job_list) 
{
    int i, max=0;

    for (i = 0; i < MAXJOBS; i++)
        if (job_list[i].jid > max)
            max = job_list[i].jid;
    return max;
}

/* addjob - Add a job to the job list */
int addjob(struct job_t *job_list, pid_t pid, int state, char *cmdline) 
{
    int i;

    if (pid < 1)
        return 0;

    for (i = 0; i < MAXJOBS; i++) {
        if (job_list[i].pid == 0) {
            job_list[i].pid = pid;
            job_list[i].state = state;
            job_list[i].jid = nextjid++;
            if (nextjid > MAXJOBS)
                nextjid = 1;
            strcpy(job_list[i].cmdline, cmdline);
            if(verbose){
                printf("Added job [%d] %d %s\n",
                       job_list[i].jid,
                       job_list[i].pid,
                       job_list[i].cmdline);
            }
            return 1;
        }
    }
    printf("Tried to create too many jobs\n");
    return 0;
}

/* deletejob - Delete a job whose PID=pid from the job list */
int deletejob(struct job_t *job_list, pid_t pid) 
{
    int i;
	//fprintf(stderr, "try to delete job:%d\n", pid);
    if (pid < 1)
        return 0;

    for (i = 0; i < MAXJOBS; i++) {
        if (job_list[i].pid == pid) {
            clearjob(&job_list[i]);
            nextjid = maxjid(job_list)+1;
            return 1;
        }
    }
    return 0;
}

/* fgpid - Return PID of current foreground job, 0 if no such job */
pid_t fgpid(struct job_t *job_list) {
    int i;

    for (i = 0; i < MAXJOBS; i++) {
        if (job_list[i].state == FG) {
			//fprintf(stderr, "current foreground job:%d\n", job_list[i].pid);
            return job_list[i].pid;
		}
	}
    return 0;
}

/* getjobpid  - Find a job (by PID) on the job list */
struct job_t *getjobpid(struct job_t *job_list, pid_t pid) {
    int i;

    if (pid < 1)
        return NULL;
    for (i = 0; i < MAXJOBS; i++)
        if (job_list[i].pid == pid)
            return &job_list[i];
    return NULL;
}

/* getjobjid  - Find a job (by JID) on the job list */
struct job_t *getjobjid(struct job_t *job_list, int jid) 
{
    int i;

    if (jid < 1)
        return NULL;
    for (i = 0; i < MAXJOBS; i++)
        if (job_list[i].jid == jid)
            return &job_list[i];
    return NULL;
}

/* pid2jid - Map process ID to job ID */
int pid2jid(pid_t pid) 
{
    int i;

    if (pid < 1)
        return 0;
    for (i = 0; i < MAXJOBS; i++)
        if (job_list[i].pid == pid) {
            return job_list[i].jid;
        }
    return 0;
}

/* listjobs - Print the job list */
void listjobs(struct job_t *job_list, int output_fd) 
{
    int i;
    char buf[MAXLINE];

    for (i = 0; i < MAXJOBS; i++) {
        memset(buf, '\0', MAXLINE);
        if (job_list[i].pid != 0) {
            sprintf(buf, "[%d] (%d) ", job_list[i].jid, job_list[i].pid);
            if(write(output_fd, buf, strlen(buf)) < 0) {
                //fprintf(stderr, "Error writing to output file\n");
                exit(1);
            }
            memset(buf, '\0', MAXLINE);
            switch (job_list[i].state) {
            case BG:
                sprintf(buf, "Running    ");
                break;
            case FG:
                sprintf(buf, "Foreground ");
                break;
            case ST:
                sprintf(buf, "Stopped    ");
                break;
            default:
                sprintf(buf, "listjobs: Internal error: job[%d].state=%d ",
                        i, job_list[i].state);
            }
            if(write(output_fd, buf, strlen(buf)) < 0) {
                //fprintf(stderr, "Error writing to output file\n");
                exit(1);
            }
            memset(buf, '\0', MAXLINE);
            sprintf(buf, "%s\n", job_list[i].cmdline);
            if(write(output_fd, buf, strlen(buf)) < 0) {
                //fprintf(stderr, "Error writing to output file\n");
                exit(1);
            }
        }
    }
}
/******************************
 * end job list helper routines
 ******************************/


/***********************
 * Other helper routines
 ***********************/

/*
 * usage - print a help message
 */
void usage(void) 
{
    printf("Usage: shell [-hvp]\n");
    printf("   -h   print this message\n");
    printf("   -v   print additional diagnostic information\n");
    printf("   -p   do not emit a command prompt\n");
    exit(1);
}

/*
 * unix_error - unix-style error routine
 */
void unix_error(char *msg)
{
    fprintf(stdout, "%s: %s\n", msg, strerror(errno));
    exit(1);
}

/*
 * app_error - application-style error routine
 */
void app_error(char *msg)
{
    fprintf(stdout, "%s\n", msg);
    exit(1);
}

/*
 * Signal - wrapper for the sigaction function
 */
handler_t *Signal(int signum, handler_t *handler) 
{
    struct sigaction action, old_action;

    action.sa_handler = handler;  
    sigemptyset(&action.sa_mask); /* block sigs of type being handled */
    action.sa_flags = SA_RESTART; /* restart syscalls if possible */

    if (sigaction(signum, &action, &old_action) < 0)
        unix_error("Signal error");
    return (old_action.sa_handler);
}

/*
 * sigquit_handler - The driver program can gracefully terminate the
 *    child shell by sending it a SIGQUIT signal.
 */
void sigquit_handler(int sig) 
{
    printf("Terminating after receipt of SIGQUIT signal\n");
    exit(1);
}

/* $begin sioprivate */
/* sio_reverse - Reverse a string (from K&R) */
static void sio_reverse(char s[])
{
    int c, i, j;

    for (i = 0, j = strlen(s)-1; i < j; i++, j--) {
        c = s[i];
        s[i] = s[j];
        s[j] = c;
    }
}

/* sio_ltoa - Convert long to base b string (from K&R) */
static void sio_ltoa(long v, char s[], int b) 
{
    int c, i = 0;
    
    do {  
        s[i++] = ((c = (v % b)) < 10)  ?  c + '0' : c - 10 + 'a';
    } while ((v /= b) > 0);
    s[i] = '\0';
    sio_reverse(s);
}

/* sio_strlen - Return length of string (from K&R) */
static size_t sio_strlen(char s[])
{
    int i = 0;

    while (s[i] != '\0')
        ++i;
    return i;
}
/* $end sioprivate */

/* Public Sio functions */
/* $begin siopublic */

ssize_t sio_puts(char s[]) /* Put string */
{
    return write(STDOUT_FILENO, s, sio_strlen(s)); //line:csapp:siostrlen
}

ssize_t sio_putl(long v) /* Put long */
{
    char s[128];
    
    sio_ltoa(v, s, 10); /* Based on K&R itoa() */  //line:csapp:sioltoa
    return sio_puts(s);
}

void sio_error(char s[]) /* Put error message and exit */
{
    sio_puts(s);
    _exit(1);                                      //line:csapp:sioexit
}

