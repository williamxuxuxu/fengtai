/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#ifndef _RTE_HEXDUMP_H_
#define _RTE_HEXDUMP_H_

/**
 * @file
 * Simple API to dump out memory in a special hex format.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LINE_LEN 128

/**
* Dump out memory in a special hex dump format.
*
* @param f
*		A pointer to a file for output
* @param title
*		If not NULL this string is printed as a header to the output.
* @param buf
*		This is the buffer address to print out.
* @param len
*		The number of bytes to dump out
* @return
*		None.
*/

static inline void
rte_hexdump(FILE *f, const char * title, const void * buf, unsigned int len)
{
	unsigned int i, out, ofs;
	const unsigned char *data = buf;
	char line[LINE_LEN];	/* space needed 8+16*3+3+16 == 75 */

	fprintf(f, "%s at [%p], len=%u\n",
		title ? : "  Dump data", data, len);
	ofs = 0;
	while (ofs < len) {
		/* format the line in the buffer */
		out = snprintf(line, LINE_LEN, "%08X:", ofs);
		for (i = 0; i < 16; i++) {
			if (ofs + i < len)
				snprintf(line + out, LINE_LEN - out,
					 " %02X", (data[ofs + i] & 0xff));
			else
				strcpy(line + out, "   ");
			out += 3;
		}


		for (; i <= 16; i++)
			out += snprintf(line + out, LINE_LEN - out, " | ");

		for (i = 0; ofs < len && i < 16; i++, ofs++) {
			unsigned char c = data[ofs];

			if (c < ' ' || c > '~')
				c = '.';
			out += snprintf(line + out, LINE_LEN - out, "%c", c);
		}
		fprintf(f, "%s\n", line);
	}
	fflush(f);
}

/**
* Dump out memory in a hex format with colons between bytes.
*
* @param f
*		A pointer to a file for output
* @param title
*		If not NULL this string is printed as a header to the output.
* @param buf
*		This is the buffer address to print out.
* @param len
*		The number of bytes to dump out
* @return
*		None.
*/

static inline void
rte_memdump(FILE *f, const char * title, const void * buf, unsigned int len)
{
	unsigned int i, out;
	const unsigned char *data = buf;
	char line[LINE_LEN];

	if (title)
		fprintf(f, "%s: ", title);

	line[0] = '\0';
	for (i = 0, out = 0; i < len; i++) {
		/* Make sure we do not overrun the line buffer length. */
		if (out >= LINE_LEN - 4) {
			fprintf(f, "%s", line);
			out = 0;
			line[out] = '\0';
		}
		out += snprintf(line + out, LINE_LEN - out, "%02x%s",
				(data[i] & 0xff), ((i + 1) < len) ? ":" : "");
	}
	if (out > 0)
		fprintf(f, "%s", line);
	fprintf(f, "\n");

	fflush(f);
}


#ifdef __cplusplus
}
#endif

#endif /* _RTE_HEXDUMP_H_ */
