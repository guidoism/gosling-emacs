/* These procedures hope to give EMACS a facility for dealing with multiple
   processes in a reasonable way */

/*		Copyright (c) 1981 Carl Ebeling		*/

/* Modified 8-Sept-81 Jeffrey Mogul (JCM) at Stanford
 *	- removed RstDsp() after failure to open mpx file; this
 *	  was leading to nasty infinite loop.  Probably this should
 *	  be done better and in other situations as well.
 */

/* Modified by Chris Torek (ACT) at umcp-cs to answer more ioctls */

/* Modified 17-Jul-82 ACT - rewrote the emacs-share to automatically
 * create buffers for process that access the multiplexed file.  Also
 * changed the definition of '-s' from 'use share' to 'don't use share'
 * with the default being 'use share'.
 */

/* ACT 22-Jul-82: added variable 'emacs-share' (I suspect this used to
 * exist!), default 1, which lets/prevents opens of the share file
 */

/* ACT 21-Oct-1982 adding code to let csh do job control.... */

/* Incorporate Umcp-Cs features to get a really nice version of mchan.c 
   for 4.1bsd */

/* Original changes for 4.2bsd (c) 1982 William N. Joy and Regents of UC */

/* More changes for 4.1aBSD by Spencer Thomas of Utah-Cs */

/* Still more changes for 4.1aBSD by Marshall Rose of UCI */

/* Changes for 4.1cBSD by Chris Kent of DecWRL */

#include "config.h"
#include <signal.h>
#include <errno.h>
#include <sys/param.h>
#include <sys/wait.h>
#include <sgtty.h>
#include <time.h>
#include <sys/time.h>
#include <sys/types.h>
#include "window.h"
#include "buffer.h"
#include "keyboard.h"
#include "mlisp.h"
#include "macros.h"
#include "mchan.h"

char *malloc();

typedef long * waddr_t;


#define ChunkSize 500		/* amount to truncate when buffer overflows */
static	PopUpUnexpected;	/* True iff unexpected opens pop up windows */
static	EmacsShare;		/* Share flag, true iff opens allowed */
static struct BoundName
	*UnexpectedProc;	/* What to do with unexpected procs */
static struct BoundName
	*UnexpectedSent;	/* What to do when unexpected procs exit */
static	ProcessBufferSize;	/* Maximum size for process buffer */

int     sel_ichans;		/* input channels */
int     sel_ochans;		/* blocked output channels */

static	struct sgttyb mysgttyb;
static	struct tchars mytchars;
static	struct ltchars myltchars;
static	int mylmode;


struct channel_blk
	stdin_chan;

int	child_changed;		/* Flag when a child process changes status */
struct VariableName
	*MPX_process;
struct channel_blk
	*MPX_chan;

char   *SIG_names[] = {		/* descriptive (?) names of signals */
    "",
    "Hangup",
    "Interrupt",
    "Quit",
    "Illegal instruction",
    "Trace/BPT trap",
    "IOT trap",
    "EMT trap",
    "Floating exception",
    "Killed",
    "Bus error",
    "Segmentation fault",
    "Bad system call",
    "Broken pipe",
    "Alarm clock",
    "Terminated",
    "Urgent I/O condition",
    "Stopped (signal)",
    "Stopped",
    "Continued",		/* */
    "Child exited",		/* */
    "Stopped (tty input)",	/* */
    "Stopped (tty output)",	/* */
    "Tty input interrupt",	/* */
    "Cputime limit exceeded",
    "Filesize limit exceeded",
    "Signal 26",
    "Signal 27",
    "Signal 28",
    "Signal 29",
    "Signal 30",
    "Signal 31",
    "Signal 32"
};

static char *KillNames[] = {	/* names used for signal-to-process */
    "",     "HUP",  "INT",  "QUIT", "ILL",  "TRAP", "IOT",  "EMT",
    "FPE",  "KILL", "BUS",  "SEGV", "SYS",  "PIPE", "ALRM", "TERM",
    "URG",
	    "STOP", "TSTP", "CONT", "CHLD", "TTIN", "TTOU",
    "IO",
    "XCPU", "XFSZ"
};

char *
pty(ptyv)
int *ptyv;
{
#include <sys/stat.h>
	struct stat stb;
	static char name[24];
	int on = 1, i, fd;

	strcpy(name, "/dev/ptypX");
	for (;;) {
		name[strlen("/dev/ptyp")] = '0';
		if (stat(name, &stb) < 0)
			return (0);
		for (i = 0; i < 16; i++) {
			name[strlen("/dev/ptyp")] = "0123456789abcdef"[i];
			*ptyv = open(name, 2);
			if (*ptyv >= 0) {
                                name[strlen("/dev/")] = 't';
                                if ( (fd = open(name, 2)) >=0) {
					/* If the following statement is included,
			 		* then a 0 length record is EOT, but no other
			 		* control characters can be sent down the pty
			 		* (e.g., ^S/^Q, ^O, etc.).  If it is not
			 		* included, then sending ^D down the pty-pipe
			 		* makes a pretty good EOF.
			 		*/
/*					ioctl(*ptyv,TIOCREMOTE,(waddr_t)&on);/*for EOT */
					ioctl(*ptyv, FIONBIO, (waddr_t)&on);
					close(fd);
					return (name);
                                } else {
                                        name[strlen("/dev/")] = 'p';
                                        close(*ptyv);
                                }
			}
		}
		name[strlen("/dev/pty")]++;
	}
}

