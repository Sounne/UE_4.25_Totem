// Copyright Epic Games, Inc. All Rights Reserved.
//
// Read http://lucumr.pocoo.org/2012/9/24/websockets-101/ for a nice intro to web sockets.
// This uses https://libwebsockets.org/trac/libwebsockets
#pragma  once
#include "WebSocketNetworkingPrivate.h"

class FWebSocketServer
{
public:

	FWebSocketServer();
	~FWebSocketServer();

	/** Create a web socket server*/
	bool Init(uint32 Port, FWebSocketClientConnectedCallBack);

	/** Service libwebsocket */
	bool Tick();

	/** Describe this libwebsocket server */
	FString Info();

// this was made public because of cross-platform build issues
public:

	/** Callback for a new websocket connection to the server */
	FWebSocketClientConnectedCallBack  ConnectedCallBack;

	/** Internal libwebsocket context */
	WebSocketInternalContext* Context;

	/** Protocols serviced by this implementation */
	WebSocketInternalProtocol* Protocols;

	friend class FWebSocket;
	uint32 ServerPort;
};


