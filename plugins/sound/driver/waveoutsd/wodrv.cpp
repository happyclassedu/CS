/*
	Copyright (C) 1998, 1999 by Nathaniel 'NooTe' Saint Martin
	Copyright (C) 1998, 1999 by Jorrit Tyberghein
	Written by Nathaniel 'NooTe' Saint Martin

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

#include "cssysdef.h"
#include "csutil/sysfunc.h"
#include <stdio.h>

#include "csutil/scf.h"
#include "csutil/csuctransform.h"
#include "isound/renderer.h"
#include "iutil/cfgfile.h"
#include "iutil/objreg.h"
#include "iutil/event.h"
#include "iutil/eventq.h"
#include "ivaria/reporter.h"
#include "wodrv.h"

#include "csutil/win32/wintools.h"

CS_IMPLEMENT_PLUGIN

SCF_IMPLEMENT_FACTORY (csSoundDriverWaveOut)


SCF_IMPLEMENT_IBASE(csSoundDriverWaveOut)
  SCF_IMPLEMENTS_INTERFACE(iSoundDriver)
  SCF_IMPLEMENTS_EMBEDDED_INTERFACE(iComponent)
SCF_IMPLEMENT_IBASE_END

SCF_IMPLEMENT_EMBEDDED_IBASE (csSoundDriverWaveOut::eiComponent)
  SCF_IMPLEMENTS_INTERFACE (iComponent)
SCF_IMPLEMENT_EMBEDDED_IBASE_END

SCF_IMPLEMENT_IBASE (csSoundDriverWaveOut::EventHandler)
  SCF_IMPLEMENTS_INTERFACE (iEventHandler)
SCF_IMPLEMENT_IBASE_END

#define WAVEOUT_BUFFER_COUNT 5


csSoundDriverWaveOut::csSoundDriverWaveOut(iBase *piBase)
{
  SCF_CONSTRUCT_IBASE(piBase);
  SCF_CONSTRUCT_EMBEDDED_IBASE(scfiComponent);
  scfiEventHandler = 0;

  object_reg = 0;
  SoundRender = 0;
  MemorySize = 0;
  Memory = 0;
}

csSoundDriverWaveOut::~csSoundDriverWaveOut()
{
  if (scfiEventHandler)
  {
    csRef<iEventQueue> q (CS_QUERY_REGISTRY(object_reg, iEventQueue));
    if (q != 0)
      q->RemoveListener (scfiEventHandler);
    scfiEventHandler->DecRef ();
  }
  SCF_DESTRUCT_EMBEDDED_IBASE(scfiComponent);
  SCF_DESTRUCT_IBASE();
}

bool csSoundDriverWaveOut::Initialize (iObjectRegistry *r)
{
  object_reg = r;
  if (!scfiEventHandler)
    scfiEventHandler = new EventHandler (this);
  csRef<iEventQueue> q (CS_QUERY_REGISTRY(object_reg, iEventQueue));
  if (q != 0)
    q->RegisterListener (scfiEventHandler, CSMASK_Nothing | CSMASK_Broadcast);
  Config.AddConfig(object_reg, "/config/sound.cfg");

  // Create mutexes to lock access to the various block exchange lists
  mutex_EmptyBlocks = csMutex::Create(true);


  return true;
}

void csSoundDriverWaveOut::Report (int severity, const char* msg, ...)
{
  va_list arg;
  va_start (arg, msg);
  csRef<iReporter> rep (CS_QUERY_REGISTRY (object_reg, iReporter));
  if (rep)
    rep->ReportV (severity, "crystalspace.sound.driver.waveout", msg, arg);
  else
  {
    csPrintfV (msg, arg);
    csPrintf ("\n");
  }
  va_end (arg);
}

bool csSoundDriverWaveOut::Open(iSoundRender *render, int frequency,
  bool bit16, bool stereo)
{
  Report (CS_REPORTER_SEVERITY_NOTIFY, "Wave-Out Sound Driver selected.");

  // store pointer to sound renderer
  if (!render) return false;
  SoundRender = render;
  SoundRender->IncRef();

  // store sound format settings
  Frequency = frequency;
  Bit16 = bit16;
  Stereo = stereo;

  // setup wave format
  Format.wFormatTag = WAVE_FORMAT_PCM;
  if (Stereo) Format.nChannels = 2;
  else Format.nChannels = 1;
  Format.nSamplesPerSec = Frequency;
  if (Bit16) Format.wBitsPerSample = 16;
  else Format.wBitsPerSample = 8;
  Format.nBlockAlign = Format.nChannels * Format.wBitsPerSample / 8;
  Format.nAvgBytesPerSec = Format.nBlockAlign * Format.nSamplesPerSec;
  Format.cbSize = 0;

  // read settings from the config file
  float BufferLength=Config->GetFloat("Sound.WaveOut.BufferLength", (float)0.05);
  Report (CS_REPORTER_SEVERITY_NOTIFY, "  pre-buffering %f seconds of sound",
    BufferLength);


  // setup misc member variables

  // We'll keep 5 buffer-segments.  Every time one is empty we'll refill it and send it along again
  float f_memsize=(BufferLength * Format.nAvgBytesPerSec) / WAVEOUT_BUFFER_COUNT;

  MemorySize = f_memsize; // Translate to an int
  MemorySize += Format.nBlockAlign; // Add the alignment size - this causes a round-up
  MemorySize -= (MemorySize%Format.nBlockAlign); // Truncate to an even alignment size
  Memory = 0;


  // Allocate the buffers
  int buffer_idx;
  
  AllocatedBlocks = new SoundBlock[WAVEOUT_BUFFER_COUNT];
  for (buffer_idx=0;buffer_idx<WAVEOUT_BUFFER_COUNT;buffer_idx++)
  {
    AllocatedBlocks[buffer_idx].Driver = this;
    AllocatedBlocks[buffer_idx].DataHandle = GlobalAlloc(GMEM_FIXED | GMEM_SHARE, sizeof(WAVEHDR) + MemorySize);
    AllocatedBlocks[buffer_idx].Data = (unsigned char *)GlobalLock(AllocatedBlocks[buffer_idx].DataHandle);
    AllocatedBlocks[buffer_idx].WaveHeader = (LPWAVEHDR)(AllocatedBlocks[buffer_idx].Data);

    // Zero out the block
    memset(AllocatedBlocks[buffer_idx].Data,0,sizeof(WAVEHDR) + MemorySize);

    EmptyBlocks.Push(&(AllocatedBlocks[buffer_idx]));
  }

  // Start the background thread
  bgThread=new BackgroundThread (this);
  csbgThread=csThread::Create (bgThread);
  csbgThread->Start();

  // Store old volume and set full volume because the software sound renderer
  // will apply volume internally. If this device does not allow volume
  // changes, all the volume set/get calls will fail, but output will be
  // correct. So don't check for errors here!
//  waveOutGetVolume(WaveOut, &OldVolume);
//  waveOutSetVolume(WaveOut, 0xffffffff);


  return S_OK;
}

void csSoundDriverWaveOut::Close()
{
  int wait_timer=0;
  int got_lock=0;
  // Signal background thread to halt
  bgThread->RequestStop();

  while (bgThread->IsRunning() && wait_timer++<120000)
    csSleep(0);

  // Clear out the EmptyBlocks queue
  if (!mutex_EmptyBlocks->LockTry())
    Report (CS_REPORTER_SEVERITY_WARNING, "  Could not obtain EmptyBlocks lock during driver shutdown.  Clearing list without a lock.");
  else
    got_lock=1;

  while (EmptyBlocks.Length())
  {
    SoundBlock *Block=EmptyBlocks.Pop();
    GlobalUnlock(Block->DataHandle);
    GlobalFree(Block->DataHandle);
  }
  if (got_lock)
    mutex_EmptyBlocks->Release();

  delete AllocatedBlocks;
  AllocatedBlocks=NULL;

 
  if (SoundRender)
  {
    SoundRender->DecRef();
    SoundRender = 0;
  }

  Memory = 0;
}

void csSoundDriverWaveOut::LockMemory(void **mem, int *memsize)
{
  *mem = Memory;
  *memsize = MemorySize;
}

void csSoundDriverWaveOut::UnlockMemory() {}
bool csSoundDriverWaveOut::IsBackground() {return true;}
bool csSoundDriverWaveOut::Is16Bits() {return Bit16;}
bool csSoundDriverWaveOut::IsStereo() {return Stereo;}
bool csSoundDriverWaveOut::IsHandleVoidSound() {return false;}
int csSoundDriverWaveOut::GetFrequency() {return Frequency;}

bool csSoundDriverWaveOut::HandleEvent(iEvent &e)
{
  return false;
}

/*
 * Applications should not call any system-defined functions from inside a callback function, 
 * except for EnterCriticalSection, LeaveCriticalSection, midiOutLongMsg, midiOutShortMsg, 
 * OutputDebugString, PostMessage, PostThreadMessage, SetEvent, timeGetSystemTime, timeGetTime, 
 * timeKillEvent, and timeSetEvent. Calling other wave functions will cause deadlock.
*/
void CALLBACK csSoundDriverWaveOut::waveOutProc(HWAVEOUT /*WaveOut*/,
  UINT uMsg, DWORD /*dwInstance*/, DWORD dwParam1, DWORD /*dwParam2*/)
{
  if (uMsg==MM_WOM_DONE) {
    LPWAVEHDR OldHeader = (LPWAVEHDR)dwParam1;
    if (OldHeader->dwUser == 0) return;
    SoundBlock *Block = (SoundBlock *)OldHeader->dwUser;
    Block->Driver->RecycleBlock(Block);
  }
}

