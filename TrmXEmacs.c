/* terminal control module for naked X */

/*
 * This is a driver for the "X" VS100 device driver. It manipulates the
 * window's directly, thus avoiding the overhead of going through a device
 * emulator (Like xpty or xterm).
 */

#include <stdio.h>
#include "display.h"

static
topos (row, column) register row, column; {
}

static
reset () {
}

static
enum IDmode { m_insert = 1, m_overwrite = 0 }
	CurMode, DesMode;

static
INSmode (new)
enum IDmode new; {
}

static curHL;
static
HLmode (on) register on; {
}

static
inslines (n) register n; {
}

static
dellines (n) register n; {
}

static
blanks (n) register n; {
}

static
init (BaudRate) {
    char *getenv();
    static inited = 0;
    if (!inited) {
      /* one time inits, program specific? */
    }
    /* other inits, terminal specific? */
}

static
cleanup () {
    INSmode (m_overwrite);
    setmode ();
    HLmode (0);
    window (0);
    topos (WindowSize, 1);
    wipeline ();
}

static
wipeline () {
}

static
wipescreen () {
}

static
delchars (n) {
}

static
writechars (start, end)
register char	*start,
		*end; {
    setmode ();
    while (start <= end) {
    /* output (*start++) */
    /* inc cursor pos */
    }
}

static
window (n) register n; {
    if (n <= 0 || n > tt.t_length) n = tt.t_length;
    /* do it */
}

static
setmode () {
    if (DesMode == CurMode) return;
    /* set current insert/deletion mode */
    CurMode = DesMode;
};

static
flash () {
}

TrmVT100 (t)
register char *t; {
  /* t is the actual terminal type, if we care */
	tt.t_topos = topos;
	tt.t_reset = reset;
	tt.t_INSmode = INSmode;
	tt.t_HLmode = HLmode;
	tt.t_inslines = inslines;
	tt.t_dellines = dellines;
	tt.t_blanks = blanks;
	tt.t_init = init;
	tt.t_cleanup = cleanup;
	tt.t_wipeline = wipeline;
	tt.t_wipescreen = wipescreen;
	tt.t_delchars = delchars;
	tt.t_writechars = writechars;
	tt.t_window = window;
	tt.t_flash = flash;
	/*
	 * cost factor calculation.
	 * IL = Insert/Delete Lines, IC = Insert Character, DC = Delete Lines
	 * cost is number_affected*mf+ov
	 */
	tt.t_ILmf = 1.0;
	tt.t_ILov = 1;
	tt.t_ICmf = 1.0;
	tt.t_ICov = 1;
	tt.t_DCmf = 1.0;
	tt.t_DCov = 1;
	tt.t_length = 24;	/* twaddle, will be set in init () */
	tt.t_width = 80;	/* likewise, twaddle */
	tt.t_needspaces = 0;
	tt.t_modeline = 7;	/* highlight modeline */
};
