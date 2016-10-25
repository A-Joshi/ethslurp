#ifndef _H_RPC_
#define _H_RPC_

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
//#include <string>
#include <stdio.h>
#include "etherlib.h"
//#include <map>
//#include <json/value.h>
//#include <boost/test/unit_test.hpp>

//-----------------------------------------------------
class IPCSocket
{
public:
	                IPCSocket   ( SFString const& _path );
	               ~IPCSocket   ( );
	SFString        sendRequest ( SFString const& _req);

	SFString const& path        ( void ) const;

private:
	FILE *m_fp;
	SFString m_path;
	int m_socket;
};

//-----------------------------------------------------
IPCSocket::~IPCSocket(void)
{
	close(m_socket);
	fclose(m_fp);
}

//-----------------------------------------------------
SFString const& IPCSocket::path( void ) const
{
	return m_path;
}

//-----------------------------------------------------
class RPCSession
{
public:
	CReceipt           eth_getTransactionReceipt ( SFString const& _transactionHash );
	SFString           eth_getCode               ( SFString const& _address, SFString const& _blockNumber );
	SFString           eth_getBalance            ( SFString const& _address, SFString const& _blockNumber );
	SFString           eth_getStorageRoot        ( SFString const& _address, SFString const& _blockNumber );
	SFString           rpcCall                   (SFString const& _methodName, SFString const& _args);

private:
	                   RPCSession               (SFString const& _path);

	IPCSocket m_ipcSocket;
	size_t m_rpcSequence = 1;
};

#endif
