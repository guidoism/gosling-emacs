/* keyboard manipulation primitives */

/*		Copyright (c) 1981,1980 James Gosling		*/

#include "keyboard.h"
#include "window.h"
#include "buffer.h"
#include "config.h"
#include "mlisp.h"
#include <sgtty.h>
#include <sys/types.h>

typedef long * waddr_t;

#include "mchan.h"

#define MAXPUSHBACK 128

#include <signal.h>
#include <setjmp.h>

static Reading;			/* True iff currently trying to read a
				   character from the tty */
static InterruptChar;		/* Set true when an interrupt character is
				   recieved */
static jmp_buf ReaderEnv;	/* a buffer for the only non-local goto in
				   Emacs.  It has to be done this way because
				   with the new signal system, system calls
				   get continued, and if we get a SIGINT when
				   reading from the tty we want to fake the
				   receipt of a ^G, so we have to break out
				   of the read. */


/* A keyboard called procedure returns:
	 0 normally
	-1 to quit */

static EndOfMac;		/* the place where the keyboard macro
				   currently being defined should end. */
static PushedBack[MAXPUSHBACK];		/* A buffer of most recently
                                   pushed back characters;
				   the last character in the buffer 
				   will be returned by GetChar the next
				   time that GetChar is called */
static int pbhead;			/* points to the most recently pushed back
				   character */
static MetaChar;		/* The meta-ized character between when */
				/* the meta-char is typed and it is read */
				/* (The escape is returned, this is saved) */
static CheckpointFrequency;	/* The number of keystrokes between
				   checkpoints. */
static Keystrokes;		/* The number of keystrokes since the last
				   checkpoint. */
static CanCheckpoint;		/* True iff we're allowed to checkpoint
				   now. */
static char KeyBuf[10];		/* Buffer for keys from GetChar() */
static NextK;			/* Next index into KeyBuf */
static EchoKeys;		/* >= 0 iff we are to echo keystrokes */
static EchoArg;			/* >= 0 iff we are to echo arg */
static Echo1, Echo2;		/* Stuff for final echo */

#define	min(a,b)	((a)<(b)?(a):(b))

EchoThem (notfinal)
register notfinal;
{
    char *dash = notfinal ? "-" : "";

    if (EchoArg >= 0 && ArgState != NoArg) {
	if (EchoKeys >= 0 && NextK)
	    message ("Arg: %d %s%s", arg, KeyToStr (KeyBuf, NextK), dash);
	else
	    message ("Arg: %d", arg);
    }
    else {
	if (EchoKeys >= 0 && NextK)
	    message ("%s%s", KeyToStr (KeyBuf, NextK), dash);
	else
	    return;
    }
    if (notfinal)
	Echo1++;		/* set echoed-flag */
    if (notfinal >= 0)
	DoDsp (0);
}


/* ProcessKeys reads keystrokes and interprets them according to the
   given keymap and its inferior keymaps */
ProcessKeys () {
    register struct keymap *m;
    static struct keymap    NullMap;
    register    c;
    NextGlobalKeymap = 0;
    NextLocalKeymap = 0;

    while (1) {
	if (NextGlobalKeymap == 0) {
	    if (Remembering)
		EndOfMac = MemUsed;
	    if (ArgState != HaveArg && MemPtr == 0 && bf_cur != minibuf)
		UndoBoundary ();
	}
	CanCheckpoint++;
	if (!InputPending && (EchoKeys == 0 || EchoArg == 0))
	    EchoThem (-1);
	if ((c = GetChar ()) < 0) {
	    CanCheckpoint = 0;
	    return 0;
	}
	if (NextK >= sizeof KeyBuf)
	    NextK = 0;
	KeyBuf[NextK++] = c;
	CanCheckpoint = 0;
	if (NextGlobalKeymap == 0)
	    NextGlobalKeymap = CurrentGlobalMap;
	if (NextLocalKeymap == 0)
	    NextLocalKeymap = bf_mode.md_keys;
	if (wn_cur -> w_buf != bf_cur)
	    SetBfp (wn_cur -> w_buf);
	if (m = NextLocalKeymap) {
	    register struct BoundName  *p;
	    NextLocalKeymap = 0;
	    if (p = m -> k_binding[c]) {
		LastKeyStruck = c & 0177;
		if (p -> b_binding != KeyBound) {
		    /* If echoed immediate preceding key, echo this one */
		    if (!InputPending && Echo2)
			EchoThem (0);
		    NextK = 0;
		    ThisCommand = LastKeyStruck;
		}
		if (ExecuteBound (p) < 0)
		    return 0;
		if (ArgState != HaveArg)
		    PreviousCommand = ThisCommand;
		if (NextLocalKeymap == 0 || NextGlobalKeymap == 0) {
		    NextGlobalKeymap = 0;
		    continue;
		}
	    }
	}
	if (m = NextGlobalKeymap) {
	    register struct BoundName  *p;
	    register struct keymap *local;
	    local = NextLocalKeymap;
	    NextGlobalKeymap = 0;
	    NextLocalKeymap = 0;
	    if (p = m -> k_binding[c]) {
		LastKeyStruck = c & 0177;
		if (p -> b_binding != KeyBound) {
		    if (!InputPending && Echo2)
			EchoThem (0);
		    NextK = 0;
		    ThisCommand = LastKeyStruck;
		}
		if (ExecuteBound (p) < 0)
		    return 0;
		if (ArgState != HaveArg)
		    PreviousCommand = ThisCommand;
		if (NextLocalKeymap) {
		    NextGlobalKeymap = NextLocalKeymap;
		    NextLocalKeymap = local ? local : &NullMap;
		}
		else {
		    NextGlobalKeymap = local ? &NullMap : 0;
		    NextLocalKeymap = local;
		}
		continue;
	    }
	    else {
		NextGlobalKeymap = local ? &NullMap : 0;
		NextLocalKeymap = local;
	    }
	}
	if (NextLocalKeymap == 0) {
	    NextK = 0;
	    IllegalOperation ();
	}
	else
	    NextGlobalKeymap = &NullMap;
    }
}

