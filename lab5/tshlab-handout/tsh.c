/* 
 * tsh - A tiny shell program with job control
 * 
 * I copied some functions from csapp.c and added a bunch of my own.
 * And I've added delimiters around them and documented them.
 * 
 * What the shell does is after receiving a command, if it's a built-in,
 * then handle accordingly. If it's not a built-in, try launching a child
 * process. If it's fg, wait till the fg job exits fg; otherwise, just
 * leave the job running, and get ready to process the next command.
 * 
 * IO redirections are handled accordingly during the process.
 * 
 * Name: Liruoyang Yu 
 * Andrew ID: liruoyay
 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <string.h>
#include <errno.h>

/* Misc manifest constants */
#define MAXLINE    1024   /* max line size */
#define MAXARGS     128   /* max args on a command line */
#define MAXJOBS      16   /* max jobs at any point in time */
#define MAXJID    1<<16   /* max job ID */
#define MAXBUF     1024   /* max buffer size */

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

/*****************************************************************
 ************************ start my globals ***********************
 *****************************************************************/

volatile sig_atomic_t fg_pid; /* the terminated fg job pid */

/*****************************************************************
 ************************ end my globals *************************
 *****************************************************************/


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

/*****************************************************************
 **************** start functions from csapp.c *******************
 *****************************************************************/
ssize_t Sio_puts(char s[]);
ssize_t Sio_putl(long v);
void Sio_error(char s[]);
handler_t *Signal(int signum, handler_t *handler);
void Sigprocmask(int how, const sigset_t *set, sigset_t *oldset);
void Sigemptyset(sigset_t *set);
void Sigfillset(sigset_t *set);
void Sigaddset(sigset_t *set, int signum);
void Sigdelset(sigset_t *set, int signum);
int Sigismember(const sigset_t *set, int signum);
int Sigsuspend(const sigset_t *set);
int Open(const char *pathname, int flags, mode_t mode);
void Close(int fd);
int Dup2(int fd1, int fd2);
/*****************************************************************
 **************** end functions from csapp.c *******************
 *****************************************************************/

/*****************************************************************
 **************** start my own helper routines *******************
 *****************************************************************/
sigset_t block_sigs();
int handle_builtin(struct cmdline_tokens* tok);
void handle_jobs(struct cmdline_tokens* tok);
void handle_bg(struct cmdline_tokens* tok);
void handle_fg(struct cmdline_tokens* tok);
struct job_t* get_job_from_argv(struct cmdline_tokens* tok);
void waitfg(sigset_t sig_prev);
/*****************************************************************
 ***************** end my own helper routines ********************
 *****************************************************************/
/*
 * main - The shell's main routine 
 */
