#ifndef __INSTP_H__
#define __INSTP_H__

#include "csutil/scf.h"
#include "isystem.h"
#include "iplugin.h"
#include "icmdmgr.h"
#include "inetcmd.h"

#define PROTOCOL_SETUP_OK     (1)
#define PROTOCOL_CONNECT_FAIL (2)
#define PROTOCOL_CONNECT_OK   (3)
#define PROTOCOL_HANDLED_OK   (4)

SCF_VERSION (iProto, 0, 0, 1);

struct iProto: public iBase
{
  public:
    virtual int GetStatus () =0;
    virtual void SetStatus(int status) =0;
};


SCF_VERSION (iPROTO, 0, 0, 3);

struct iPROTO: public iPlugIn
{
  virtual bool Initialize (iSystem *iSys) = 0;
  virtual bool Open()=0;
  virtual bool Close()=0;

  /// Various Network Events coming from network that need to be handled
  virtual int OnServerStart(int NetPort) = 0;
  virtual int OnServerStop(int NetPort) = 0;
  virtual int OnConnect(int NetPort) = 0;
  virtual int OnAccept(int NetPort) = 0;
  virtual int OnReceive(int NetPort, size_t len, const char *msg) = 0;
  virtual int OnDisconnect(int port) = 0;

  /// Command processor - for things coming from above
  virtual int OnCmd(int NetPort, iNetCmd *cmd) = 0;

  /// Create an outbound protocol "pipe" connection
  virtual int InitiateClient(const char *hostname,
			     int port,
			     int clienttype) = 0;

  /// Create a server that listens
  virtual int InitiateServer(const char *hostname, 
			     int ipPortNumber,
			     int ServerType,
			     int MaxConnections) = 0;

  /// Setup Network Command Manager. - this wires the protocol upwards
  /// For any call backs required.
  virtual bool AddCmdManager(iCMDMGR *NewCmdManager) = 0;

  /// Utility stuff
  virtual int GetLastError() = 0;

  /// Get an integer associated with protocol.
  virtual int GetProtocolVersion() =0;

  /// Create a new net-command object
  virtual iNetCmd *NewCommand(int nodetype) const = 0;
  /// Create a new net-command object
  virtual iNetCmd *NewCommand(int nodetype, const char *name) const = 0;
  /// Create a new net-command object
  virtual iNetCmd *NewCommand(int nodetype, const char *name, int value) const = 0;
  /// Create a new net-command object
  virtual iNetCmd *NewCommand(int nodetype, const char *name, const char *value) const = 0;
  /// Create a new net-command object
  virtual iNetCmd *NewCommand(int nodetype, const char *name, float value) const = 0;
};

#endif 
