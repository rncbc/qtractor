// qtractorClipFadeFunctor.cpp
//
/****************************************************************************
   Copyright (C) 2010, rncbc aka Rui Nuno Capela. All rights reserved.

   Adapted and refactored from Robert Penner's easing equations.
   Copyright (C) 2001, Robert Penner.

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
/*
   TERMS OF USE - EASING EQUATIONS
 
   Open source under the BSD License.
 
   Copyright (C) 2001 Robert Penner
   All rights reserved.
 
   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:
 
   * Redistributions of source code must retain the above copyright notice,
     this list of conditions and the following disclaimer.
   * Redistributions in binary form must reproduce the above copyright notice,
     this list of conditions and the following disclaimer in the documentation
     and/or other materials provided with the distribution.
   * Neither the name of the author nor the names of contributors may be used
     to endorse or promote products derived from this software without
     specific prior written permission.
 
   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
   AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
   THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
   PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
   CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
   EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
   PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
   OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
   WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
   OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
   ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*/

#include "qtractorClipFadeFunctor.h"


//---------------------------------------------------------------------------
//  Functor argument normalization.
//
//	t = (x - x0) / (x1 - x0)
//  a = y1 - y0
//  b = y0
//

// Linear fade.
//
struct FadeLinear
{
	float operator() (float t, float a, float b) const
		{ return a * t + b; }
};


// Quadratic (t^2) fade in: accelerating from zero velocity.
//
struct FadeInQuad
{
	float operator() (float t, float a, float b) const
		{ return a * (t * t) + b; }
};


// Quadratic (t^2) fade out: decelerating to zero velocity.
//
struct FadeOutQuad
{
	float operator() (float t, float a, float b) const
		{ return a * (t * (2.0f - t)) + b; }
};


// Quadratic (t^2) fade in-out: acceleration until halfway, then deceleration.
//
struct FadeInOutQuad
{
	float operator() (float t, float a, float b) const
	{
		t *= 2.0f;
		if (t < 1.0f) {
			return 0.5f * a * (t * t) + b;
		} else {
			t -= 1.0f;
			return 0.5f * a * (1.0f - (t * (t - 2.0f))) + b;
		}
	}
};


// Cubic (t^3) fade in: accelerating from zero velocity.
//
struct FadeInCubic
{
	float operator() (float t, float a, float b) const
		{ return a * (t * t * t) + b; }
};


// Cubic (t^3) fade out: decelerating from zero velocity.
//
struct FadeOutCubic
{
	float operator() (float t, float a, float b) const
		{ t -= 1.0f; return a * ((t * t * t) + 1.0f) + b; }
};


// Cubic (t^3) fade in-out: acceleration until halfway, then deceleration.
//
struct FadeInOutCubic
{
	float operator() (float t, float a, float b) const
	{
		t *= 2.0f;
		if (t < 1.0f) {
			return 0.5f * a * (t * t * t) + b;
		} else {
			t -= 2.0f;
			return 0.5f * a * ((t * t * t) + 2.0f) + b;
		}
	}
};


// Fade model class.
//
struct FadeMode
{
	FadeMode(float y0, float y1) : a(y1 - y0), b(y0) {}

	float a, b;
};


// Fade-in mode.
//
struct FadeInMode : public FadeMode
{
	FadeInMode() : FadeMode(0.0f, 1.0f) {}
};


// Fade-out mode.
//
struct FadeOutMode : public FadeMode
{
	FadeOutMode() : FadeMode(1.0f, 0.0f) {}
};


// Fade functor template class.
//
template<typename M, typename F>
class FadeCurve : public qtractorClip::FadeFunctor
{
public:

	FadeCurve() : m_mode(M()), m_func(F()) {}

	float operator() (float t) const
		{ return m_func(t, m_mode.a, m_mode.b); }

private:

	M m_mode;
	F m_func;
};


// Fade functor factory class (static).
//
qtractorClip::FadeFunctor *qtractorClip::createFadeFunctor (
	FadeMode fadeMode, FadeType fadeType )
{
	switch (fadeMode) {

	case FadeIn:
		switch (fadeType) {
		case Linear:
			return new FadeCurve<FadeInMode, FadeLinear> ();
		case InQuad:
			return new FadeCurve<FadeInMode, FadeInQuad> ();
		case OutQuad:
			return new FadeCurve<FadeInMode, FadeOutQuad> ();
		case InOutQuad:
			return new FadeCurve<FadeInMode, FadeInOutQuad> ();
		case InCubic:
			return new FadeCurve<FadeInMode, FadeInCubic> ();
		case OutCubic:
			return new FadeCurve<FadeInMode, FadeOutCubic> ();
		case InOutCubic:
			return new FadeCurve<FadeInMode, FadeInOutCubic> ();
		default:
			break;
		}
		break;

	case FadeOut:
		switch (fadeType) {
		case Linear:
			return new FadeCurve<FadeOutMode, FadeLinear> ();
		case InQuad:
			return new FadeCurve<FadeOutMode, FadeInQuad> ();
		case OutQuad:
			return new FadeCurve<FadeOutMode, FadeOutQuad> ();
		case InOutQuad:
			return new FadeCurve<FadeOutMode, FadeInOutQuad> ();
		case InCubic:
			return new FadeCurve<FadeOutMode, FadeInCubic> ();
		case OutCubic:
			return new FadeCurve<FadeOutMode, FadeOutCubic> ();
		case InOutCubic:
			return new FadeCurve<FadeOutMode, FadeInOutCubic> ();
		default:
			break;
		}
		break;

	default:
		break;
	}

	return NULL;
}


// end of qtractorClipFadeFunctor.cpp
