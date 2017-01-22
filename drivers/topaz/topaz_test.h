/**
 * Copyright (c) 2008-2012 Quantenna Communications, Inc.
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 **/

#ifndef __TOPAZ_TEST_H
#define __TOPAZ_TEST_H

#if defined(__KERNEL__)
#include <linux/ctype.h>
#include <qtn/qtn_debug.h>
#endif

#define TOPAZ_TEST_ASSERT_EQUAL(x, y)						\
	do {									\
		if (!((x) == (y))) {						\
			DBGFN("%s:%d:%s, '%s' %d 0x%x != '%s' %d 0x%x\n",	\
					__FILE__, __LINE__, __FUNCTION__,	\
					#x, (int)(x), (unsigned int)(x),	\
					#y, (int)(y), (unsigned int)(y));	\
			return -1;						\
		}								\
	} while(0)


static inline const char * topaz_test_skip_space(const char *c)
{
	while (c && *c && isspace(*c)) {
		c++;
	}
	return c;
}

static inline const char *topaz_test_skip_nonspace(const char *c)
{
	while (c && *c && !isspace(*c)) {
		c++;
	}
	return c;
}

static inline const char * topaz_test_next_word(const char *c)
{
	return topaz_test_skip_space(topaz_test_skip_nonspace(topaz_test_skip_space(c)));
}

static inline int topaz_test_split_words(char **words, char *c)
{
	int word_count = 0;

	/* skip leading space */
	while (c && *c && isspace(*c)) {
		c++;
	}

	while (c && *c) {
		words[word_count++] = c;

		/* skip this word */
		while (c && *c && !isspace(*c)) {
			c++;
		}

		/* replace spaces with NULL */
		while (c && *c && isspace(*c)) {
			*c = 0;
			c++;
		}
	}

	return word_count;
}

#define TOPAZ_TEST_CTRL_SRCMAC	{ 0x00, 0x26, 0x86, 0x00, 0x00, 0x00 }
#define TOPAZ_TEST_CTRL_DSTMAC	{ 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF }

int topaz_dpi_test_parse(int argc, char **argv);
int topaz_fwt_test_parse(int argc, char **argv);
int topaz_ipprt_emac0_test_parse(int argc, char **argv);
int topaz_ipprt_emac1_test_parse(int argc, char **argv);
int topaz_vlan_test_parse(int argc, char **argv);

#endif /* __TOPAZ_TEST_H */

