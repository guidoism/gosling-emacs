/* terminal control module for MiniBee's */

/*		Copyright (c) 1981 James Gosling		*/

#include <stdio.h>
#include "display.h"

static
int	curX, curY;

static
writechars (start, end)
register char	*start,
		*end; {
    register char *p;
    register int i;
    while (start <= end) {
	putchar (*start++);
	curX++;
    }
};

static
blanks (n) {
	while (--n >= 0) {
	    putchar (' ');
	    curX++;
	}
};

static
topos (row, column) register row, column; {
    if (curY == row) {
	if (curX == column)
	    return;
	if (curX == column + 1) {
	    putchar (010);
	    goto done;
	}
    }
    if (curY + 1 == row && (column == 1 || column==curX)) {
	if(column!=curX) putchar (015);
	putchar (012);
	goto done;
    }
    if (row == 1 && column == 1) {
	putchar (033);
	putchar ('H');
	goto done;
    }
    putchar (033);
    putchar ('F');
    putchar ((row-1) + (row-1)/10*6);
    putchar ((column-1) + (column-1)/10*6);
done:
    curX = column;
    curY = row;
};


static
reset () {
    curX = 1;
    curY = 1;
    printf ("\033H\033J");
};

static
null () {
};

static
wipeline () {
    putchar (033);
    putchar ('K');
};

static
wipescreen () {
    printf("\033H\033J");
    curX = curY = 1;
};


TrmMiniB () {
	tt.t_INSmode = null;
	tt.t_HLmode = null;
	tt.t_inslines = null;
	tt.t_dellines = null;
	tt.t_blanks = blanks;
	tt.t_init = null;
	tt.t_cleanup = null;
	tt.t_wipeline = wipeline;
	tt.t_wipescreen = wipescreen;
	tt.t_topos = topos;
	tt.t_reset = reset;
	tt.t_delchars = null;
	tt.t_writechars = writechars;
	tt.t_window = 0;
	tt.t_ILmf = 0;
	tt.t_ILov = MissingFeature;
	tt.t_ICmf = 0;
	tt.t_ICov = MissingFeature;
	tt.t_DCmf = MissingFeature;
	tt.t_DCov = MissingFeature;
	tt.t_length = 24;
	tt.t_width = 80;
	tt.t_modeline = 0;		/* no highlights anyway */
}
