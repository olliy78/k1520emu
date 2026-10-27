/*
 * make.c -- kleines make im Funktionsumfang von Unix V7 fuer MUTOS
 *
 * Eigenbau (kein Fremdcode): ein Makefile mit Zielen, Abhaengigkeiten,
 * Suffixregeln (.c.o, .s.o), Makros und Zeitstempelvergleich ueber stat().
 * Der Aufbau folgt dem Verhalten des V7-make (Bell Labs); der Quelltext
 * ist klein gehalten, damit ihn der K&R-aehnliche MUTOS-cc (V7-Stil: keine
 * Prototypen, kein void, kein enum, keine Strukturzuweisung) uebersetzt.
 *
 * Unterstuetzt:
 *   ziel [ziel...] : [abh...] [; befehl]      Befehlszeilen beginnen mit TAB
 *   NAME = wert        $(NAME) ${NAME} $N     $(NAME:.c=.o)  $$
 *   $@ Ziel, $< erste Abhaengigkeit (bei Suffixregel die Quelle),
 *   $* Ziel ohne Suffix, $? neuere Abhaengigkeiten, $^ alle Abhaengigkeiten
 *   .SUFFIXES: .o .c .s      .c.o:  (Suffixregeln)
 *   Befehlsvorsatz '-' (Fehler ignorieren) und '@' (nicht anzeigen)
 *   Zeilenfortsetzung mit '\' am Zeilenende, Kommentare mit '#'
 *   make [-f datei] [-n] [-s] [-i] [NAME=wert...] [ziel...]
 * Eingebaut: CC=cc, CFLAGS=, .c.o: und .s.o: ueber "$(CC) $(CFLAGS) -c $<".
 * Nicht unterstuetzt: Doppelpunkt-Doppelregeln, include, Bedingungen,
 * Muster mit %, Archivglieder lib(x.o), .PHONY/.DEFAULT/.PRECIOUS.
 * Befehle laufen ueber system(), also /bin/sh -c.
 */
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>

#define LINESZ	1024
#define MAXCUR	16
#define MAXS	16

struct dep {
	char *d_name;
	struct dep *d_next;
};

struct cmd {
	char *c_text;
	struct cmd *c_next;
};

struct nameent {
	char *n_n;
	struct dep *n_deps;
	struct cmd *n_cmds;
	char *n_src;		/* Quelle einer Suffixregel */
	int n_isrule;		/* kam als Ziel vor einem ':' vor */
	int n_fresh;		/* Regelzeile gelesen, noch kein Befehl: neue Befehle ersetzen alte */
	int n_busy;
	int n_done;
	long n_time;
	struct nameent *n_next;
};

struct macent {
	char *m_n;
	char *m_v;
	int m_cmdline;
	struct macent *m_next;
};

char *malloc();
char *getenv();
char *index();
char *rindex();
long time();
char *strcat();
char *strcpy();

struct nameent *names;
struct nameent *first;
struct macent *macs;
struct nameent *cur[MAXCUR];
int ncur;
char *sfx[MAXS];
int nsfx;
int nflag, sflag, iflag;
int cmdrun;

/* Werte der Sondermakros waehrend eines Befehls */
char *m_target = "";
char *m_dep = "";
char *m_stem = "";
char *m_newer = "";
char *m_all = "";

/* Ausgabepuffer der Makroexpansion */
char *xp;
char *xend;

fatal(fmt, a)
char *fmt, *a;
{
	fflush(stdout);
	fprintf(stderr, "make: ");
	fprintf(stderr, fmt, a);
	fprintf(stderr, "\n");
	exit(1);
}

char *
alloc(n)
unsigned n;
{
	char *p;

	p = malloc(n);
	if (p == 0)
		fatal("kein Speicher", "");
	return p;
}

char *
save(s)
char *s;
{
	char *p;

	p = alloc(strlen(s) + 1);
	strcpy(p, s);
	return p;
}

blank(c)
int c;
{
	return c == ' ' || c == '\t';
}

