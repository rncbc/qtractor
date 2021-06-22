// lv2_atom_helpers.h
//
/****************************************************************************
   Copyright (C) 2005-2021, rncbc aka Rui Nuno Capela. All rights reserved.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; either version 2
   of the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License along
   with this program; if not, write to the Free Software Foundation, Inc.,
   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

*****************************************************************************/

/*  Helper functions for LV2 atom:Sequence event buffer.
 *
 *  tentatively adapted from:
 *
 *  - lv2_evbuf.h,c - An abstract/opaque LV2 event buffer implementation.
 *
 *  - event-helpers.h - Helper functions for the LV2 Event extension.
 *    <http://lv2plug.in/ns/ext/event>
 *
 *    Copyright 2008-2012 David Robillard <http://drobilla.net>
 */

#ifndef LV2_ATOM_HELPERS_H
#define LV2_ATOM_HELPERS_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

#include "lv2/lv2plug.in/ns/ext/atom/atom.h"


// An abstract/opaque LV2 atom:Sequence buffer.
//
typedef
struct _LV2_Atom_Buffer
{
	uint32_t capacity;
	uint32_t chunk_type;
	uint32_t sequence_type;
	LV2_Atom_Sequence aseq;

} LV2_Atom_Buffer;


// Pad a size to 64 bits (for LV2 atom:Sequence event sizes).
//
static inline
uint32_t lv2_atom_buffer_pad_size ( uint32_t size )
{
	return (size + 7) & (~7);
}


// Clear and initialize an existing LV2 atom:Sequenece buffer.
//
static inline
void lv2_atom_buffer_reset ( LV2_Atom_Buffer *abuf, bool input )
{
	if (input) {
		abuf->aseq.atom.size = sizeof(LV2_Atom_Sequence_Body);
		abuf->aseq.atom.type = abuf->sequence_type;
	} else {
		abuf->aseq.atom.size = abuf->capacity;
		abuf->aseq.atom.type = abuf->chunk_type;
	}
}


// Allocate a new, empty LV2 atom:Sequence buffer.
//
static inline
LV2_Atom_Buffer *lv2_atom_buffer_new (
	uint32_t capacity, uint32_t chunk_type, uint32_t sequence_type, bool input )
{
	LV2_Atom_Buffer *abuf = (LV2_Atom_Buffer *)
		malloc(sizeof(LV2_Atom_Buffer) + sizeof(LV2_Atom_Sequence) + capacity);

	abuf->capacity = capacity;
	abuf->chunk_type = chunk_type;
	abuf->sequence_type = sequence_type;

	lv2_atom_buffer_reset(abuf, input);

	return abuf;
}


// Free an LV2 atom:Sequenece buffer allocated with lv2_atome_buffer_new.
//
static inline
void lv2_atom_buffer_free ( LV2_Atom_Buffer *abuf )
{
	free(abuf);
}


// Return the total padded size of events stored in a LV2 atom:Sequence buffer.
//
static inline
uint32_t lv2_atom_buffer_get_size ( LV2_Atom_Buffer *abuf )
{
	if (abuf->aseq.atom.type == abuf->sequence_type)
		return abuf->aseq.atom.size - sizeof(LV2_Atom_Sequence_Body);
	else
		return 0;
}


// Return the actual LV2 atom:Sequence implementation.
//
static inline
LV2_Atom_Sequence *lv2_atom_buffer_get_sequence ( LV2_Atom_Buffer *abuf )
{
	return &abuf->aseq;
}


// An iterator over an atom:Sequence buffer.
//
typedef
struct _LV2_Atom_Buffer_Iterator
{
	LV2_Atom_Buffer *abuf;
	uint32_t offset;

} LV2_Atom_Buffer_Iterator;