void csSoundDriverWaveOut::RecycleBlock(SoundBlock *Block)
{
  mutex_EmptyBlocks->LockWait();
  EmptyBlocks.Push(Block);
  mutex_EmptyBlocks->Release();
}


const char *csSoundDriverWaveOut::BackgroundThread::GetMMError (MMRESULT r) 
{
  switch (r) 
  {
    case MMSYSERR_NOERROR:
      return "no MM error";

    case MMSYSERR_ALLOCATED:
      return "resource already allocated by an other program";

    case MMSYSERR_BADDEVICEID:
      return "bad device";

    case MMSYSERR_NODRIVER:
      return "there is no device";

    case MMSYSERR_NOMEM:
      return "unable to allocate memory";

    default:
      {
	static char err[MAXERRORLENGTH * CS_UC_MAX_UTF8_ENCODED];
	if (cswinIsWinNT ())
	{
	  static WCHAR wideErr[MAXERRORLENGTH];
          waveOutGetErrorTextW (r, wideErr, 
	    sizeof (wideErr) / sizeof (WCHAR));
	  csUnicodeTransform::WCtoUTF8 ((utf8_char*)err, 
	    sizeof (err) / sizeof (char), wideErr, (size_t)-1);
	}
	else
	{
	  static char ansiErr[MAXERRORLENGTH];
          waveOutGetErrorTextA (r, ansiErr, 
	    sizeof (ansiErr) / sizeof (char));
	  wchar_t* wideErr = cswinAnsiToWide (ansiErr);
	  csUnicodeTransform::WCtoUTF8 ((utf8_char*)err, 
	    sizeof (err) / sizeof (char), wideErr, (size_t)-1);
	  delete[] wideErr;
	}
	return err;
      }
  }
}