/* fuehrende und nachfolgende Blanks entfernen, gibt Anfang zurueck */
char *
trim(s)
char *s;
{
	char *e;

	while (blank(*s))
		s++;
	e = s + strlen(s);
	while (e > s && blank(e[-1]))
		e--;
	*e = 0;
	return s;
}

struct nameent *
lookup(s, create)
char *s;
int create;
{
	struct nameent *n;

	for (n = names; n; n = n->n_next)
		if (strcmp(n->n_n, s) == 0)
			return n;
	if (!create)
		return 0;
	n = (struct nameent *)alloc(sizeof(struct nameent));
	n->n_n = save(s);
	n->n_deps = 0;
	n->n_cmds = 0;
	n->n_src = 0;
	n->n_isrule = 0;
	n->n_fresh = 0;
	n->n_busy = 0;
	n->n_done = 0;
	n->n_time = 0;
	n->n_next = names;
	names = n;
	return n;
}

setmac(nm, v, cmdline)
char *nm, *v;
int cmdline;
{
	struct macent *m;

	for (m = macs; m; m = m->m_next)
		if (strcmp(m->m_n, nm) == 0) {
			if (m->m_cmdline && !cmdline)
				return;
			m->m_v = save(v);
			m->m_cmdline = cmdline;
			return;
		}
	m = (struct macent *)alloc(sizeof(struct macent));
	m->m_n = save(nm);
	m->m_v = save(v);
	m->m_cmdline = cmdline;
	m->m_next = macs;
	macs = m;
}

char *
getmac(nm)
char *nm;
{
	struct macent *m;

	if (nm[0] && nm[1] == 0) {
		switch (nm[0]) {
		case '@': return m_target;
		case '<': return m_dep;
		case '*': return m_stem;
		case '?': return m_newer;
		case '^': return m_all;
		}
	}
	for (m = macs; m; m = m->m_next)
		if (strcmp(m->m_n, nm) == 0)
			return m->m_v;
	return getenv(nm);
}

put(c)
int c;
{
	if (xp >= xend)
		fatal("Zeile zu lang", "");
	*xp++ = c;
}

/* s in den Ausgabepuffer expandieren */
sub(s, depth)
char *s;
int depth;
{
	char nm[48], tmp[LINESZ];
	char *e, *v, *sp, *from, *to, *w, *wend, *oxp, *oxend;
	int close, i, lf, lw;

	if (depth > 10)
		fatal("Makro zu tief verschachtelt", "");
	while (*s) {
		if (*s != '$') {
			put(*s++);
			continue;
		}
		s++;
		if (*s == '$') {
			put('$');
			s++;
			continue;
		}
		sp = 0;
		if (*s == '(' || *s == '{') {
			close = (*s == '(') ? ')' : '}';
			e = index(s + 1, close);
			if (e == 0)
				fatal("Makro ohne Klammer", "");
			i = e - (s + 1);
			if (i > 40)
				i = 40;
			strncpy(nm, s + 1, i);
			nm[i] = 0;
			s = e + 1;
			sp = index(nm, ':');
			if (sp)
				*sp++ = 0;
		} else if (*s) {
			nm[0] = *s++;
			nm[1] = 0;
		} else
			break;
		v = getmac(nm);
		if (v == 0)
			continue;
		if (sp == 0) {
			sub(v, depth + 1);
			continue;
		}
		/* $(NAME:.alt=.neu): Suffix je Wort ersetzen */
		from = sp;
		to = index(sp, '=');
		if (to == 0)
			fatal("falsche Ersetzung $(%s)", nm);
		*to++ = 0;
		oxp = xp;
		oxend = xend;
		xp = tmp;
		xend = tmp + LINESZ - 1;
		sub(v, depth + 1);
		*xp = 0;
		xp = oxp;
		xend = oxend;
		lf = strlen(from);
		w = tmp;
		for (;;) {
			while (blank(*w))
				w++;
			if (*w == 0)
				break;
			wend = w;
			while (*wend && !blank(*wend))
				wend++;
			lw = wend - w;
			if (lw >= lf && strncmp(w + lw - lf, from, lf) == 0)
				lw -= lf;
			for (i = 0; i < lw; i++)
				put(w[i]);
			/* passte der Suffix nicht, bleibt das Wort unveraendert */
			if (lw != wend - w)
				for (v = to; *v; v++)
					put(*v);
			put(' ');
			w = wend;
		}
	}
}

