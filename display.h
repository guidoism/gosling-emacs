/* Ultra-hot screen management package
 *		James Gosling, January 1980
 */

#include "Trm.h"

#define ScreenLength (tt.t_length)
#define ScreenWidth (tt.t_width)

int	ScreenGarbaged,		/* true => screen content is uncertain. */
	DoHighlights,		/* true => hightlights should be done */
	cursX,			/* X and Y coordinates of the cursor */
	cursY,			/* between updates. */
	CurrentLine,		/* current line for writing to the virtual
				 * screen. */
	IDdebug,		/* line insertion/deletion debug switch */
	RDdebug,		/* line redraw debug switch */
	left;			/* number of columns left on the current
				 * line of the virtual screen. */
char
	*cursor;		/* pointer into a line object, indicates
				 * where to put the next character */

/* 'dsputc' places a character at the current position on the display,
 * the character must be a simple one, taking up EXACTLY one position on
 * the screen.  ie. tabs and \n's shouldn't be passed to dsputc. */
#define dsputc(c) (--left>=0 ? (*cursor++ = c) : 0)

/* 'setpos' positions the cursor at position (row,col) in the virtual
 * screen
setpos(row,col);

/* set up highlights for the rectangular region extending from the BottomRow
 * to the TopRow and from the LeftColumn to the RightColumn.  This highlight
 * will be applied when next the screen is updated.
BoxHighlight(TopRow,BottomRow,LeftCol,RightCol)

/* 'UpdateScreen' updates the physical screen, assuming that it looks
 * like the 'CurrentScreen', making it look like the 'DesiredScreen'.
 * If 'copy' is true then after calling UpdateScreen the DesiredScreen
 * will be unchanged, otherwise it will be blank.
UpdateScreen(copy);

 */