bool csSoundDriverWaveOut::BackgroundThread::CheckError(const char *action, MMRESULT code)
{
  if (code != MMSYSERR_NOERROR) 
  {
    if (code != LastError)
    {
      parent_driver->Report (CS_REPORTER_SEVERITY_ERROR,
	"%s: %.8x %s .", action, code, GetMMError (code));
      LastError = code;
    }
    return false;
  }
  else
  {
    return true;
  }
}

bool csSoundDriverWaveOut::BackgroundThread::FillBlock(SoundBlock *Block)
{
  LPWAVEHDR lpWaveHdr;
  MMRESULT result;

  // Unprepare the block, in case it was used previously.  Per MSDN an unprepare on a block that has
  //  not been previously prepared is safe - it does nothing and returns zero.
  lpWaveHdr = (LPWAVEHDR)Block->Data;
  result = waveOutUnprepareHeader(WaveOut, lpWaveHdr, sizeof(WAVEHDR));
  if (result != 0 && !CheckError("waveOutUnprepareHeader", result))
    return false;

  // Ensure the block pointers are correct
  Block->WaveHeader = (LPWAVEHDR)Block->Data;

  // Parent Memory pointer is used by the renderer to lock memory to write into
  //  It's a very circular process
  parent_driver->Memory = Block->Data + sizeof(WAVEHDR);

  // call the sound renderer mixing function
  parent_driver->SoundRender->MixingFunction();

  // Set up and prepare header.
  lpWaveHdr->lpData = (char *)parent_driver->Memory;
  lpWaveHdr->dwBufferLength = parent_driver->MemorySize;
  lpWaveHdr->dwFlags = 0L;
  lpWaveHdr->dwLoops = 0L;
  lpWaveHdr->dwUser = (DWORD)Block;

  result = waveOutPrepareHeader(WaveOut, lpWaveHdr, sizeof(WAVEHDR));
  if (!CheckError("waveOutPrepareHeader", result)) {
      return false;
  }
  else
  {
      // Now the data block can be sent to the output device. The
      // waveOutWrite function returns immediately and waveform
      // data is sent to the output device in the background.
      result = waveOutWrite(WaveOut, lpWaveHdr, sizeof(WAVEHDR));
      if (!CheckError("waveOutWrite", result)) {
        result = waveOutUnprepareHeader(WaveOut, lpWaveHdr, sizeof(WAVEHDR));
        CheckError("waveOutUnprepareHeader", result);
        return false;
      }
  }
  return true;
}