/* src nach dst expandieren, hoechstens n Zeichen */
expand(src, dst, n)
char *src, *dst;
int n;
{
	xp = dst;
	xend = dst + n - 1;
	sub(src, 0);
	*xp = 0;
}

addcmd(text)
char *text;
{
	int i;
	struct cmd *c, *p;

	if (ncur == 0)
		fatal("Befehl ohne Ziel: %s", text);
	for (i = 0; i < ncur; i++) {
		if (cur[i]->n_fresh) {
			cur[i]->n_cmds = 0;
			cur[i]->n_fresh = 0;
		}
		c = (struct cmd *)alloc(sizeof(struct cmd));
		c->c_text = save(text);
		c->c_next = 0;
		if (cur[i]->n_cmds == 0)
			cur[i]->n_cmds = c;
		else {
			for (p = cur[i]->n_cmds; p->c_next; p = p->c_next)
				;
			p->c_next = c;
		}
	}
}

adddep(n, s)
struct nameent *n;
char *s;
{
	struct dep *d, *p;

	d = (struct dep *)alloc(sizeof(struct dep));
	d->d_name = save(s);
	d->d_next = 0;
	if (n->n_deps == 0)
		n->n_deps = d;
	else {
		for (p = n->n_deps; p->d_next; p = p->d_next)
			;
		p->d_next = d;
	}
}

/* naechstes Wort ab *pp (Blanks trennen); schneidet es ab, 0 am Ende */
char *
word(pp)
char **pp;
{
	char *p, *w;

	p = *pp;
	while (blank(*p))
		p++;
	if (*p == 0) {
		*pp = p;
		return 0;
	}
	w = p;
	while (*p && !blank(*p))
		p++;
	if (*p)
		*p++ = 0;
	*pp = p;
	return w;
}

doline(line)
char *line;
{
	char lhs[LINESZ], rhs[LINESZ];
	char *p, *q, *w, *semi, *nm, *v;
	struct nameent *n;
	int i;

	if (line[0] == '\t') {
		p = trim(line);
		if (*p && *p != '#')
			addcmd(p);
		return;
	}
	p = index(line, '#');
	if (p)
		*p = 0;
	line = trim(line);
	if (*line == 0)
		return;
	for (p = line; *p && *p != '=' && *p != ':'; p++)
		;
	if (*p == '=') {
		*p++ = 0;
		nm = trim(line);
		/* Wert sofort expandieren: X = $(X) -y haengt an */
		v = trim(p);
		expand(v, rhs, LINESZ);
		setmac(nm, trim(rhs), 0);
		ncur = 0;
		return;
	}
	if (*p != ':')
		fatal("Syntaxfehler: %s", line);
	*p++ = 0;
	semi = index(p, ';');
	if (semi)
		*semi++ = 0;
	expand(line, lhs, LINESZ);
	expand(p, rhs, LINESZ);
	ncur = 0;
	q = lhs;
	while ((w = word(&q)) != 0) {
		if (ncur >= MAXCUR)
			fatal("zu viele Ziele in einer Zeile", "");
		n = lookup(w, 1);
		n->n_isrule = 1;
		n->n_fresh = 1;
		cur[ncur++] = n;
		if (first == 0 && w[0] != '.')
			first = n;
	}
	if (ncur == 0)
		fatal("Regel ohne Ziel: %s", line);
	q = rhs;
	if (strcmp(cur[0]->n_n, ".SUFFIXES") == 0) {
		if (index(rhs, '.') == 0)
			nsfx = 0;
		while ((w = word(&q)) != 0) {
			if (nsfx >= MAXS)
				fatal("zu viele Suffixe", "");
			sfx[nsfx++] = save(w);
		}
		ncur = 0;
		return;
	}
	while ((w = word(&q)) != 0)
		for (i = 0; i < ncur; i++)
			adddep(cur[i], w);
	if (semi) {
		p = trim(semi);
		if (*p)
			addcmd(p);
	}
}

