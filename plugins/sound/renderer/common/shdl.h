/*
    Copyright (C) 2001 by Martin Geisse <mgeisse@gmx.net>

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

#ifndef __SHDL_H__
#define __SHDL_H__

#include "isound\handle.h"
struct iSoundData;

class csSoundHandle : public iSoundHandle
{
public:
  DECLARE_IBASE;
  // the sound data for this handle
  iSoundData *Data;
  // is this sound registered?
  bool Registered;
  // This is a streamed sound and the stream is started
  bool ActiveStream;
  // loop the stream
  bool LoopStream;

  // constructor
  csSoundHandle(iSoundData *);
  // destructor
  ~csSoundHandle();
  // release the sound data
  void ReleaseSoundData();
  // update the sound handle
  void Update_Time(cs_time ElapsedTime);
  void Update_Num(long NumSamples);
  // implementation-specific update method
  virtual void vUpdate(void *buf, long NumSamples) = 0;

  // is this a static or streamed sound?
  virtual bool IsStatic();
  // play an instance of this sound
  virtual void Play(bool Loop);
  // Start playing the stream (only for streamed sound)
  virtual void StartStream(bool Loop);
  // Stop playing the stream
  virtual void StopStream();
  // Reset the stream to the beginning
  virtual void ResetStream();
};

#endif // __SHDL_H__