// Reset an iterator to point to the start of an LV2 atom:Sequence buffer.
//
static inline
bool lv2_atom_buffer_begin (
	LV2_Atom_Buffer_Iterator *iter, LV2_Atom_Buffer *abuf )
{
	iter->abuf = abuf;
	iter->offset = 0;

	return (abuf->aseq.atom.size > 0);
}


// Reset an iterator to point to the end of an LV2 atom:Sequence buffer.
//
static inline
bool lv2_atom_buffer_end (
	LV2_Atom_Buffer_Iterator *iter, LV2_Atom_Buffer *abuf )
{
	iter->abuf = abuf;
	iter->offset = lv2_atom_buffer_pad_size(lv2_atom_buffer_get_size(abuf));

	return (iter->offset < abuf->capacity - sizeof(LV2_Atom_Event));
}


// Check if a LV2 atom:Sequenece buffer iterator is valid.
//
static inline
bool lv2_atom_buffer_is_valid ( LV2_Atom_Buffer_Iterator *iter )
{
	return iter->offset < lv2_atom_buffer_get_size(iter->abuf);
}


// Advance a LV2 atom:Sequenece buffer iterator forward one event.
//
static inline
bool lv2_atom_buffer_increment ( LV2_Atom_Buffer_Iterator *iter )
{
	if (!lv2_atom_buffer_is_valid(iter))
		return false;

	LV2_Atom_Buffer *abuf = iter->abuf;
	LV2_Atom_Sequence *aseq = &abuf->aseq;
	uint32_t size = ((LV2_Atom_Event *) ((char *)
		LV2_ATOM_CONTENTS(LV2_Atom_Sequence, aseq) + iter->offset))->body.size;
	iter->offset += lv2_atom_buffer_pad_size(sizeof(LV2_Atom_Event) + size);

	return true;
}


// Get the event currently pointed at a LV2 atom:Sequence buffer iterator.
//
static inline
LV2_Atom_Event *lv2_atom_buffer_get (
	LV2_Atom_Buffer_Iterator *iter, uint8_t **data )
{
	if (!lv2_atom_buffer_is_valid(iter))
		return NULL;

	LV2_Atom_Buffer *abuf = iter->abuf;
	LV2_Atom_Sequence *aseq = &abuf->aseq;
	LV2_Atom_Event *aev = (LV2_Atom_Event *) ((char *)
		LV2_ATOM_CONTENTS(LV2_Atom_Sequence, aseq) + iter->offset);

	*data = (uint8_t *) LV2_ATOM_BODY(&aev->body);

	return aev;
}


// Write an event at a LV2 atom:Sequence buffer iterator.
//
#if defined(Q_CC_GNU) || defined(Q_CC_MINGW)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstringop-overflow"
#endif

static inline
bool lv2_atom_buffer_write (
	LV2_Atom_Buffer_Iterator *iter,
	uint32_t frames,
	uint32_t /*subframes*/,
	uint32_t type,
	uint32_t size,
	const uint8_t *data )
{
	LV2_Atom_Buffer *abuf = iter->abuf;
	LV2_Atom_Sequence *aseq = &abuf->aseq;
	if  (abuf->capacity - sizeof(LV2_Atom) - aseq->atom.size
		< sizeof(LV2_Atom_Event) + size)
		return false;

	LV2_Atom_Event *aev = (LV2_Atom_Event*) ((char *)
		LV2_ATOM_CONTENTS(LV2_Atom_Sequence, aseq) + iter->offset);

	aev->time.frames = frames;
	aev->body.type = type;
	aev->body.size = size;

	memcpy(LV2_ATOM_BODY(&aev->body), data, size);

	size = lv2_atom_buffer_pad_size(sizeof(LV2_Atom_Event) + size);
	aseq->atom.size += size;
	iter->offset += size;

	return true;
}

#if defined(Q_CC_GNU) || defined(Q_CC_MINGW)
#pragma GCC diagnostic pop
#endif


#endif	// LV2_ATOM_HELPERS_H

// end of lv2_atom_helpers.h
