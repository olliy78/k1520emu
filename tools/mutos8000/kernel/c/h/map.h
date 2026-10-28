/* map.h -- Speicher-/Swapkarte (malloc/mfree): Eintrag m_size, m_addr (geraten nach V7; malloc 2DF0 first-fit). */
#ifndef MAP_H
#define MAP_H
struct map {
	int		m_size;
	unsigned	m_addr;
};
extern struct map	coremap[];
extern struct map	swapmap[];
#endif
