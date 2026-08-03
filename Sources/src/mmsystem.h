#ifndef BLITZKRIEG_PORTABLE_MMSYSTEM_H
#define BLITZKRIEG_PORTABLE_MMSYSTEM_H
static inline unsigned long timeGetTime(void) { return 0; }
static inline unsigned int timeBeginPeriod(unsigned int) { return 0; }
static inline unsigned int timeEndPeriod(unsigned int) { return 0; }
#endif