/* read a character from the keyboard; call the redisplay if needed */
GetChar () {
    register c;
    register alarmtime =
	EchoKeys >= 0 ? (EchoArg >= 0 ? min (EchoKeys, EchoArg)
				      : EchoKeys)
		      : EchoArg;

    if(pbhead >= 0){
        c = PushedBack[pbhead--];
	goto ReturnIt;
    }
    if((c = MetaChar) >= 0) {
	MetaChar = -1;
	if (InputPending > 0)
	    InputPending--;
	goto ReturnIt;
    }
    if (MemPtr) {
	if (err) {
	    MemPtr = 0;
	    c = -1;
	    goto ReturnIt;
	}
	c = (unsigned char) *MemPtr++;
	if (c)
	{
	    c &= 0177;			/* fix up ^@-s */
	    goto ReturnIt;
	}
	MemPtr = 0;
	c = -1;
	goto ReturnIt;
    }
    if (err && InputFD!=stdin){
	c = -1;
	goto ReturnIt;
    }
    if (InputFD==stdin && mpxin->ch_count==0 && !InputPending) {
	ioctl (fileno(stdin), FIONREAD, (waddr_t)&InputPending);
	if(!InputPending) {
	    DoDsp (0);
	    if(CheckpointFrequency>0 && CanCheckpoint
		    && Keystrokes>CheckpointFrequency) {
		CheckpointEverything ();
		Keystrokes = 0;
	    }
	}
    }
    Keystrokes++;
    if (setjmp (ReaderEnv) || ((Reading=1),InterruptChar)) {
	c = Ctl ('G');
	if (InputFD == stdin) mpxin->ch_count = 0;
	InputPending = 0;
	InterruptChar = 0;
    } else
    if(InputFD == stdin) {
	if (alarmtime > 0)
	    sigset (SIGALRM, EchoThem);
	c = mpx_getc(mpxin, alarmtime);
	InputPending = mpxin->ch_count;
    }
    else {
	c = getc(InputFD);
	InputPending = stdin->_cnt>0;
    }
    if (MiniBuf && (!InMiniBuf || *MiniBuf) && !ResetMiniBuf)
	MiniBuf = *MiniBuf ? "" : 0;	/* Only reset minibuf w/ kbd input */
    Reading = 0;
    if(c<0)
    {
	c = -1;
	goto ReturnIt;
    }

    if (MetaFlag)
    {
	c &= 0377;
	if (c & 0200)			/* if real meta char */
	{
	    MetaChar = c & 0177;	/* remember the character */
	    c = '\033';			/* and return an ESC */
	    InputPending++;
	}				/* this is a kludge, but it''s the */
    }					/* easiest thing to do */
    else
	c &= 0177;

    Remember(c);
    if (MetaFlag && MetaChar >= 0)	/* handle meta''s right */
	Remember(MetaChar);

ReturnIt:
    Echo2 = Echo1;		/* Save last echoed-flag */
    Echo1 = 0;			/* Clear echoed-flag */
    return c;
}

