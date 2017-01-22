#ifndef __INSTRUMENT_HOOKS_H
#define __INSTRUMENT_HOOKS_H

#ifdef PROFILE_LINUX_EP
void notrace set_instrument_enter_func(void(*enter)(void *, void *));
void notrace set_instrument_exit_func(void(*exit)(void *, void *));
void notrace clear_instrument_funcs(void);
#endif

#endif // __INSTRUMENT_HOOKS_H