int 
main(int argc, char **argv) 
{
    char c;
    char cmdline[MAXLINE];    /* cmdline for fgets */
    int emit_prompt = 1; /* emit prompt (default) */

    /* Redirect stderr to stdout (so that driver will get all output
     * on the pipe connected to stdout) */
    Dup2(1, 2);

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
        eval(cmdline);
        
        fflush(stdout);
        fflush(stdout);
    } 
    
    exit(0); /* control never reaches here */
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
void 
eval(char *cmdline) 
{
    int bg;              /* should the job run in bg or fg? */
    struct cmdline_tokens tok;

    /* Parse command line */
    bg = parseline(cmdline, &tok); 

    if (bg == -1) { /* parsing error */
        perror("Error occurred whe parsing your command.");
        return;
    }
    if (tok.argv[0] == NULL) { /* ignore empty lines */
        return;
    }
    
    /*************************
     * Handling command start
     *************************/
    // If the command is built-in and is handled, then return.
    if (handle_builtin(&tok)) {
        return;
    }

    /*************************
     * New child process
     *************************/
    pid_t c_pid;         /* child process pid */
    sigset_t sig_prev;   /* sigmask before blocking */
    int ret;             /* used for receiving some return values */
    
    // block signals for waiting on fg job
    sig_prev = block_sigs();
    // launch child process
    c_pid = fork();
    
    /*********************************************************
     *********************** start child *********************
     *********************************************************/
    if (c_pid == 0) {
        // restore original sigmask
        Sigprocmask(SIG_SETMASK, &sig_prev, NULL);
        // put the child in its own process group
        setpgid(0, 0);
        /***********************************
         * start process IO redirections
         ***********************************/
        if (tok.infile) {
            int fd = Open(tok.infile, O_RDONLY, S_IRWXU);
            Dup2(fd, STDIN_FILENO);
            Close(fd);
        }
        if (tok.outfile) {
            int fd = Open(tok.outfile, O_WRONLY, S_IRWXU);
            Dup2(fd, STDOUT_FILENO);
            Close(fd);
        }
        /***********************************
         * end process IO redirections
         ***********************************/
        // launch the program specified by the cmd
        execve(tok.argv[0], tok.argv, environ);
        // execve failed
        printf("%s: Command not found\n", tok.argv[0]);
        exit(0);
    }
    /*********************************************************
     *********************** end child ***********************
     *********************************************************/
    
    // if foreground job
    if (!bg) {
        // add the job to job list
        ret = addjob(job_list, c_pid, FG, cmdline);
        if (!ret) {
            app_error("Adding job error");
        }
        // wait on fg job
        waitfg(sig_prev);
    }
    // if background job
    else {
        // add the job to job list
        addjob(job_list, c_pid, BG, cmdline);
        struct job_t* job = getjobpid(job_list, c_pid);
        // job's not been added correctly
        if (!job) {
            app_error("Adding job error");
        }
        printf("[%d] (%d) %s\n", job->jid, job->pid, job->cmdline);
        // unblock SIGCHLD
        Sigprocmask(SIG_SETMASK, &sig_prev, NULL);
    }
    return;
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
int 
parseline(const char *cmdline, struct cmdline_tokens *tok) 
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
void 
sigchld_handler(int sig) 
{
    int old_errno = errno;
    int status;
    pid_t pid;
    if (verbose) {
        Sio_puts("sigchld handler: entering\n");
    }
    while ((pid = waitpid(-1, &status, WNOHANG|WUNTRACED)) > 0) {
        struct job_t* job = getjobpid(job_list, pid);
        // if the child hasn't been added to the job list. Should never happen.
        if (!job) {
            continue;
        }
        /** start examine the child terminating status **/
        int jid = job->jid;   /* job jid */
        int is_stop = 0;      /* if the job is stopped */
        if (WIFEXITED(status)) {  /* job terminated naturally */
            if (verbose) {
                Sio_puts("sigchld handler: job [");
                Sio_putl(jid);
                Sio_puts("] (");
                Sio_putl(pid);
                Sio_puts(") terminates OK with status ");
                Sio_putl(WEXITSTATUS(status));
                Sio_puts("\n");
            }
        }
        else if (WIFSIGNALED(status)) {  /* job terminated by signal */
            Sio_puts("Job [");
            Sio_putl(jid);
            Sio_puts("] (");
            Sio_putl(pid);
            Sio_puts(") terminated by signal ");
            Sio_putl(WTERMSIG(status));
            Sio_puts("\n");
        }
        else if (WIFSTOPPED(status)) {  /* job stopped */
            Sio_puts("Job [");
            Sio_putl(jid);
            Sio_puts("] (");
            Sio_putl(pid);
            Sio_puts(") stopped by signal ");
            Sio_putl(WSTOPSIG(status));
            Sio_puts("\n");
            is_stop = 1;
        }
        /** end examine the child terminating status **/
        
        // if the terminated/stopped child is a fg job
        if (job->state == FG) {
            fg_pid = job->pid;
            // if the job is stopped
            if (is_stop) {
                job->state = ST;
            }
        }
        // if the job is terminated, then delete the job
        if (!is_stop && deletejob(job_list, pid)) {
            if (verbose) {
                Sio_puts("sigchld handler: job [");
                Sio_putl(jid);
                Sio_puts("] (");
                Sio_putl(pid);
                Sio_puts(") deleted\n");
            }
        }
        else {
            // control should never reach here
            continue;
        }
    }
    if (verbose) {
        Sio_puts("sigchld handler: exiting\n");
    }
    errno = old_errno;
}

/* 
 * sigint_handler - The kernel sends a SIGINT to the shell whenver the
 *    user types ctrl-c at the keyboard.  Catch it and send it along
 *    to the foreground job.  
 */
void 
sigint_handler(int sig) 
{
    int old_errno = errno;
    // get the foreground job pid
    pid_t fg_pid = fgpid(job_list);
    if (verbose) {
        Sio_puts("sigint handler: entering\n");
        Sio_puts("sigint handler: sending SIGINT to (");
        Sio_putl(fg_pid);
        Sio_puts(")\n");
    }
    // signal SIGINT to the fg process group
    if (killpg(fg_pid, SIGINT) < 0) {
        Sio_error("sigint handler: sending SIGINT ERROR");
    }
    if (verbose) {
        Sio_puts("sigint handler: exiting\n");
    }
    errno = old_errno;
}

/*
 * sigtstp_handler - The kernel sends a SIGTSTP to the shell whenever
 *     the user types ctrl-z at the keyboard. Catch it and suspend the
 *     foreground job by sending it a SIGTSTP.  
 */
void 
sigtstp_handler(int sig) 
{
    int old_errno = errno;
    // get the foreground job pid
    pid_t fg_pid = fgpid(job_list);
    if (verbose) {
        Sio_puts("sigtstp handler: entering\n");
        Sio_puts("sigtstp handler: sending SIGTSTP to (");
        Sio_putl(fg_pid);
        Sio_puts(")\n");
    }
    // signal SIGTSTP to the fg process group
    if (killpg(fg_pid, SIGTSTP) < 0) {
        Sio_error("sigtstp handler: sending SIGTSTP ERROR");
    }
    if (verbose) {
        Sio_puts("sigtstp handler: exiting\n");
    }
    errno = old_errno;
}

/*********************
 * End signal handlers
 *********************/

/***********************************************
 * Helper routines that manipulate the job list
 **********************************************/

/* clearjob - Clear the entries in a job struct */
void 
clearjob(struct job_t *job) {
    job->pid = 0;
    job->jid = 0;
    job->state = UNDEF;
    job->cmdline[0] = '\0';
}

/* initjobs - Initialize the job list */
void 
initjobs(struct job_t *job_list) {
    int i;

    for (i = 0; i < MAXJOBS; i++)
        clearjob(&job_list[i]);
}

/* maxjid - Returns largest allocated job ID */
int 
maxjid(struct job_t *job_list) 
{
    int i, max=0;

    for (i = 0; i < MAXJOBS; i++)
        if (job_list[i].jid > max)
            max = job_list[i].jid;
    return max;
}

/* addjob - Add a job to the job list */
int 
addjob(struct job_t *job_list, pid_t pid, int state, char *cmdline) 
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
int 
deletejob(struct job_t *job_list, pid_t pid) 
{
    int i;

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
pid_t 
fgpid(struct job_t *job_list) {
    int i;

    for (i = 0; i < MAXJOBS; i++)
        if (job_list[i].state == FG)
            return job_list[i].pid;
    return 0;
}

/* getjobpid  - Find a job (by PID) on the job list */
struct job_t 
*getjobpid(struct job_t *job_list, pid_t pid) {
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
int 
pid2jid(pid_t pid) 
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
void 
listjobs(struct job_t *job_list, int output_fd) 
{
    int i;
    char buf[MAXLINE];

    for (i = 0; i < MAXJOBS; i++) {
        memset(buf, '\0', MAXLINE);
        if (job_list[i].pid != 0) {
            sprintf(buf, "[%d] (%d) ", job_list[i].jid, job_list[i].pid);
            if(write(output_fd, buf, strlen(buf)) < 0) {
                fprintf(stderr, "Error writing to output file\n");
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
                fprintf(stderr, "Error writing to output file\n");
                exit(1);
            }
            memset(buf, '\0', MAXLINE);
            sprintf(buf, "%s\n", job_list[i].cmdline);
            if(write(output_fd, buf, strlen(buf)) < 0) {
                fprintf(stderr, "Error writing to output file\n");
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
void 
usage(void) 
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
void 
unix_error(char *msg)
{
    fprintf(stdout, "%s: %s\n", msg, strerror(errno));
    exit(1);
}

/*
 * app_error - application-style error routine
 */
void 
app_error(char *msg)
{
    fprintf(stdout, "%s\n", msg);
    exit(1);
}

/*
 * Signal - wrapper for the sigaction function
 */
handler_t 
*Signal(int signum, handler_t *handler) 
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
void 
sigquit_handler(int sig) 
{
    printf("Terminating after receipt of SIGQUIT signal\n");
    exit(1);
}

/*****************************************************************
 **************** start functions from csapp.c *******************
 *****************************************************************/

/*************************************************************
 * The Sio (Signal-safe I/O) package - simple reentrant output
 * functions that are safe for signal handlers.
 *************************************************************/

/* Private sio functions */

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
/* $end siopublic */

/*******************************
 * Wrappers for the SIO routines
 ******************************/
ssize_t Sio_putl(long v)
{
    ssize_t n;
  
    if ((n = sio_putl(v)) < 0)
    sio_error("Sio_putl error");
    return n;
}

ssize_t Sio_puts(char s[])
{
    ssize_t n;
  
    if ((n = sio_puts(s)) < 0)
    sio_error("Sio_puts error");
    return n;
}

void Sio_error(char s[])
{
    sio_error(s);
}

/************************************
 * Wrappers for Unix signal functions 
 ***********************************/
void Sigprocmask(int how, const sigset_t *set, sigset_t *oldset)
{
    if (sigprocmask(how, set, oldset) < 0)
    unix_error("Sigprocmask error");
    return;
}

void Sigemptyset(sigset_t *set)
{
    if (sigemptyset(set) < 0)
    unix_error("Sigemptyset error");
    return;
}

void Sigaddset(sigset_t *set, int signum)
{
    if (sigaddset(set, signum) < 0)
    unix_error("Sigaddset error");
    return;
}

int Sigsuspend(const sigset_t *set)
{
    int rc = sigsuspend(set); /* always returns -1 */
    if (errno != EINTR)
        unix_error("Sigsuspend error");
    return rc;
}

/********************************
 * Wrappers for Unix I/O routines
 ********************************/

int Open(const char *pathname, int flags, mode_t mode) 
{
    int rc;

    if ((rc = open(pathname, flags, mode))  < 0)
	unix_error("Open error");
    return rc;
}

void Close(int fd) 
{
    int rc;

    if ((rc = close(fd)) < 0)
	unix_error("Close error");
}

int Dup2(int fd1, int fd2) 
{
    int rc;

    if ((rc = dup2(fd1, fd2)) < 0)
	unix_error("Dup2 error");
    return rc;
}

/*****************************************************************
 **************** end functions from csapp.c *******************
 *****************************************************************/

/*****************************************************************
 **************** start my own helper routines *******************
 *****************************************************************/

/* Block SIGCHID, SIGINT, SIGTSTP and return the original sigmask */
sigset_t block_sigs() {
    sigset_t sig_prev;
    sigset_t sig_set;
    
    Sigemptyset(&sig_set);
    Sigaddset(&sig_set, SIGCHLD);
    Sigaddset(&sig_set, SIGINT);
    Sigaddset(&sig_set, SIGTSTP);
    
    // set blocking
    Sigprocmask(SIG_BLOCK, &sig_set, &sig_prev);
    return sig_prev;
}

/* 
 * Handler for built-in commands.
 * Return 1 if the command is built-in and handled, 0 otherwise.
 */
int handle_builtin(struct cmdline_tokens* tok) {
    switch (tok->builtins) {
        case BUILTIN_QUIT:
            exit(0);
        case BUILTIN_BG:
            handle_bg(tok);
            return 1;
        case BUILTIN_FG:
            handle_fg(tok);
            return 1;
        case BUILTIN_JOBS:
            handle_jobs(tok);
            return 1;
        default:
            return 0;
    }
}

/*
 * Handle jobs command 
 */
void handle_jobs(struct cmdline_tokens* tok) {
    // write to outfile if outfile provided
    if (tok->outfile) {
        int fd = Open(tok->outfile, O_WRONLY, S_IRWXU);
        listjobs(job_list, fd);
        Close(fd);
    }
    // write to stdout
    else {
        listjobs(job_list, STDOUT_FILENO);
    }
}

/*
 * Handle bg command 
 */
void handle_bg(struct cmdline_tokens* tok) {
    // not enough arguments
    if (tok->argc < 2) {
        printf("bg command requires PID or %%jobid argument");
        return;
    }
    // get the regarding job
    struct job_t* job = get_job_from_argv(tok);
    // no job found by the pid/jid
    if (!job) {
        printf("%s: no such job", tok->argv[1]);
        return;
    }
    // resume the job as a bg job
    job->state = BG;
    printf("[%d] (%d) %s", job->jid, job->pid, job->cmdline);
    // signal all processes in the group
    killpg(job->pid, SIGCONT);
}

/*
 * Handle fg command 
 */
void handle_fg(struct cmdline_tokens* tok) {
    // not enough arguments
    if (tok->argc < 2) {
        printf("fg command requires PID or %%jobid argument");
        return;
    }
    // get the regarding job
    struct job_t* job = get_job_from_argv(tok);
    // no job found by the pid/jid
    if (!job) {
        printf("%s: no such job", tok->argv[1]);
        return;
    }
    // resume the job as a fg job
    job->state = FG;
    // block the signals for waiting on the fg job
    sigset_t sig_prev = block_sigs();
    // signal all processes in the group
    killpg(job->pid, SIGCONT);
    // wait on the fg job
    waitfg(sig_prev);
}

/*
 * Helper function for bg/fg handlers.
 * Return the job found by pid/jid.
 * Return NULL if the job hasn't been found.
 */
struct job_t* get_job_from_argv(struct cmdline_tokens* tok) {
    struct job_t* job = NULL;
    // got jid
    if (tok->argv[1][0] == '%') {
        int jid;
        // parse the jid
        if (sscanf(tok->argv[1], "%%%d", &jid) == EOF) {
            unix_error("parsing bg %arg");
        }
        job = getjobjid(job_list, jid);
    }
    // got pid
    else {
        pid_t pid = atoi(tok->argv[1]);
        job = getjobpid(job_list, pid);
    }
    return job;
}

/*
 * Perform the waiting for the fg job to exit fg.
 */
void waitfg(sigset_t sig_prev) {
    fg_pid = 0;
    // wait on the foreground job
    while (!fg_pid) {
        Sigsuspend(&sig_prev);
    }
    // unblock the blocked signals
    Sigprocmask(SIG_SETMASK, &sig_prev, NULL);
    if (verbose) {
        printf("fg job (%d) is no long in the fg\n", fg_pid);
    }
}
/*****************************************************************
 ***************** end my own helper routines ********************
 *****************************************************************/
