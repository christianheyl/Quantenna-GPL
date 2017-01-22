/*
 * Copyright (C) 2004, 2007-2010, 2011-2012 Synopsys, Inc. (www.synopsys.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __ASM_LINKAGE_H
#define __ASM_LINKAGE_H

#define SYMBOL_NAME_STR(X) #X
#define SYMBOL_NAME(X) X

#define SYMBOL_NAME_LABEL(X) X:

#ifdef __ASSEMBLY__
; the ENTRY macro in linux/linkage.h does not work for us ';' is treated
; as a comment and I could not find a way to put a new line in a #define

.macro ARC_ENTRY name
  .globl SYMBOL_NAME(\name)
  .align 4
  SYMBOL_NAME_LABEL(\name)
.endm


.macro ARC_EXIT name
#define ASM_PREV_SYM_ADDR(name)  .-##name
  .size \name, ASM_PREV_SYM_ADDR(\name)
.endm

#endif

#define __arcfp_code __attribute__((__section__(".text.arcfp")))
#define __arcfp_data __attribute__((__section__(".data.arcfp")))

#endif