/* Start up a subprocess with its standard input and output connected to 
   a channel on the mpx file.  Also set its process group so we can kill it
   and set up its process block.  The process block is assumed to be pointed
   to by current_process. */

create_process (command)
register char  *command;
{
    index_t channel;
    int     pgrp,
	    len,
	    ld;
    char   *ptyname;
    register	pid;
    extern char *shell ();
    extern UseCshOptionF;
    extern UseUsersShell;

    ptyname = pty (&channel);
    if (ptyname == 0) {
	error ("Can't get a pty");
	return (-1);
    }
    sel_ichans |= 1<<channel;

    sighold (SIGINT);
    if ((pid = vfork ()) < 0) {
	error ("Fork failed");
	close (channel);
	sel_ichans &= ~(1<<channel);
	return (-1);
    }

    if (pid == 0) {
	close (channel);
	sigrelse (SIGCHLD);
	setpgrp (0, getpid ());
	sigsys (SIGINT, SIG_DFL);
	sigsys (SIGQUIT, SIG_DFL);
	if ((ld = open ("/dev/tty", 2)) >= 0) {
	    ioctl (ld, TIOCNOTTY, (waddr_t)0);
	    close (ld);
	}
	close (2);
	if (open (ptyname, 2) < 0) {
	     write (1, "Can't open subprocess tty ", 26);
	     write (1, ptyname, strlen(ptyname));
	     _exit (1);
	}
	pgrp = getpid();
	ioctl (2, TIOCSPGRP, (waddr_t)&pgrp);
	close (0);
	close (1);
	dup (2);
	dup (2);
	ioctl (0, TIOCSETP, (waddr_t)&mysgttyb);
	ioctl (0, TIOCSETC, (waddr_t)&mytchars);
	ioctl (0, TIOCSLTC, (waddr_t)&myltchars);
	ioctl (0, TIOCLSET, (waddr_t)&mylmode);
	len = 0;			/* set page features to 0 */
	len = UseUsersShell;
	UseUsersShell = 1;
	ld = strcmp(shell(), "/bin/csh") ? OTTYDISC : NTTYDISC;
	ioctl (0, TIOCSETD, (waddr_t)&ld);
	UseUsersShell = len;
	execlp (shell (), shell (),
		UseUsersShell && UseCshOptionF ? "-cf" : "-c", command, 0);
	write (1, "Couldn't exec the shell\n", 24);
	_exit (1);
    }

    sigrelse (SIGINT);
    current_process -> p_name = command;
    current_process -> p_pid = pid;
    current_process -> p_gid = pid;
    current_process -> p_flag = RUNNING | CHANGED;
    child_changed++;
    current_process -> p_reason = 0;
    current_process -> p_chan.ch_index = channel;
    current_process -> p_chan.ch_ptr = NULL;
    current_process -> p_chan.ch_count = 0;
    current_process -> p_chan.ch_outrec.index = channel;
    current_process -> p_chan.ch_outrec.count = 0;
    current_process -> p_chan.ch_outrec.ccount = 0;
    return 0;
}

/* Process a signal from a child process and make the appropriate change in 
   the process block. Since signals are NOT queued, if two signals are
   received before this routine gets called, then only the first process in
   the process list will be handled.  We will try to get the MPX file stuff
   to help us out since it passes along signals from subprocesses.
*/
int	subproc_id;		/* The process id of a subprocess
				   started by the old subproc stuff.
				   We will zero it so they will know it
				   has finished */
child_sig () {
    register int    pid;
    union wait w;
    register struct process_blk *p;
    extern struct process_blk  *get_next_process ();

loop: 
    pid = wait3 (&w.w_status, WUNTRACED | WNOHANG, 0);
    if (pid <= 0) {
	if (errno == EINTR) {
	    errno = 0;
	    goto loop;
	}
	if (pid == -1) {
	    if (!active_process (current_process))
		current_process = get_next_process ();
	}
	return;
    }
    if (pid == subproc_id) {	/* It may not be our progeny */
	subproc_id = 0;		/* Take care of those subprocesses first 
				*/
	goto loop;
    }
    for (p = process_list; p != NULL; p = p -> next_process)
	if (pid == p -> p_pid)
	    break;
    if (p == NULL)
	goto loop;		/* We don't know who this is */

    if (WIFSTOPPED (w)) {
	p -> p_flag = STOPPED | CHANGED;
	p -> p_reason = w.w_stopsig;
    child_changed++;
    }
    else
	if (WIFEXITED (w)) {
	    p -> p_flag = EXITED | CHANGED;
	    child_changed++;
	    p -> p_reason = w.w_retcode;
	}
	else
	    if (WIFSIGNALED (w)) {
		p -> p_flag = SIGNALED | CHANGED;
		if (w.w_coredump)
		    p -> p_flag |= COREDUMPED;
		child_changed++;
		p -> p_reason = w.w_termsig;
	    }
    if (!active_process (current_process))
	current_process = get_next_process ();
    goto loop;
}


/* Find the process which is connected to buf_name */

