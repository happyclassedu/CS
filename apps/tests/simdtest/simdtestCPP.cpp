/*
Copyright (C) 2007 by Michael Gist

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Library General Public
License as published by the Free Software Foundation; either
version 2 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Library General Public License for more details.

You should have received a copy of the GNU Library General Public
License along with this library; if not, write to the Free
Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#

#include "cssysdef.h"

#include "csutil/processor/simdtypes.h"

#include "simdtest.h"
#include "simdfunc.h"

/*
 * File needed for GCC, because we need to compile this cpp with the relevant cflags.
 * GCC is a PITA :P
 */
bool SIMDTest::testCPP(float* a, float* b, float* c, int size)
{
    return SIMDFunc(a, b, c, size);
}
