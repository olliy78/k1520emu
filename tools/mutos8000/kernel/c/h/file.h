/* file.h -- offene Datei, 8 Byte, Tabelle 9EEE..A07E (50 Eintraege).  Belegt: f_flag 0, f_count 1, f_offset 4;
 * f_inode 2 (V7, nicht einzeln gelesen); closef reicht das Wort +4 als dritten Parameter an close weiter (f_chan). */
#ifndef FILE_H
#define FILE_H
#include "types.h"
struct file {
	char		f_flag;		/* +0 */
	cnt_t		f_count;	/* +1 */
	struct inode	*f_inode;	/* +2 */
	union {				/* +4 */
		off_t		f_off;	/* Lese-/Schreibzeiger (long) */
		struct chan	*f_chan; /* V7-Multiplexkanal: erstes Wort, wird closef als 3. Argument uebergeben (Kernel 2B36) */
	} f_un;
};
#define f_offset	f_un.f_off
#define FREAD	01
#define FWRITE	02
#define FPIPE	04
extern struct file	file[];
#endif