struct process_blk *find_process (buf_name)
register char   *buf_name;
{
    register struct process_blk *p;

    if (buf_name == NULL)
	return (NULL);
    for (p = process_list; p != NULL; p = p -> next_process) {
	if (!active_process (p))
	    continue;
	if (strcmp (p -> p_chan.ch_buffer -> b_name, buf_name) == 0)
	    break;
    }
    return (p);
}

/* Get the first active process in the process list and assign to the current
   process */

struct process_blk *get_next_process () {
    register struct process_blk *p;

    for (p = process_list; p && !active_process (p); p = p -> next_process);
    return p;
}

/* This corresponds to the filbuf routine used by getchar.  This handles all 
   the input from the mpx file.  Input coming from the terminal is sent back
   to getchar() in the same manner as filbuf.  Control messages are sent to
   Take_msg for interpretation.  Normal input from other channels is routed
   to the correct buffer. */

static char cbuffer[BUFSIZ];	/* used for reading mpx file */
static int  mpx_count;		/* number of unprocessed characters in
				   buffer */

/* ARGSUSED */
fill_chan (chan, alrmtime)
register struct channel_blk *chan;
{
    int ichans, ochans, cc;
    register struct channel_blk *this_chan;
    register struct process_blk *p;
    struct timeval	timeout;

    if (alrmtime <= 0 || alrmtime > 100000000)
	alrmtime = 100000;

readloop:
    if (err != 0)			/* check for ^G interrupts */
	return 0;

    ichans = sel_ichans; ochans = sel_ochans;
    if (chan == NULL)
	ichans &= ~1;			/* don't look at tty in this case */

    /* 
     * If we do this here, iff there is no input, then it will always
     * happen asap.
     */
    if (child_changed) {
	int c_ichans = ichans;
	timeout.tv_sec = 0; timeout.tv_usec = 0;
	if (select(32, &c_ichans, 0, 0, &timeout) <= 0)	/* if none waiting */
	{
	    change_msgs ();
	    child_changed = 0;
	}
    }

    timeout.tv_sec = alrmtime; timeout.tv_usec = 0;
    if ((cc = select(32, &ichans, &ochans, 0, &timeout)) < 0)
	goto readloop;			/* try again */
    else
	if (cc == 0)
	    EchoThem (1);


    if (ichans&1) {
        ichans &= ~1;
	cc = read(0, cbuffer, sizeof (cbuffer));
	if (cc > 0) {
	    if (child_changed) {
		change_msgs ();
		child_changed = 0;
	    }
	    mpxin->ch_ptr = cbuffer;
	    mpxin->ch_count = cc - 1;
	    stdin->_flag &= ~_IOEOF;
	    return (*mpxin->ch_ptr++ & 0377);
	}
	else if (cc == 0)
	{
	    fprintf(stderr,"null read from stdin\r\n");
	    stdin->_flag |= _IOEOF;	/* mark EOF encountered */
	    return(EOF);
	}
    }
    for (p = process_list; p != NULL; p = p->next_process) {
	this_chan = &p->p_chan;
	if (ichans & (1<<this_chan->ch_index)) {
	    ichans &= ~(1<<this_chan->ch_index);
	    cc = read(this_chan->ch_index, cbuffer, sizeof (cbuffer));
	    if (cc > 0) {
		this_chan->ch_ptr = cbuffer;
		this_chan->ch_count = cc;
		stuff_buffer(this_chan);
	    }
            else if (cc <= 0)
	    {
/*  With pty:s, when the parent process of a pty exits we are notified,
    just as we would be with any of our other children.  After the process
    exits, select() will indicate that we can read the channel.  When we
    do this, read() returns 0.  Upon receiving this, we close the channel.

    For unexpected processes, when the peer closes the connection, select()
    will indicate that we can read the channel.  When we do this, read()
    returns -1 with errno = ECONNRESET.  Since we never get notified of
    this via wait3(), we must explictly mark the process as having exited.
    (This corresponds to the action performed when a M_CLOSE is received
    with the MPXio version of Emacs.)
 */
		sel_ichans &= ~(1 << this_chan -> ch_index); /* disconnect */
		sel_ochans &= ~(1 << this_chan -> ch_index); /* disconnect */
		close (this_chan->ch_index);
	    }
	}
	if (ochans & (1<<this_chan->ch_index)) {
	    ochans &= ~(1<<this_chan->ch_index);
	    if (this_chan->ch_outrec.ccount) {
	       cc = write(this_chan->ch_index, "", 0);
	       if (cc < 0)
		   continue;
	       this_chan->ch_outrec.ccount = 0;
	    }
	    if (this_chan->ch_outrec.count) {
	       cc = write(this_chan->ch_index,
		   this_chan->ch_outrec.data, this_chan->ch_outrec.count);
	       if (cc > 0) {
		   this_chan->ch_outrec.data += cc;
		   this_chan->ch_outrec.count -= cc;
	       }
	    }
	    if (this_chan->ch_outrec.count == 0)
		sel_ochans &= ~(1<<this_chan->ch_index);
	}
    }
    /* SWT - do this after stuffing output.  Hopefully the "Exited"
     * message will always come at the end of the buffer then.
     */
    if (child_changed) {
	change_msgs ();
	child_changed = 0;
    }

    if (chan != NULL)
	goto readloop;

    return 0;
}