readmake(fp)
FILE *fp;
{
	char raw[LINESZ], acc[LINESZ];
	int len;

	acc[0] = 0;
	while (fgets(raw, LINESZ, fp)) {
		len = strlen(raw);
		if (len && raw[len - 1] == '\n')
			raw[--len] = 0;
		if (strlen(acc) + len + 2 >= LINESZ)
			fatal("Zeile zu lang", "");
		if (len && raw[len - 1] == '\\') {
			raw[--len] = 0;
			strcat(acc, raw);
			strcat(acc, " ");
			continue;
		}
		strcat(acc, raw);
		doline(acc);
		acc[0] = 0;
	}
	if (acc[0])
		doline(acc);
}

/* Aenderungszeit, 0 = Datei fehlt */
long
mtime(f)
char *f;
{
	struct stat sb;

	if (stat(f, &sb) < 0)
		return 0L;
	if (sb.st_mtime <= 0L)
		return 1L;
	return sb.st_mtime;
}

hasdep(n, s)
struct nameent *n;
char *s;
{
	struct dep *d;

	for (d = n->n_deps; d; d = d->d_next)
		if (strcmp(d->d_name, s) == 0)
			return 1;
	return 0;
}

/* Suffixregel suchen: x.o aus x.c ueber ".c.o" */
implicit(n)
struct nameent *n;
{
	char *dot, *slash, rn[64], src[128];
	struct nameent *r;
	struct dep *d;
	int i, stemlen;

	dot = rindex(n->n_n, '.');
	slash = rindex(n->n_n, '/');
	if (dot == 0 || (slash && slash > dot))
		return;
	stemlen = dot - n->n_n;
	if (stemlen > 100 || strlen(dot) > 8)
		return;
	for (i = 0; i < nsfx; i++) {
		if (strcmp(sfx[i], dot) == 0)
			continue;
		strcpy(rn, sfx[i]);
		strcat(rn, dot);
		r = lookup(rn, 0);
		if (r == 0 || r->n_cmds == 0)
			continue;
		strncpy(src, n->n_n, stemlen);
		src[stemlen] = 0;
		strcat(src, sfx[i]);
		if (!hasdep(n, src) && mtime(src) == 0L)
			continue;
		n->n_cmds = r->n_cmds;
		n->n_src = save(src);
		if (!hasdep(n, src)) {
			d = (struct dep *)alloc(sizeof(struct dep));
			d->d_name = n->n_src;
			d->d_next = n->n_deps;
			n->n_deps = d;
		}
		return;
	}
}

runcmd(raw)
char *raw;
{
	char buf[LINESZ];
	char *p;
	int silent, ign, st;

	expand(raw, buf, LINESZ);
	silent = sflag;
	ign = iflag;
	p = buf;
	while (*p == '@' || *p == '-' || blank(*p)) {
		if (*p == '@')
			silent = 1;
		if (*p == '-')
			ign = 1;
		p++;
	}
	if (*p == 0)
		return;
	if (!silent || nflag) {
		printf("%s\n", p);
		fflush(stdout);
	}
	cmdrun++;
	if (nflag)
		return;
	st = system(p);
	if (st != 0 && !ign) {
		fflush(stdout);
		fprintf(stderr, "make: *** Fehlercode %d\n", (st >> 8) & 0377);
		exit(1);
	}
}

/* Liste "a b c" an buf anhaengen */
catword(buf, w)
char *buf, *w;
{
	if (strlen(buf) + strlen(w) + 2 >= LINESZ)
		fatal("Abhaengigkeitsliste zu lang", "");
	if (buf[0])
		strcat(buf, " ");
	strcat(buf, w);
}

