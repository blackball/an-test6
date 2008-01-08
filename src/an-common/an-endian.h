/*
  This file is part of the Astrometry.net suite.
  Copyright 2008 Dustin Lang.

  The Astrometry.net suite is free software; you can redistribute
  it and/or modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation, version 2.

  The Astrometry.net suite is distributed in the hope that it will be
  useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with the Astrometry.net suite ; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
*/
#ifndef AN_ENDIAN_H
#define AN_ENDIAN_H

#if (__BYTE_ORDER == __BIG_ENDIAN) || (_BYTE_ORDER == _BIG_ENDIAN) || (BYTE_ORDER == BIG_ENDIAN)
#define IS_BIG_ENDIAN 1
#else
#define IS_BIG_ENDIAN 0
#endif

// convert a u32 from little-endian to local.
inline uint32_t u32_letoh(uint32_t i) {
#if IS_BIG_ENDIAN
    return (
            ((i & 0x000000ff) << 24) |
            ((i & 0x0000ff00) <<  8) |
            ((i & 0x00ff0000) >>  8) |
            ((i & 0xff000000) >> 24));
#else
	return i;
#endif
}

// convert a u32 from local to little-endian.
inline uint32_t u32_htole(uint32_t i) {
    return u32_letoh(i);
}


static inline void v_htole(void* p, int nbytes) {
#if IS_BIG_ENDIAN
	int i;
	unsigned char* c = p;
	for (i=0; i<(nbytes/2); i++) {
		unsigned char tmp = c[i];
		c[i] = c[nbytes-(i+1)];
		c[nbytes-(i+1)] = tmp;
	}
#else
    // nop.
#endif
}

// convert a 32-bit object from local to little-endian.
inline void v32_htole(void* p) {
	return v_htole(p, 4);
}
inline void v16_htole(void* p) {
	return v_htole(p, 2);
}

#endif