/* Give a message that a process has changed and indicate why.  Dead processes
   are not removed until after a Display Processes command has been issued so
   that the user doesn't wonder where his process went in times of intense
   hacking. */

change_msgs () {
    register struct process_blk *p;
    register struct buffer *old = bf_cur;
    int sent = 0;			/* if non-zero, call sentinel */
    char    line[50];

    sighold (SIGINT);
    for (p = process_list; p != NULL; p = p -> next_process)
	if (p -> p_flag & CHANGED) {
	    sent = 0;
	    p -> p_flag &= ~CHANGED;
	    switch (p -> p_flag & (SIGNALED | EXITED)) {
		case SIGNALED: 
		    SetBfp (p -> p_chan.ch_buffer);
		    SetDot (bf_s1 + bf_s2 + 1);
		    sprintfl (line, sizeof line, "%s%s\n",
			SIG_names[p -> p_reason],
			p -> p_flag & COREDUMPED ? " (core dumped)" : "");
		    if (p->p_chan.ch_sent == NULL)
			InsStr (line);
		    else
			sent++;		/* call sentinel */
		    break;
		case EXITED: 
			SetBfp (p -> p_chan.ch_buffer);
			SetDot (bf_s1 + bf_s2 + 1);
			sprintfl (line, sizeof line,
			    p -> p_reason ? "Exit %d\n" : "Exited\n",
			    p -> p_reason);
		    if (p -> p_chan.ch_sent == NULL)
			InsStr (line);
		    else
			sent++;

		    break;
		}

		if (p->p_flag & RUNNING)
		{
		    strcpy(line, "Running\n");
		    sent++;
		}
		if (p->p_flag & STOPPED)
		{
		    strcpy(line, "Stopped\n");
		    sent++;
		}

		if (p->p_chan.ch_sent != NULL && sent)
		{
		    register    Expression * MPX_Exp =
				MPX_process -> v_binding -> b_exp;
		    int     larg = arg;
		    enum ArgStates lstate = ArgState;
		    int     old_int = MPX_Exp -> exp_int;
		    char   *old_str = MPX_Exp -> exp_v.v_string;
		    register struct channel_blk *chan =
					       &(p -> p_chan);
		    register struct channel_blk *oldchan = MPX_chan;

		    arg = p -> p_reason | (p->p_flag << 16);
		    ArgState = HaveArg;
		    MPX_Exp -> exp_int =
			strlen (chan -> ch_buffer -> b_name);
		    MPX_Exp -> exp_v.v_string =
			chan -> ch_buffer -> b_name;
		    /* Set up so user can get string reason as well */
		    chan->ch_ptr = line;
		    chan->ch_count = strlen(line);
		    MPX_chan = chan;
		    ExecuteBound (chan -> ch_sent);
		    MPX_chan = oldchan;
		    chan->ch_ptr = NULL;
		    chan->ch_count = 0;
		    MPX_Exp -> exp_int = old_int;
		    MPX_Exp -> exp_v.v_string = old_str;
		    arg = larg;
		    ArgState = lstate;
		}
	}
    sigrelse (SIGINT);
    DoDsp (1);
    SetBfp (old);
}

/* Send any pending output as indicated in the process block to the 
   appropriate channel.
*/
send_chan (process)
register struct process_blk *process;
{
    register struct wh *output;

    output = &process -> p_chan.ch_outrec;
    if (output -> count == 0 && output -> ccount == 0) {
	/* error ("Null output"); */
	return 0;		/* No output to be done */
    }
    if (output->ccount) {
	if (write(output->index, "", 0) >= 0) {
	    output->ccount = 0;
	    return 0;
	}
    } else {
	if (output->count) {
	    int cc = write(output->index, output->data, output->count);
	    if (cc > 0) {
		output->data += cc;
		output->count -= cc;
	    }
	}
        if (output->count == 0)
	    return 0;
    }
    sel_ochans |= 1<<(output->index);
    return 0;			/* ACT 8-Sep-1982 */
}

/* Output has been recieved from a process on "chan" and should be stuffed in
   the correct buffer */
/* ACT 9-Sep-1982 Modified to remove "lockout" restriction and allow
   recursive stuffs. */

stuff_buffer (chan)
register struct channel_blk *chan;
{
    struct buffer  *old_buffer = bf_cur;

    sighold (SIGINT);

    if (chan -> ch_proc == NULL) {
	SetBfp (chan -> ch_buffer);
	SetDot (bf_s1 + bf_s2 + 1);
	InsCStr (chan -> ch_ptr, chan -> ch_count);
	if ((bf_s1 + bf_s2) > ProcessBufferSize) {
	    DelFrwd (1, ChunkSize);
	    DotLeft (ChunkSize);
	}
	if (bf_cur -> b_mark == NULL)
	    bf_cur -> b_mark = NewMark ();
	SetMark (bf_cur -> b_mark, bf_cur, dot);
	DoDsp (1);
	SetBfp (old_buffer);
	if (interactive)
	    WindowOn (bf_cur);
    }
    else {			/* ACT 31-Aug-1982 Added hold on prefix arg */
	register char	*old_str;
	int		larg = arg, old_int;
	enum ArgStates	lstate = ArgState;
	register Expression
			*MPX_Exp = MPX_process -> v_binding -> b_exp;
	struct channel_blk
			*old_chan = MPX_chan;

	old_int = MPX_Exp -> exp_int;
	old_str = MPX_Exp -> exp_v.v_string;
	arg = 1;
	ArgState = NoArg;	/* save arg & arg state */
	MPX_Exp -> exp_int = strlen (chan -> ch_buffer -> b_name);
	MPX_Exp -> exp_v.v_string =  chan -> ch_buffer -> b_name;
	MPX_chan = chan;	/* User will be able to get the output
				   for */
	ExecuteBound (chan -> ch_proc);
	MPX_chan = old_chan;	/* a very short time only */
	MPX_Exp -> exp_int = old_int;
	MPX_Exp -> exp_v.v_string = old_str;
	arg = larg;
	ArgState = lstate;	/* restore arg */
	SetBfp (chan -> ch_buffer);
	if ((bf_s1 + bf_s2) > ProcessBufferSize) {
	    DelFrwd (1, ChunkSize);
	    DotLeft (ChunkSize);
	}
	SetBfp (old_buffer);
    }
    chan -> ch_count = 0;

    sigrelse (SIGINT);
    return 0;			/* ACT 8-Sep-1982 */
}