void csSoundDriverWaveOut::BackgroundThread::Run()
{
  MMRESULT result;
  int shutdown_wait_counter=0;
  running=true;

  // Startup waveOut device
  result = waveOutOpen (&WaveOut, WAVE_MAPPER, &(parent_driver->Format),
    (LONG)&waveOutProc, 0L, CALLBACK_FUNCTION);
  CheckError ("waveOutOpen", result);


  while (!request_stop)
  {
    int block_count;

    // Grab a lock on the available blocks list
    parent_driver->mutex_EmptyBlocks->LockWait();

    // Fill all available blocks
    block_count=parent_driver->EmptyBlocks.Length();
    while (block_count>0)
    {
      SoundBlock *CurrentBlock=parent_driver->EmptyBlocks.Pop();
      if (!FillBlock(CurrentBlock))
        parent_driver->EmptyBlocks.Insert(0,CurrentBlock);
      block_count--;
    }

    // Release the lock
    parent_driver->mutex_EmptyBlocks->Release();
    // Give up timeslice
    csSleep(0);
  }

  // Request all blocks to stop and be returned 
  result = waveOutReset(WaveOut);
  CheckError ("waveOutReset", result);

  // Need to wait for queued blocks to all be finished
  while (1)
  {
    // Dont stall forever
    if (shutdown_wait_counter++>100000)
      break; 

    // Grab a lock on the available blocks list
    parent_driver->mutex_EmptyBlocks->LockWait();

    if (parent_driver->EmptyBlocks.Length() >= WAVEOUT_BUFFER_COUNT)
    {
      // Release the lock
      parent_driver->mutex_EmptyBlocks->Release();
      break;
    }

    // Release the lock
    parent_driver->mutex_EmptyBlocks->Release();
    // Give up timeslice
    csSleep(0);
  }

  // Close the wave output device
  waveOutClose(WaveOut);

  running=false;
}
