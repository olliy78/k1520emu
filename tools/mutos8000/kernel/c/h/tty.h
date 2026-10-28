/* tty.h -- tty-Struktur.  UNGEMESSEN (K7a): ttyinput/ttread/ttwrite (7222..7DA6) wurden noch nicht zurueckgebaut.
 * Dies ist der V7-Aufbau (clist-Koepfe t_rawq/t_canq/t_outq mit je c_cc, c_cf, c_cl) als Platzhalter; Groesse und
 * Reihenfolge in MUTOS (Zeichenwarteschlangen getc/putc 3BFC/3C80 in Assembler) NICHT belegt.  Vor Gebrauch messen. */
#ifndef TTY_H
#define TTY_H
#include "types.h"
struct clist {
	int	c_cc;
	char	*c_cf;
	char	*c_cl;
};
struct tty {
	struct clist	t_rawq;
	struct clist	t_canq;
	struct clist	t_outq;
	int		(*t_oproc)();
	int		(*t_iproc)();
	struct proc	*t_rsel;
	struct proc	*t_wsel;
	dev_t		t_dev;
	int		t_flags;
	int		t_state;
	int		t_pgrp;
	char		t_delct;
	char		t_line;
	char		t_col;
	char		t_erase;
	char		t_kill;
	char		t_char;
};
#endif