/* Return a count of all active processes */

count_processes () {
    register struct process_blk *p;
    register    count = 0;

    for (p = process_list; p != NULL; p = p -> next_process)
	if (active_process (p))
	    count++;
    return (count);
}

/* Flush a process but only if process is inactive */

flush_process (process)
register struct process_blk *process;
{
    register struct process_blk *p,
				*lp;

    if (active_process (process)) {
	error ("Can't flush an active process");
	return 0;
    }

    for (lp = NULL, p = process_list;
	    (p != NULL) && (p != process);
	    lp = p, p = p -> next_process);
    if (p != process) {
	error ("Can't find process");
	return 0;
    }
    if (lp == NULL)
	process_list = process -> next_process;
    else
	lp -> next_process = process -> next_process;
    free (process);
    return 0;
}

/* Kill off all active processes: done only to exit when user really
   insists */

kill_processes () {
    register struct process_blk *p;

    for (p = process_list; p != NULL; p = p -> next_process) {
	if (active_process (p)) {
	    ioctl (p -> p_chan.ch_index, TIOCGPGRP, (waddr_t)&(p -> p_gid));
	    if (p -> p_gid != -1) {
	        message("Killing pgrp %d", p->p_gid);
		DoDsp(1);
		sleep(3);
		killpg (p -> p_gid, SIGKILL);
	    }
	    if (p -> p_pid != -1) {
	        message("Killing pgrp %d", p->p_pid);
		DoDsp(1);
		sleep(3);
		killpg (p -> p_pid, SIGKILL);
	    }
	}
    }
}

/* Start up a new process by creating the process block and initializing 
   things correctly */

start_process (com, buf, proc)
register char	*com,
		*buf;
{
    extern struct process_blk  *get_next_process ();

    if (com == 0)
	return 0;
    current_process =
	(struct process_blk *) malloc (sizeof (struct process_blk));
    if (current_process == NULL) {
	error ("Out of memory");
	return 0;
    }
    sighold (SIGCHLD);
    current_process -> next_process = process_list;
    process_list = current_process;
    if (create_process (com) < 0) {/* job was not started, so undo */
	flush_process (current_process);
	current_process = get_next_process ();
	sigrelse (SIGCHLD);
	return 0;
    }
    SetBfn (buf == NULL ? "Command execution" : buf);
    if (interactive)
	WindowOn (bf_cur);
    current_process -> p_chan.ch_buffer = bf_cur;
    current_process -> p_chan.ch_proc = (proc < 0 ? NULL : MacBodies[proc]);
    current_process -> p_chan.ch_sent = NULL;
    sigrelse (SIGCHLD);
    return 0;
}


/* Emacs command to start up a default process: uses "Command Execution"
   buffer if one is not specified.  Also does default stuffing */

StartProcess () {
register char   *com = (char *) (savestr (getstr ("Command: ")));
register char   *buf;

    if ((com == 0) || (*com == 0)) {
	error ("No command");
	return 0;
    }
    buf = (char *) getstr ("Connect to buffer: ");
    if (*buf == 0)
	buf = NULL;
    start_process (com, buf, -1);
    return 0;			/* ACT 8-Sep-1982 */
}

/* Start up a process whose output will get filtered through a procedure
   specified by the user */

StartFilteredProcess () {
    register char   *com = (char *) (savestr (getstr ("Command: ")));
    register char   *buf;
    int	 proc;
    char bufname[MAXPATHLEN];

    if ((com == 0) || (*com == 0)) {
	error ("No command");
	return 0;
    }
    buf = getstr ("Connect to buffer: ");
    if (buf == 0) return 0;
    strcpy (bufname, buf);
    proc = getword (MacNames, "On-output procedure: ");
    start_process (com, bufname[0] ? bufname : NULL, proc);
    return 0;			/* ACT 8-Sep-1982 */
}

/* Set the UnexpectedProc pointer */
static
SetUnexpectedProc () {
    register proc =
	getword (MacNames, "Filter unexpected processes through command: ");
    UnexpectedProc = proc < 0 ? NULL : MacBodies[proc];
}

