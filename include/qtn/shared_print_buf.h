#ifndef __SHARED_PRINT_BUF
#define __SHARED_PRINT_BUF

struct shared_print_producer {
	u32	produced;
	u32	bufsize;
	char*	buf;		/* producer address space ptr */
};

struct shared_print_consumer {
	const volatile struct shared_print_producer * producer;
	u32 consumed;
	char* buf;		/* consumer address space ptr */
};

#endif // __SHARED_PRINT_BUF
