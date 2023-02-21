/* pchan.c - PTYio handler for multiple processes */

/* Original changes for 4.2bsd (c) 1982 William N. Joy and Regents of UC */

/* More changes for 4.1aBSD by Spencer Thomas of Utah-Cs */

/* Still more changes for 4.1aBSD by Marshall Rose of UCI */

/* Changes for 4.1cBSD by Chris Kent of Dec-Wrl */

#include "config.h"


#include <stdio.h>
#include <signal.h>
#include <errno.h>
#include <wait.h>
#include <sgtty.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "window.h"
#include "keyboard.h"
#include "buffer.h"
#include "mlisp.h"
#include "macros.h"
#include "mchan.h"



extern	int	child_changed;	/* all these from schan.c */
extern	int	PopUpUnexpected;
extern	int	EmacsShare;
extern	struct	BoundName	*UnexpectedProc;
extern	struct	BoundName	*UnexpectedSent;
extern	struct	process_blk	*GetBufProc ();
extern	struct	process_blk	*find_process ();

extern int  errno;
extern int  sys_nerr;
extern char *sys_errlist[];

static	int     sel_ichans;		/* input channels */
static	int     sel_ochans;		/* blocked output channels */

static	struct sgttyb mysgttyb;
static	struct tchars mytchars;
static	struct ltchars myltchars;
static	int mylmode;



/* Find a free pty and open it. */

static	char *pty(ptyv)
int *ptyv;
{
	struct stat stb;
	static char name[24];
	int on = 1, i;

	strcpy(name, "/dev/ptypX");
	for (;;) {
		name[strlen("/dev/ptyp")] = '0';
		if (stat(name, &stb) < 0)
			return (0);
		for (i = 0; i < 16; i++) {
			name[strlen("/dev/ptyp")] = "0123456789abcdef"[i];
			*ptyv = open(name, 2);
			if (*ptyv >= 0) {
				ioctl(*ptyv, FIONBIO, &on);
				name[strlen("/dev/")] = 't';
				return (name);
			}
		}
		name[strlen("/dev/pty")]++;
	}
}

/* Start up a subprocess with its standard input and output connected to 
   a channel on a pty.  Also set its process group so we can kill it
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

    sighold (SIGCHLD);
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
	    ioctl (ld, TIOCNOTTY, 0);
	    close (ld);
	}
	close (2);
	if (open (ptyname, 2) < 0) {
	     write (1, "Can't open tty\n", 15);
	     _exit (1);
	}
	pgrp = getpid();
	setpgrp (0, pgrp);
	ioctl (2, TIOCSPGRP, &pgrp);
	close (0);
	close (1);
	dup (2);
	dup (2);
	ioctl (0, TIOCSETP, &mysgttyb);
	ioctl (0, TIOCSETC, &mytchars);
	ioctl (0, TIOCSLTC, &myltchars);
	ioctl (0, TIOCLSET, &mylmode);
	len = 0;			/* set page features to 0 */
	len = UseUsersShell;
	UseUsersShell = 1;
	ld = strcmp(shell(), "/bin/csh") ? OTTYDISC : NTTYDISC;
	ioctl (0, TIOCSETD, &ld);
	UseUsersShell = len;
	execlp (shell (), shell (),
		UseUsersShell && UseCshOptionF ? "-cf" : "-c", command, 0);
	write (1, "Couldn't exec the shell\n", 24);
	_exit (1);
    }

    current_process -> p_name = command;
    current_process -> p_pid = pid;
    current_process -> p_gid = pid;
    current_process -> p_flag = RUNNING | CHANGED;
    child_changed++;
    current_process -> p_chan.ch_index = channel;
    current_process -> p_chan.ch_ptr = NULL;
    current_process -> p_chan.ch_count = 0;
    current_process -> p_chan.ch_outrec.index = channel;
    current_process -> p_chan.ch_outrec.count = 0;
    current_process -> p_chan.ch_outrec.ccount = 0;
    return 0;
}


/* This corresponds to the filbuf routine used by getchar.  This handles all 
   the input from a pty.  Input coming from the terminal is sent back
   to getchar() in the same manner as filbuf.

   With pty:s, when the parent process of a pty exits we are notified,
   just as we would be with any of our other children.  After the process
   exits, select() will indicate that we can read the channel.  When we
   do this, read() returns 0.  Upon receiving this, we close the channel.

   For unexpected processes, when the peer closes the connection, select()
   will indicate that we can read the channel.  When we do this, read()
   returns -1 with errno = ECONNRESET.  Since we never get notified of
   this via wait3(), we must explictly mark the process as having exited.
   (This corresponds to the action performed when a M_CLOSE is received
   with the MPXio version of Emacs -- see mchan.c)
 */

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
	if (cc == 0) {
	    EchoThem (1);
	    alrmtime = 10000;
	}


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
    if (child_changed) {
	change_msgs ();
	child_changed = 0;
    }

    if (chan != NULL)
	goto readloop;

    return 0;
}