/* Return a process buffer or NULL */
struct process_blk *
GetBufProc ()
{
    register    b = getword (BufNames, "Process: ");
    if (b < 0)
	return NULL;
    return find_process (BufNames[b]);
}

/* Insert a filter-procedure between a process and emacs. This function
   should subsume the StartFilteredProcess function, but we should retain
   that one for compatibility I suppose. */
InsertFilter ()
{
    register struct process_blk *process;
    register int proc;

    if ((process = GetBufProc ()) == NULL) {
	error ("Not a Process");
	return 0;
    }
    proc = getword(MacNames, "On-output procedure: ");
    process -> p_chan.ch_proc = (proc < 0 ? NULL : MacBodies[proc]);
    return(0);
}

/* Reset filter rebinds the process filter to NULL */
ResetFilter () {
    register struct process_blk *process;
  
    if ((process = GetBufProc ()) == NULL) {
	error ("Not a Process");
	return 0;
    }
    process -> p_chan.ch_proc = NULL;
    return 0;			/* ACT 8-Sep-1982 */
}

/* ProcessFilterName returns the name of the process filter */
ProcessFilterName () {
    register struct process_blk *process;
    char   *name;

    if ((process = GetBufProc ()) == NULL) {
	error ("Not a Process");
	return 0;
    }
    MLvalue -> exp_type = IsString;
    MLvalue -> exp_release = 0;
    name = process -> p_chan.ch_proc
	? process -> p_chan.ch_proc -> b_name : "";
    MLvalue -> exp_int = strlen (name);
    MLvalue -> exp_v.v_string = name;
    return 0;
}

static
InsertSentinel ()
{
    register struct process_blk *process;
    register int proc;

    if ((process = GetBufProc ()) == NULL) {
	error ("Not a Process");
	return 0;
    }
    proc = getword(MacNames, "On-exit procedure: ");
    process -> p_chan.ch_sent = (proc < 0 ? NULL : MacBodies[proc]);
    return(0);
}

static
ResetSentinel () {
    register struct process_blk *process;
  
    if ((process = GetBufProc ()) == NULL) {
	error ("Not a Process");
	return 0;
    }
    process -> p_chan.ch_sent = NULL;
    return 0;
}

static
ProcessSentinelName () {
    register struct process_blk *process;
    char   *name;

    if ((process = GetBufProc ()) == NULL) {
	error ("Not a Process");
	return 0;
    }
    MLvalue -> exp_type = IsString;
    MLvalue -> exp_release = 0;
    name = process -> p_chan.ch_sent
	? process -> p_chan.ch_sent -> b_name : "";
    MLvalue -> exp_int = strlen (name);
    MLvalue -> exp_v.v_string = name;
    return 0;
}

static
SetUnexpectedSent() {
    register proc =
	getword (MacNames, "unexpected processes on exit call command: ");
    UnexpectedSent = proc < 0 ? NULL : MacBodies[proc];
}

/* List the current processes.  After listing stopped or exited processes,
   flush them from the process list. */

ListProcesses () {
    register struct buffer *old = bf_cur;
    register struct process_blk *p;
    char    line[150], tline[20];

    SetBfn ("Process list");
    if (interactive)
	WindowOn (bf_cur);
    EraseBf (bf_cur);
    InsStr ("\
Buffer			Status		   Command\n\
------			------		   -------\n");
    sighold (SIGCHLD);
    for (p = process_list; p != NULL; p = p -> next_process) {
	sprintfl (line, sizeof line, "%-24s", p -> p_chan.ch_buffer -> b_name);
	InsStr (line);
	switch (p -> p_flag & (STOPPED | RUNNING | EXITED | SIGNALED)) {
	    case STOPPED: 
		sprintfl (line, sizeof line, "%-17s", "Stopped");
		break;
	    case RUNNING: 
		sprintfl (line, sizeof line, "%-17s", "Running");
		break;
	    case EXITED: 
		sprintfl (tline, sizeof tline,
			p -> p_reason ? "Exit %d" : "Exited", p -> p_reason);
		sprintf (line, "%-17s", tline);
		flush_process (p);
		break;
	    case SIGNALED: 
		sprintfl (tline, sizeof tline, "%s%s",
			SIG_names[p -> p_reason],
			p -> p_flag & COREDUMPED ? " (core dumped)" : "");
		sprintf (line, "%-17s", tline);
		flush_process (p);
		break;
	    default: 
		sprintf(line, "0x%x\n", p -> p_flag);
		InsStr (line);
		continue;
	}
	InsStr (line);
	sprintfl (line, sizeof line, "   %-32s\n", p -> p_name);
	InsStr (line);
    }
    sigrelse (SIGCHLD);
    bf_modified = 0;
    SetBfp (old);
    WindowOn (bf_cur);
    return 0;
}

/* Take input from mark to dot and feed to the subprocess */
RegionToProcess () {
    register	left,
		right;
    register struct process_blk *process;
    register struct wh *output;

    if ((process = GetBufProc ()) == NULL) {
	error ("Not a Process");
	return 0;
    }

    if (bf_cur -> b_mark == 0) {
	error ("Mark not set");
	return 0;
    }
    left = ToMark (bf_cur -> b_mark);
    if (left <= dot)
	right = dot;
    else {
	right = left;
	left = dot;
    }
    if (right - left <= 0) {
	error ("Region is null");
	return 0;
    }
    if (left < bf_s1 && right >= bf_s1)
	GapTo (left);
    output = &process -> p_chan.ch_outrec;
    if (output -> count || output -> ccount)
	error ("Overwriting data on blocked channel");
    output -> index = process -> p_chan.ch_index;
    output -> ccount = 0;
    output -> count = right - left;
    output -> data = &CharAt (left);
    send_chan (process);
    return 0;
}

