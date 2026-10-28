/* mount.h -- Mounttabelle, 6 Byte je Eintrag, 5 Eintraege ab A07E.  Belegt (iget sucht m_inodp). */
#ifndef MOUNT_H
#define MOUNT_H
struct mount {
	dev_t		m_dev;		/* +0 */
	struct buf	*m_bufp;	/* +2 */
	struct inode	*m_inodp;	/* +4 */
};
extern struct mount	mount[];
#endif
