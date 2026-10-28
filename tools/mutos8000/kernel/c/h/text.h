/* text.h -- Textstruktur.  UNGEMESSEN: in MUTOS sind xlock/xunlock/xccdec/xumount/xrele Stubs (8 Byte, nur csv/cret),
 * es gibt keine Texttabelle; p_textp bleibt in proc.  Struktur nach V7/WEGA, nur fuer xalloc/xfree. */
#ifndef TEXT_H
#define TEXT_H
struct text {
	int		x_daddr;
	int		x_caddr;
	int		x_size;
	struct inode	*x_iptr;
	char		x_count;
	char		x_ccount;
	char		x_flag;
};
#define XTRC	01
#define XWRIT	02
#define XLOAD	04
#define XLOCK	010
#define XWANT	020
extern struct text	text[];
#endif