/* Send a string to the process as input */
StringToProcess () {
    register char *input_string;
    register struct process_blk *process;
    register struct wh *output;
 
    if ((process = GetBufProc()) == NULL) {
	error ("Not a Process");
	return 0;
    }
    input_string = getstr("String: ");
    output = &process -> p_chan.ch_outrec;
    if (output -> count || output -> ccount)
	error("Overwriting data on blocked channel");
    output -> index = process -> p_chan.ch_index;
    output -> ccount = 0;
    output -> count = strlen(input_string);
    if (output -> count <= 0)
	error("Null string");
    output -> data = input_string;
    send_chan (process);
    return 0;
}
   
/* Get the current output which has been thrown at us and send it
   to the user as a string; this is only allowed if MPX_chan is non-null
   indicating that this has been indirectly called from stuff_buffer. */

ProcessOutput () {
    if (MPX_chan == NULL) {
	error ("process-output can only be called from filter");
	return 0;
    }

    MLvalue -> exp_type = IsString;
    MLvalue -> exp_release = 1;
    MLvalue -> exp_int = MPX_chan -> ch_count;
    MLvalue -> exp_v.v_string = (char *) malloc (MLvalue -> exp_int + 1);
    cpyn (MLvalue -> exp_v.v_string, MPX_chan -> ch_ptr,
	MLvalue -> exp_int);
    MLvalue -> exp_v.v_string[MLvalue -> exp_int] = '\0';
    return 0;
}

/* Send an signal to the specified process group.  Goes to leader
   (process which started whole mess) iff "leader". */

sig_process (signal, leader) register leader; {
    register struct process_blk *process;

    if ((process = GetBufProc ()) == NULL) {
	error ("Not a process");
	return 0;
    }


/* We must update the process flag explicitly in the case of continuing a 
   process since no signal will come back */

    if (signal == SIGCONT) {
	sighold (SIGCHLD);
	process -> p_flag = (process -> p_flag & ~STOPPED) | RUNNING | CHANGED;
	child_changed++;
	sigrelse (SIGCHLD);
    }

    ioctl (process -> p_chan.ch_index, TIOCGPGRP, (waddr_t)&(process -> p_gid));
    switch (signal) {
	case SIGINT:  case SIGQUIT:
	    ioctl (process -> p_chan.ch_index, TIOCFLUSH, (waddr_t)0);
	    process -> p_chan.ch_outrec.count = 0;
	    process -> p_chan.ch_outrec.count = 0;
	    break;
    }

    leader = leader ? process -> p_pid : process -> p_gid;
    if (leader != -1)
    return 0;
}

IntProcess () {
    return (sig_process (SIGINT, 0));
}

IntPLeader () {
    return (sig_process (SIGINT, 1));
}

QuitProcess () {
    return (sig_process (SIGQUIT, 0));
}

QuitPLeader () {
    return (sig_process (SIGQUIT, 1));
}

KillProcess () {
    return (sig_process (SIGKILL, 0));
}

KillPLeader () {
    return (sig_process (SIGKILL, 1));
}

StopProcess () {
    return (sig_process (SIGTSTP, 0));
}

StopPLeader () {
    return (sig_process (SIGTSTP, 1));
}

ContProcess () {
    return (sig_process (SIGCONT, 0));
}

ContPLeader () {
    return (sig_process (SIGCONT, 1));
}

SignalToProcess () {
    return SignalToProcOrLeader (0);
}

SignalToPLeader () {
    return SignalToProcOrLeader (1);
}

SignalToProcOrLeader (leader) {
    register char *s = getnbstr ("Signal: ");
    register i;

    if (!s || !*s) return 0;
    if (*s >= '0' && *s <= '9')
	return sig_process (atoi (s), leader);
    for (i = 0; i < sizeof KillNames/sizeof *KillNames; i++)
	if (strcmp (KillNames[i], s) == 0)
	    return sig_process (i, leader);
    error ("\"%s\" is not a signal name", s);
    return 0;
}

EOTProcess () {
    register struct process_blk *process;
    register struct wh *output;

    if ((process = GetBufProc ()) == NULL) {
	error ("Not a process");
	return (0);
    }
    output = &process -> p_chan.ch_outrec;
    if (output -> count || output -> ccount)
	error ("Overwriting on blocked channel");

    output -> index = process -> p_chan.ch_index;
    output -> count = 0;
    output -> ccount = 1;
    output -> data = "";
    send_chan (process);
    return 0;			/* ACT 8-Sep-1982 */
}

/* Some useful functions on the process */
StrFunc (CurrentProcess,
    (current_process ? current_process -> p_chan.ch_buffer -> b_name : ""));

/* Return the name of the currently active process: it is defined as the name
   of the current buffer if is attached to an active process. */

