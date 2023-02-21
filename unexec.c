/* 
 * unexec.c - Convert a running program into an a.out file.
 * 
 * Author:	Spencer W. Thomas
 * 		Computer Science Dept.
 * 		University of Utah
 * Date:	Tue Mar  2 1982
 * Copyright (c) 1982 Spencer W. Thomas
 *
 * Synopsis:
 *	unexec( new_name, a_name, data_start, bss_start )
 *	char *new_name, *a_name;
 *	unsigned data_start, bss_start;
 *
 * Takes a snapshot of the program and makes an a.out format file in the
 * file named by the string argument new_name.
 * If a_name is non-NULL, the symbol table will be taken from the given file.
 * 
 * The boundaries within the a.out file may be adjusted with the data_start 
 * and bss_start arguments.  Either or both may be given as 0 for defaults.
 * 
 * Data_start gives the boundary between the text segment and the data
 * segment of the program.  The text segment can contain shared, read-only
 * program code and literal data, while the data segment is always unshared
 * and unprotected.  Data_start gives the lowest unprotected address.  Since
 * the granularity of write-protection is on 1k page boundaries on the VAX, a
 * given data_start value which is not on a page boundary is rounded down to
 * the beginning of the page it is on.  The default when 0 is given leaves the
 * number of protected pages the same as it was before.
 * 
 * Bss_start indicates how much of the data segment is to be saved in the
 * a.out file and restored when the program is executed.  It gives the lowest
 * unsaved address, and is rounded up to a page boundary.  The default when 0
 * is given assumes that the entire data segment is to be stored, including
 * the previous data and bss as well as any additional storage allocated with
 * break (2).
 * 
 * This routine is expected to only work on a VAX running 4.1 bsd UNIX and
 * will probably require substantial conversion effort for other systems.
 * In particular, it depends on the fact that a process' _u structure is
 * accessible directly from the user program.
 *
 * If you make improvements I'd like to get them too.
 * harpo!utah-cs!thomas, thomas@Utah-20
 *
 */
#include <stdio.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <a.out.h>
/* GROSS HACK. UGH WRONG DEATH DON"T EXECUTE THE CODE UGH */
#define NBPG 512
/* ^^^^^^^^^^^^ AWFUL */
#define PAGEMASK    (NBPG - 1)
#define PAGEUP(x)   (((int)(x) + PAGEMASK) & ~PAGEMASK)


#define PSIZE	    10240

extern etext;
extern edata;
extern end;

static struct exec hdr, ohdr;

static int unexeced;

/* ****************************************************************
 * unexec
 *
 * driving logic.
 */
unexec( new_name, a_name, data_start, bss_start )
char *new_name, *a_name;
unsigned data_start, bss_start;
{
    perror( "dump-emacs doesn't work on this system" );
}