/* Send any pending output as indicated in the process block to the 
   appropriate channel. */

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


/* Kill off all active processes: done only to exit when user really
   insists. */

kill_processes () {
    register struct process_blk *p;

    for (p = process_list; p != NULL; p = p -> next_process) {
	if (active_process (p)) {
	    ioctl (p -> p_chan.ch_index, TIOCGPGRP, &(p -> p_gid));
	    if (p -> p_gid != -1)
		killpg (p -> p_gid, SIGKILL);
	    if (p -> p_pid != -1)
		killpg (p -> p_pid, SIGKILL);
	}
    }
}


/* Send an signal to the specified process group.  Goes to leader
   (process which started whole mess) iff "leader". */

sig_process (signal, leader) register   leader; {
    register struct process_blk *process;
    struct tchars   mytchars;
    struct ltchars  myltchars;

    if ((process = GetBufProc ()) == NULL) {
	error ("Not a process");
	return 0;
    }


/* We must update the process flag explicitly in the case of continuing a 
   process since no signal will come back */
    if (signal == SIGCONT) {
	sighold (SIGCHLD);
	process -> p_flag =
	    (process -> p_flag & ~STOPPED) | RUNNING | CHANGED;
	child_changed++;
	sigrelse (SIGCHLD);
    }

    if (!leader)
	switch (signal) {
	    case SIGINT: 
		mytchars.t_intrc = -1;
		ioctl (process -> p_chan.ch_index, TIOCGETC, &mytchars);
		if (mytchars.t_intrc == -1)
		    break;
		return send_char (process, mytchars.t_intrc);

	    case SIGQUIT: 
		mytchars.t_quitc = -1;
		ioctl (process -> p_chan.ch_index, TIOCGETC, &mytchars);
		if (mytchars.t_quitc == -1)
		    break;
		return send_char (process, mytchars.t_quitc);

	    case SIGTSTP: 
		myltchars.t_suspc = -1;
		ioctl (process -> p_chan.ch_index, TIOCGLTC, &myltchars);
		if (myltchars.t_suspc == -1)
		    break;
		return send_char (process, myltchars.t_suspc);
	}
    ioctl (process -> p_chan.ch_index, TIOCGPGRP, &(process -> p_gid));

    leader = leader ? process -> p_pid : process -> p_gid;

    if (leader != -1)
	killpg (leader, signal);
    return 0;
}


/* Send an EOT to a process. */

EOTProcess () {
    register struct process_blk *process;
    struct tchars   mytchars;

    if ((process = GetBufProc ()) == NULL) {
	error ("Not a process");
	return (0);
    }

    mytchars.t_eofc = -1;
    ioctl (process -> p_chan.ch_index, TIOCGETC, &mytchars);
    if (mytchars.t_eofc == -1) {
	error ("Unable to determine EOT");
	return 0;
    }
    return send_char (process, mytchars.t_eofc);
}


/* Send a special character to a process. */

static send_char (process, c)
register struct process_blk *process;
char c;
{
    register struct wh *output = &process -> p_chan.ch_outrec;

    if (output -> count || output -> ccount)
	error ("Overwriting on blocked channel");

    output -> index = process -> p_chan.ch_index;
    output -> ccount = 0;
    output -> count = 1;
    output -> data = &c;
    send_chan (process);
    return 0;
}


/* Find the process-id of a process (or parent process). */

PID (leader) {
    register char  *p_name = getstr ("Process name: ");
    register struct process_blk *process;

    MLvalue -> exp_type = IsInteger;
    process = find_process (p_name);
    if (process == NULL)
	MLvalue -> exp_int = 0;
    else {
	ioctl (process -> p_chan.ch_index, TIOCGPGRP, &(process -> p_gid));
	MLvalue -> exp_int = leader ? process -> p_pid : process -> p_gid;
    }
    return 0;
}


/* Initialize the PTYio system. */

/* When a connection closes, any write()s to it will cause a SIGPIPE to
   be given to us.  By ignoring the signal, write() will return NOTOK
   after setting errno = EPIPE.  The relevant routines should test for
   this after a losing write().  In reality though, when the peer closes
   the connection, we'll find out via select() and an error read().
   Hence, fill_chan() will handle things for us. */

InitProcesses () {

    mpxin -> ch_index = 0;
    mpxin -> ch_ptr = NULL;
    mpxin -> ch_count = 0;

    sel_ichans = 1 << 0;	/* stdin */

    ioctl (0, TIOCGETP, &mysgttyb);
    mysgttyb.sg_flags = EVENP | ODDP;
    ioctl (0, TIOCGETC, &mytchars);
    ioctl (0, TIOCGLTC, &myltchars);
    ioctl (0, TIOCLGET, &mylmode);

}

/* named this way for historical reasons... */

QuitMpx () {
}

/* This isn't quite correct, the close() should do it, but Unix doesn't
   fully cooperate with us -- it sometimes will tell other processes that
   the port is still open for business. */

SuspendMpx () {
}

ResumeMpx () {
}



