// qtractorAtomic.h
//
/****************************************************************************
   Copyright (C) 2005-2013, rncbc aka Rui Nuno Capela. All rights reserved.

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

#ifndef __qtractorAtomic_h
#define __qtractorAtomic_h

#define HAVE_QATOMIC_H

#if defined(HAVE_QATOMIC_H)
#   include <qatomic.h>
#endif


#if defined(__cplusplus)
extern "C" {
#endif

#if defined(__GNUC__)

#if defined(powerpc) || defined(__ppc__)

static inline int ATOMIC_CAS1 (
	volatile int *pAddr, int iOldValue, int iNewValue )
{
	register int result;
	asm volatile (
		"# ATOMIC_CAS1			\n"
		"	lwarx	r0, 0, %1	\n"
		"	cmpw	r0, %2		\n"
		"	bne-	1f			\n"
		"	sync				\n"
		"	stwcx.	%3, 0, %1	\n"
		"	bne-	1f			\n"
		"	li		%0, 1		\n"
		"	b		2f			\n"
		"1:						\n"
		"	li		%0, 0		\n"
		"2:						\n"
		: "=r" (result)
		: "r" (pValue), "r" (iOldValue), "r" (iNewValue)
		: "r0"
	);
	return result;
}

#elif defined(__i386__) || defined(__x86_64__)

static inline int ATOMIC_CAS1 (
	volatile int *pValue, int iOldValue, int iNewValue )
{
	register char result;
	asm volatile (
		"# ATOMIC_CAS1			\n"
		"lock ; cmpxchgl %2, %3	\n"
		"sete %1				\n"
		: "=a" (iNewValue), "=qm" (result)
		: "r" (iNewValue), "m" (*pValue), "0" (iOldValue)
		: "memory"
	);
	return result;
}

#else
#   error "qtractorAtomic.h: unsupported target compiler processor (GNUC)."
#endif

#elif defined(WIN32) || defined(__WIN32__) || defined(_WIN32)

static inline int ATOMIC_CAS1 (
	volatile int *pValue, int iOldValue, int iNewValue )
{
	register char result;
	__asm {
		push	ebx
		push	esi
		mov		esi, pValue
		mov		eax, iOldValue
		mov		ebx, iNewValue
		lock	cmpxchg dword ptr [esi], ebx
		sete	result
		pop		esi
		pop		ebx
	}
	return result;
}

#else
#   error "qtractorAtomic.h: unsupported target compiler processor (WIN32)."
#endif


#if defined(HAVE_QATOMIC_H)

typedef QAtomicInt qtractorAtomic;

#if QT_VERSION >= 0x050000
#define ATOMIC_GET(a)	((a)->load())
#define ATOMIC_SET(a,v)	((a)->store(v))
#else
#define ATOMIC_GET(a)	((int) *(a))
#define ATOMIC_SET(a,v)	(*(a) = (v))
#endif

static inline int ATOMIC_CAS ( qtractorAtomic *pVal,
	int iOldValue, int iNewValue )
{
	return pVal->testAndSetOrdered(iOldValue, iNewValue);
}

#else

typedef struct { volatile int value; } qtractorAtomic;

#define ATOMIC_GET(a)	((a)->value)
#define ATOMIC_SET(a,v)	((a)->value = (v))

static inline int ATOMIC_CAS ( qtractorAtomic *pVal,
	int iOldValue, int iNewValue )
{
	return ATOMIC_CAS1(&(pVal->value), iOldValue, iNewValue);
}

#endif


// Strict test-and-set primite.
static inline int ATOMIC_TAS ( qtractorAtomic *pVal )
{
	return ATOMIC_CAS(pVal, 0, 1);
}

// Strict test-and-add primitive.
static inline int ATOMIC_ADD ( qtractorAtomic *pVal, int iAddValue )
{
	volatile int iOldValue, iNewValue;
	do {
		iOldValue = ATOMIC_GET(pVal);
		iNewValue = iOldValue + iAddValue;
	} while (!ATOMIC_CAS(pVal, iOldValue, iNewValue));
	return iNewValue;
}

#define ATOMIC_INC(a) ATOMIC_ADD((a), (+1))
#define ATOMIC_DEC(a) ATOMIC_ADD((a), (-1))

// Special test-and-zero primitive (invented here:)
static inline int ATOMIC_TAZ ( qtractorAtomic *pVal )
{
	volatile int iOldValue;
	do {
		iOldValue = ATOMIC_GET(pVal);
	} while (iOldValue && !ATOMIC_CAS(pVal, iOldValue, 0));
	return iOldValue;
}


#if defined(__cplusplus)
}
#endif


#endif // __qtractorAtomic_h

// end of qtractorAtomic.h
