/* callo.h -- Zeitaufrufliste.  UNGEMESSEN: timeout (2A4C) lief im Messlauf nie; Aufbau nach V7. */
#ifndef CALLO_H
#define CALLO_H
struct callo {
	int	c_time;
	caddr_t	c_arg;
	int	(*c_func)();
};
extern struct callo	callout[];
#endif
