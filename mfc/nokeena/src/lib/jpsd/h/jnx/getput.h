/*
 * @file getput.h
 * @brief
 * getput.h - unaligned get/put routines
 *
 * These routines provide a way of packing and unpacking multiple byte
 * quantities without worrying about alignment.  They also deal with
 * host/network ordering issues.
 *
 * Yuvaraja Mariappan, Dec 2014
 * Copyright (c) 2015, Juniper Networks, Inc.
 * All rights reserved.
 */

#ifndef _GET_PUT_H
#define _GET_PUT_H

#include <sys/jnx/types.h>

static inline uint32_t get_long(const void *ptr)
{
	const uint8_t *cp = ptr;
	uint32_t val;

	val = *cp++;
	val = (val << 8) | *cp++;
	val = (val << 8) | *cp++;
	val = (val << 8) | *cp;

	return val;
}

static inline void *put_long(void *ptr, uint32_t value)
{
	uint8_t *cp = ptr;

	*cp++ = (uint8_t) ((value >> 24) & 0xff);
	*cp++ = (uint8_t) ((value >> 16) & 0xff);
	*cp++ = (uint8_t) ((value >> 8) & 0xff);
	*cp++ = (uint8_t) (value & 0xff);

	return cp;
}

static inline void *put_short(void *ptr, uint32_t value)
{
	uint8_t *cp = ptr;

	*cp++ = (uint8_t) (value >> 8);
	*cp++ = (uint8_t) (value & 0xff);

	return cp;
}

static inline uint16_t get_short(const void *ptr)
{
	const uint8_t *cp = ptr;
	uint16_t shrt;

	shrt = *cp++;
	shrt = (shrt << 8) | *cp;

	return shrt;
}

#endif /* !_GET_PUT_H */
