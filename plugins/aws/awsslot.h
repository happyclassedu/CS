#ifndef __AWS_SLOT_H__
#define __AWS_SLOT_H__

/**************************************************************************
    Copyright (C) 2000-2001 by Christopher Nelson 
    
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
*****************************************************************************/

#include "ivaria/aws.h"
#include "csutil/csvector.h"

/*********************************************************************************************************************
*                                                                                                                    *
*      This implements the signal source architecture.  A signal source is an object which knows how to register and *
*    unregister slots.  A signal source always defines and emits signals.  A signal source does not care if someone  *
*    is listening or not.                                                                                            *
*                                                                                                                    *
*********************************************************************************************************************/

class awsSink : public iAwsSink
{
  struct TriggerMap
  {
    unsigned long name;
    void (iBase::*trigger)(iAwsSource &source);

    TriggerMap(unsigned long n, void (iBase::*t)(iAwsSource &source)):name(n), trigger(t) {};
  };
    
  /// List of triggers registered.
  csBasicVector triggers;

public:
  awsSink();
  virtual ~awsSink();

  /// Maps a trigger name to a trigger id
  virtual unsigned long GetTriggerID(char *name);

  /// Handles trigger events
  virtual void HandleTrigger(int trigger, iAwsSource &source);

  /// A sink should call this to register trigger events
  virtual void RegisterTrigger(char *name, void (iBase::*Trigger)(iAwsSource &source));
};

class awsSource : public iAwsSource
{
   /// contains a list of all slots that we have registered
   csBasicVector slots;

   struct SlotSignalMap
   {
      /// The slot that's registered
      iAwsSlot *slot;

      /// The signal it's registered for
      unsigned long signal;
   };

public:  
    awsSource();

    virtual ~awsSource();

    /// Registers a slot for any one of the signals defined by a source.  Each sources's signals exist in it's own namespace
    virtual bool RegisterSlot(iAwsSlot *slot, unsigned long signal);

    /// Unregisters a slot for a signal.
    virtual bool UnregisterSlot(iAwsSlot *slot, unsigned long signal);

    /// Broadcasts a signal to all slots that are interested.
    virtual void Broadcast(unsigned long signal);
};

/**********************************************************************************************************************
*                                                                                                                     *
*   This implements the slot architecture.  Slots are conduits for signals.  Signals always have a source.  Slots may *
*  be a conduit for multiple signals from multiple sources.  It is up to the user to determine how to use his slots.  *
*  Slots do not care, other than they always activate a particular trigger of a particular sink.                      *
*                                                                                                                     *
**********************************************************************************************************************/
class awsSlot : public iAwsSlot
{
   /// The sink that this slot manages.
   iAwsSink *sink;
   
   /** A mapping between signals and triggers.  One signal may map to multiple triggers, or
    * vice versa.  The mapping list is traversed everytime there is a signal emitted.  All
    * mappings are evaluated and if they qualify, are activated. */
   struct SignalTriggerMap
   {
     unsigned long signal;
     unsigned long trigger;
     unsigned long refs;

     SignalTriggerMap(unsigned long s, unsigned long t, unsigned long r):signal(s), trigger(t), refs(r) {};
   };

   csBasicVector stmap;
  
public:
  /// Does nothing
  awsSlot();

  /// Also does nothing
  virtual ~awsSlot();

  /// Sets up the slot's sink
  virtual void Initialize(iAwsSink *sink);
  
  /// Creates a connection from the source:signal to the sink:trigger specified.
  virtual void Connect(iAwsSource &source, unsigned long signal, unsigned long trigger);
                                          
  /// Disconnects the slot from a signal source, also properly unmaps a signal::trigger binding.
  virtual void Disconnect(iAwsSource &source, unsigned long signal, unsigned long trigger);

  /// Emit a signal to activate all necessary triggers.
  virtual void Emit(iAwsSource &source, unsigned long signal);
};


#endif 