long
build(nm)
char *nm;
{
	struct nameent *n;
	struct dep *d;
	struct cmd *c;
	long t, dt, newest;
	char newer[LINESZ], all[LINESZ], stem[128];
	char *dot, *slash;
	int need;

	n = lookup(nm, 1);
	if (n->n_done)
		return n->n_time;
	if (n->n_busy) {
		fprintf(stderr, "make: Zirkelbezug bei %s\n", nm);
		return n->n_time;
	}
	n->n_busy = 1;
	t = mtime(nm);
	if (n->n_cmds == 0)
		implicit(n);
	newest = 0L;
	newer[0] = 0;
	all[0] = 0;
	for (d = n->n_deps; d; d = d->d_next) {
		dt = build(d->d_name);
		catword(all, d->d_name);
		if (dt > newest)
			newest = dt;
		if (t == 0L || dt > t)
			catword(newer, d->d_name);
	}
	need = (t == 0L) || (newest > t);
	if (t == 0L && n->n_cmds == 0 && n->n_deps == 0 && !n->n_isrule)
		fatal("weiss nicht, wie %s zu bauen ist", nm);
	if (need && n->n_cmds) {
		strncpy(stem, nm, 127);
		stem[127] = 0;
		dot = rindex(stem, '.');
		slash = rindex(stem, '/');
		if (dot && !(slash && slash > dot))
			*dot = 0;
		m_target = nm;
		m_dep = n->n_src ? n->n_src : (n->n_deps ? n->n_deps->d_name : "");
		m_stem = stem;
		m_newer = newer;
		m_all = all;
		for (c = n->n_cmds; c; c = c->c_next)
			runcmd(c->c_text);
		m_target = m_dep = m_stem = m_newer = m_all = "";
		t = nflag ? time((long *)0) : mtime(nm);
		if (t == 0L)
			t = time((long *)0);
	} else if (need)
		t = newest ? newest : time((long *)0);
	n->n_time = t;
	n->n_busy = 0;
	n->n_done = 1;
	return t;
}

/* eingebaute Regeln, wie eine Makefile-Vorlage gelesen */
char *deflt[] = {
		".SUFFIXES: .o .c .s",
		"CC=cc",
		"CFLAGS=",
		".c.o:",
		"\t$(CC) $(CFLAGS) -c $<",
		".s.o:",
		"\t$(CC) $(CFLAGS) -c $<",
		0
	};

main(argc, argv)
int argc;
char **argv;
{
	char *mf, *a, *eq, nmb[64];
	char *tg[32];
	int ntg, i, k;
	FILE *fp;
	char line[80];

	mf = 0;
	ntg = 0;
	for (i = 0; deflt[i]; i++) {
		strcpy(line, deflt[i]);
		doline(line);
	}
	first = 0;
	ncur = 0;
	for (i = 1; i < argc; i++) {
		a = argv[i];
		if (a[0] == '-' && a[1]) {
			for (k = 1; a[k]; k++)
				switch (a[k]) {
				case 'n': nflag = 1; break;
				case 's': sflag = 1; break;
				case 'i': iflag = 1; break;
				case 'f':
					if (++i >= argc)
						fatal("-f ohne Dateiname", "");
					mf = argv[i];
					break;
				default:
					fatal("unbekannte Option %s", a);
				}
			continue;
		}
		eq = index(a, '=');
		if (eq && eq - a < 60) {
			strncpy(nmb, a, eq - a);
			nmb[eq - a] = 0;
			setmac(nmb, eq + 1, 1);
			continue;
		}
		if (ntg >= 32)
			fatal("zu viele Ziele", "");
		tg[ntg++] = a;
	}
	if (mf) {
		fp = fopen(mf, "r");
		if (fp == NULL)
			fatal("kann %s nicht oeffnen", mf);
	} else {
		fp = fopen("makefile", "r");
		if (fp == NULL)
			fp = fopen("Makefile", "r");
		if (fp == NULL)
			fatal("kein makefile gefunden", "");
	}
	readmake(fp);
	fclose(fp);
	ncur = 0;
	if (ntg == 0) {
		if (first == 0)
			fatal("kein Ziel", "");
		tg[ntg++] = first->n_n;
	}
	for (i = 0; i < ntg; i++) {
		cmdrun = 0;
		build(tg[i]);
		if (cmdrun == 0)
			printf("make: '%s' ist aktuell.\n", tg[i]);
	}
	fflush(stdout);
	exit(0);
}
