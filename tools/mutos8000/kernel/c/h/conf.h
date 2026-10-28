/* conf.h -- Geraeteschalter.  bdevsw: 8 Byte je Eintrag (open, close, strategy, d_tab), Tabelle 8766.
 * cdevsw: 14 Byte je Eintrag (open, close, read, write, ioctl, stop, tty), Tabelle 8796.  Belegt (K6a). */
#ifndef CONF_H
#define CONF_H
struct bdevsw {
	int		(*d_open)();
	int		(*d_close)();
	int		(*d_strategy)();
	struct devtab	*d_tab;
};
struct cdevsw {
	int		(*d_open)();
	int		(*d_close)();
	int		(*d_read)();
	int		(*d_write)();
	int		(*d_ioctl)();
	int		(*d_stop)();
	struct tty	*d_ttys;
};
/* K7e: Leitungsdisziplin, 20 Byte je Eintrag (ttioctl: MULT #20; Tabelle 8868, ein Eintrag; die letzten drei Worte sind 0) */
struct linesw {
	int		(*l_open)();
	int		(*l_close)();
	int		(*l_read)();
	int		(*l_write)();
	int		(*l_ioctl)();
	int		(*l_input)();
	int		(*l_output)();
	int		l_x[3];
};
extern struct linesw	linesw[];
extern struct bdevsw	bdevsw[];
extern struct cdevsw	cdevsw[];
/* Gerateetabelle (devtab, V7 iobuf): Kopf der Puffer des Geraets; Aufbau nur soweit getblk ihn benutzt. */
struct devtab {
	char		d_active;
	char		d_errcnt;
	struct buf	*b_forw;
	struct buf	*b_back;
	struct buf	*d_actf;
	struct buf	*d_actl;
};
#endif