ActiveProcess () {
    register struct process_blk *p;

    for (p = process_list; p != NULL; p = p -> next_process)
	if (active_process (p) && (p -> p_chan.ch_buffer == bf_cur))
	    break;
    if (p == NULL)
	p = current_process;
    MLvalue -> exp_type = IsString;
    MLvalue -> exp_release = 0;
    if (p == NULL) {
	MLvalue -> exp_int = 0;
	MLvalue -> exp_v.v_string = NULL;
    }
    else {
	MLvalue -> exp_int = strlen (p -> p_chan.ch_buffer -> b_name);
	MLvalue -> exp_v.v_string = p -> p_chan.ch_buffer -> b_name;
    }
    return 0;
}

/* Change the current-process to the one indicated */

ChangeCurrentProcess () {
    register struct process_blk *process = GetBufProc ();

    if (process == NULL) {
	error ("Not a process");
	return 0;
    }
    current_process = process;
    return 0;
}

/* Return the process' status:
	-1 - not an active process
	 0 - a stopped process
	 1 - a running process
*/

/* It's tempting to make this call GetBufProc() - but do not ... */
ProcessStatus () {
    register char *name = getstr ("Process: ");
    register struct process_blk *process = find_process (name);

    MLvalue -> exp_type = IsInteger;
    if (process == NULL)
	MLvalue -> exp_int = -1;
    else
	if (process -> p_flag & RUNNING)
	    MLvalue -> exp_int = 1;
	else
	    MLvalue -> exp_int = 0;
    return 0;
}

/* Get the process id */
ProcessID () {
    return PID (0);
}

/* Get the process leader id */
PLeaderID () {
    return PID (1);
}

PID (leader) {
    register char  *p_name = getstr ("Process name: ");
    register struct process_blk *process;

    MLvalue -> exp_type = IsInteger;
    process = find_process (p_name);
    if (process == NULL)
	MLvalue -> exp_int = 0;
    else
	MLvalue -> exp_int = leader ? process -> p_pid : process -> p_gid;
    return 0;
}

/* Get input from a subprocess (or the tty) and process it.
 * Tty input is just buffered until requested.
 */
static AwaitProcessInput ()		/* just poll for input */
{
    fill_chan(NULL, 0);
}


/* Initialize things on the multiplexed file.  This involves connecting the
   standard input to a channel on the mpx file. */

InitMpx () {
    extern  child_sig ();
    extern char *MyTtyName;

    mpxin -> ch_index = 0;
    sel_ichans = 1;
    ioctl (0, TIOCGETP, (waddr_t)&mysgttyb);
    mysgttyb.sg_flags = EVENP | ODDP;
    ioctl (0, TIOCGETC, (waddr_t)&mytchars);
    ioctl (0, TIOCGLTC, (waddr_t)&myltchars);
    ioctl (0, TIOCLGET, (waddr_t)&mylmode);
    mpxin -> ch_ptr = NULL;
    mpxin -> ch_count = 0;
    sigset (SIGCHLD, child_sig);

    if (!Once)
    {
    DefStrVar ("MPX-process", "");
    MPX_process = NextInitVarDesc[-1];

    ProcessBufferSize = 10000;	/* # of chars in buffer before truncating */
    DefIntVar ("process-buffer-size", &ProcessBufferSize);

    PopUpUnexpected = 1;
    DefIntVar ("pop-up-process-windows", &PopUpUnexpected);

    EmacsShare = 1;
    DefIntVar ("emacs-share", &EmacsShare);

    defproc (StartProcess, "start-process");
    defproc (StartFilteredProcess, "start-filtered-process");
    defproc (InsertFilter, "insert-filter");
    defproc (ResetFilter, "reset-filter");
    defproc (ProcessFilterName, "process-filter-name");
    defproc (RegionToProcess, "region-to-process");
    defproc (StringToProcess, "string-to-process");
    defproc (IntProcess, "int-process");
    defproc (QuitProcess, "quit-process");
    defproc (KillProcess, "kill-process");
    defproc (StopProcess, "stop-process");
    defproc (ContProcess, "continue-process");
    defproc (IntPLeader, "int-process-leader");
    defproc (QuitPLeader, "quit-process-leader");
    defproc (KillPLeader, "kill-process-leader");
    defproc (StopPLeader, "stop-process-leader");
    defproc (ContPLeader, "continue-process-leader");
    defproc (SignalToProcess, "signal-to-process");
    defproc (SignalToPLeader, "signal-to-process-leader");
    defproc (EOTProcess, "eot-process");
    defproc (CurrentProcess, "current-process");
    defproc (ProcessStatus, "process-status");
    defproc (ChangeCurrentProcess, "change-current-process");
    defproc (ActiveProcess, "active-process");
    defproc (ProcessID, "process-id");
    defproc (PLeaderID, "process-leader-id");
    defproc (ProcessOutput, "process-output");
    defproc (ListProcesses, "list-processes");
    defproc (SetUnexpectedProc, "unexpected-process-filter");
    defproc (InsertSentinel, "insert-sentinel");
    defproc (ResetSentinel, "reset-sentinel");
    defproc (ProcessSentinelName, "process-sentinel-name");
    defproc (SetUnexpectedSent, "unexpected-process-sentinel");
    defproc (AwaitProcessInput, "await-process-input");
    }
}

QuitMpx () {
}


SuspendMpx () {
}

ResumeMpx () {
}