/* Remember for kbd macros */
static
Remember(c)
unsigned char c;
{
    if (Remembering) {
	if (c)
	    KeyMem[MemUsed++] = c;
	else
	    KeyMem[MemUsed++] = 128;	/* handle ^@ right */
	if (MemUsed >= MemLen) {
	    error ("Keystroke memory overflow!");
	    Remembering = EndOfMac = MemUsed = KeyMem[0] = 0;
	}
    }
}

/* Given a keystroke sequence look up the BoundName that it is bound to */
struct BoundName **LookupKeys (map, keys, len)
register struct keymap *map;
register char *keys;
register len;
{
    register struct BoundName  *b;
    while (map && --len >= 0) {
	b = map -> k_binding[*keys];
	if (len == 0)
	    return &map -> k_binding[*keys];
	keys++;
	if (b == 0 || b -> b_binding != KeyBound)
	    break;
	map = b -> b_bound.b_keymap;
    }
    return 0;
}

StartRemembering () {
    if (Remembering)
	error ("Already remembering!");
    else {
	Remembering++;
	MemUsed = EndOfMac = 0;
	message("Remembering...");
    }
    return 0;
}

StopRemembering () {
    if (Remembering) {
	Remembering = 0;
	KeyMem[EndOfMac] = 0;
	message("Keyboard macro defined.");
    }
    return 0;
}

/* Execute the given command string */
ExecStr (s)
char *s; {
    register char  *old = MemPtr;
    MemPtr = s;
    ProcessKeys ();
    MemPtr = old;
}

ExecuteKeyboardMacro () {
    static Executing;		/* true iff already executing */

    if (Remembering)
	error ("Sorry, you can't call the keyboard macro while defining it.");
    else
	if (MemUsed == 0)
	    error ("No keyboard macro to execute.");
	else if (Executing)
	    return 0;
	else {
	    register i = arg;
	    Executing++;
	    arg = 0;
	    ArgState = NoArg;
	    do ExecStr (KeyMem);
	    while (!err && --i>0);
	    Executing = 0;
	}
    return 0;
}

static
PushBackCharacter () {
    register    n = (int) *getkey (&GlobalMap, ": push-back-character ");
    if (!err)
	if (++pbhead >= MAXPUSHBACK) {
            pbhead = -1;
            error ("Can't push back this many characters.");
        }
        else
            PushedBack[pbhead] = n;
    return 0;
}

RecursiveEdit () {
    struct ProgNode *oldp = CurExec;
    register FILE *oldf = InputFD;
    InputFD = stdin;
    CurExec = 0;
    RecurseDepth++;
    Cant1LineOpt++;
    RedoModes++;
    ProcessKeys ();
    RecurseDepth--;
    Cant1LineOpt++;
    RedoModes++;
    CurExec = oldp;
    InputFD = oldf;
    return 0;
}

/* Return MLisp value nonzero if (a) input pending or (b) we can't tell */
static
KeysPending () {
    ReleaseExpr (MLvalue);
    MLvalue -> exp_type = IsInteger;
    if (!InputPending)
	ioctl (fileno(stdin), FIONREAD, (waddr_t)&InputPending);
    MLvalue -> exp_int = InputPending;
    return 0;
}

/* This routine is called at interrupt level on receipt of an INT signal.  It
   cleanly terminates whatever is going on at the moment. */


static InterruptKey () {
    if (!Reading)
	IllegalOperation ();
    InterruptChar++;
    if (Reading) {
	Reading = 0;
	sigrelse (SIGINT);
	longjmp (ReaderEnv, 1);
    }
}


InitKey () {
    pbhead = -1;
    MetaChar = -1;
/*    sigset (SIGINT, InterruptKey); *//*XXXXXXXXX*/
    sigset (SIGINT, InterruptKey);/*XXXXXXXXX*/
    if (!Once)
    {
	setkey (CtlXmap, ('e'), ExecuteKeyboardMacro, "execute-keyboard-macro");
	setkey (CtlXmap, ('('), StartRemembering, "start-remembering");
	setkey (CtlXmap, (')'), StopRemembering, "stop-remembering");
	DefIntVar ("checkpoint-frequency", &CheckpointFrequency);
	CheckpointFrequency = 300;
	DefIntVar ("echo-keystrokes", &EchoKeys);
	EchoKeys = -1;
	DefIntVar ("echo-argument", &EchoArg);
	EchoArg = -1;
	defproc (PushBackCharacter, "push-back-character");
	defproc (RecursiveEdit, "recursive-edit");
	defproc (KeysPending, "pending-input");
	DefIntVar ("this-command", &ThisCommand);
    }
}
