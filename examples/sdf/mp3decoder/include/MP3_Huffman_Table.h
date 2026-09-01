#ifndef _HUFFMAN_TABLES_H_
#define _HUFFMAN_TABLES_H_

/* The last two tables are A and B, respectively. The three values are:
 * Pointer to the table
 * Number of entries
 * Linbits
 * In the tables each 32-bit word contains left (0) offset in the high 32 bits
 * and right (1) offset in the low 32 bits. If left offset is 0, then the 
 * xyzw values are in the low 8 bits.
 */

//#include "MP3_Main.h"
#include <stdint.h>

/* g_huffman_main packs a pointer to each Huffman sub-table into element
 * [0], alongside its treelen and linbits. UINT32 could hold that pointer
 * when this decoder was written for 32-bit targets; on a 64-bit build it
 * truncates, which is a hard error ("cast from UINT32* to UINT32 loses
 * precision") for all 33 tables. uintptr_t is the type that is defined
 * to hold a pointer round-trip on any target. */
extern uintptr_t g_huffman_main [34][3];

#endif /* _HUFFMAN_TABLES_H_ */
