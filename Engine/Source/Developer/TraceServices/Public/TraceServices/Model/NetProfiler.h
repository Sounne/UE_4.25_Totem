// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Templates/Function.h"
#include "TraceServices/Containers/Tables.h"
#include "TraceServices/Model/AnalysisSession.h"
#include <limits>

namespace Trace
{

enum class ENetProfilerDeliveryStatus : uint8
{
	Unknown,
	Delivered,
	Dropped
};

enum ENetProfilerConnectionMode : uint8
{
	Outgoing = 0,
	Incoming = 1,
	Count
};

struct FNetProfilerName
{
	const TCHAR* Name;	// Name
	uint32 NameIndex;	// Index in the type array?
};

// This is our event type separate per level as the same name might be used on different levels
struct FNetProfilerEventType
{
	uint32 EventTypeIndex;
	const TCHAR* Name;
	uint32 NameIndex : 16;
	uint32 Level : 16;
};

typedef double FNetProfilerTimeStamp;

struct FNetProfilerLifeTime
{
	FNetProfilerTimeStamp Begin = 0;
	FNetProfilerTimeStamp End = std::numeric_limits<double>::infinity();
};

//struct FNetProfilerProtocol
//{
//	const TCHAR* Name;
//	uint32 ProtocolIndex;
//	uint64 ProtocolIdentifier;
//};

struct FNetProfilerObjectInstance
{
	uint32 ObjectIndex = 0U;		// Index in the object array
	uint16 NameIndex = 0U;			// Index in the Name array
	uint64 TypeId = uint64(0);		// ProtocolIdentifier
	uint32 NetId = 0U;				// NetHandleIndex or NetGUID
	FNetProfilerLifeTime LifeTime;	// Lifetime of this instance
};

struct FNetProfilerContentEvent
{
	uint64 StartPos : 24;		// Inclusive start position in the packet
	uint64 EndPos : 24;			// Exclusive end position in the packet; BitSize = EndPos - StartPos
	uint64 Level : 4;			// Level
	uint64 Padding : 12;		// Padding

	uint32 EventTypeIndex;		// Will replace name index
	uint32 NameIndex;			// identify the name / type, should we store the actual Name as well
	uint32 ObjectInstanceIndex;	// object instance, Non zero if this is a NetObject, we can then look up data by indexing into ObjectInstances

	uint32 ParentIndex;			// Parent to be able to build a tree of nested events?
};

struct FNetProfilerPacket
{
	FNetProfilerTimeStamp TimeStamp;
	uint32 SequenceNumber;
	uint32 ContentSizeInBits;						// This is the part that is tracked by the PacketContents
	uint32 TotalPacketSizeInBytes;					// This is the actual size of the packet sent on the socket
	ENetProfilerDeliveryStatus DeliveryStatus;		// Indicates if the packet was delivered or not, updated as soon as we know

	// Index into Events
	uint32 StartEventIndex;
	uint32 EventCount;
};

struct FNetProfilerConnection
{
	const TCHAR* Name;
	FNetProfilerLifeTime LifeTime;
	uint32 GameInstanceIndex;
	uint32 ConnectionIndex : 16;
	uint32 ConnectionId : 14;
	uint32 bHasIncomingData : 1;
	uint32 bHasOutgoingData : 1;
};

struct FNetProfilerGameInstance
{
	FNetProfilerLifeTime LifeTime;
	uint32 GameInstanceIndex;
	uint32 GameInstanceId;
};

// What do we need?
struct FNetProfilerAggregatedStats
{
	uint32 EventTypeIndex;
	uint32 InstanceCount = 0U;
	uint32 TotalInclusive = 0U;
	uint32 MaxInclusive = 0U;
	uint32 AverageInclusive = 0U;
	uint32 TotalExclusive = 0U;
	uint32 MaxExclusive = 0U;
};

// What queries do we need?
class INetProfilerProvider : public IProvider
{
public:
	// Return the version reported in the trace
	// A return value of 0 indicates no network trace data
	virtual uint32 GetNetTraceVersion() const = 0;

	// Access Names
	virtual uint32 GetNameCount() const = 0;
	virtual void ReadNames(TFunctionRef<void(const FNetProfilerName*, uint64)> Callback) const = 0;
	virtual void ReadName(uint32 NameIndex, TFunctionRef<void(const FNetProfilerName&)> Callback) const = 0;

	// Access EventTypes
	virtual uint32 GetEventTypesCount() const = 0;
	virtual void ReadEventTypes(TFunctionRef<void(const FNetProfilerEventType*, uint64)> Callback) const = 0;
	virtual void ReadEventType(uint32 EventTypeIndex, TFunctionRef<void(const FNetProfilerEventType&)> Callback) const = 0;

	// Access GameInstances
	virtual uint32 GetGameInstanceCount() const = 0;
	virtual void ReadGameInstances(TFunctionRef<void(const FNetProfilerGameInstance&)> Callback) const = 0;

	// Access Connections
	virtual uint32 GetConnectionCount(uint32 GameInstanceIndex) const = 0;
	virtual void ReadConnections(uint32 GameInstanceIndex, TFunctionRef<void(const FNetProfilerConnection&)> Callback) const = 0;
	virtual void ReadConnection(uint32 ConnectionIndex, TFunctionRef<void(const FNetProfilerConnection&)> Callback) const = 0;
	virtual uint32 GetConnectionChangeCount() const = 0;

	// Access Object Instances
	virtual uint32 GetObjectCount(uint32 GameInstanceIndex) const = 0;
	virtual void ReadObjects(uint32 GameInstanceIndex, TFunctionRef<void(const FNetProfilerObjectInstance&)> Callback) const = 0;
	virtual void ReadObject(uint32 GameInstanceIndex, uint32 ObjectIndex, TFunctionRef<void(const FNetProfilerObjectInstance&)> Callback) const = 0;
	virtual uint32 GetObjectsChangeCount(uint32 GameInstanceIndex) const = 0;

	// Find Packet Index from SequenceNumber
	virtual int32 FindPacketIndexFromPacketSequence(uint32 ConnectionIndex, ENetProfilerConnectionMode Mode, uint32 SequenceNumber) const = 0;

	// Gets the number of packets for the specified connection and connection mode.
	virtual uint32 GetPacketCount(uint32 ConnectionIndex, ENetProfilerConnectionMode Mode) const = 0;

	// Enumerates packets in the provided inclusive packet interval, [PacketIndexIntervalStart, PacketIndexIntervalEnd].
	virtual void EnumeratePackets(uint32 ConnectionIndex, ENetProfilerConnectionMode Mode, uint32 PacketIndexIntervalStart, uint32 PacketIndexIntervalEnd, TFunctionRef<void(const FNetProfilerPacket&)> Callback) const = 0;
	// Returns a change number incremented each time a change occurs in the packets for the specified connection and connection mode.
	virtual uint32 GetPacketChangeCount(uint32 ConnectionIndex, ENetProfilerConnectionMode Mode) const = 0;

	// Enumerates packet content events in the inclusive event index interval [StartEventIndex, EndEventIndex]. */
	virtual void EnumeratePacketContentEventsByIndex(uint32 ConnectionIndex, ENetProfilerConnectionMode Mode, uint32 StartEventIndex, uint32 EndEventIndex, TFunctionRef<void(const FNetProfilerContentEvent&)> Callback) const = 0;
	// Enumerates packet content events for a packet, in the exclusive bit range [StartPosition, EndPosition). */
	virtual void EnumeratePacketContentEventsByPosition(uint32 ConnectionIndex, ENetProfilerConnectionMode Mode, uint32 PacketIndex, uint32 StartPosition, uint32 EndPosition, TFunctionRef<void(const FNetProfilerContentEvent&)> Callback) const = 0;
	// Returns a change number incremented each time a change occurs in the packet content events for the specified connection and connection mode. */
	virtual uint32 GetPacketContentEventChangeCount(uint32 ConnectionIndex, ENetProfilerConnectionMode Mode) const = 0;

	// Computes aggregated stats for a packet interval or for a range of content events in a single packet.
	// [PacketIndexIntervalStart, PacketIndexIntervalEnd] is the inclusive packet interval.
	// [StartPosition, EndPosition) is the exclusive bit range interval; only used when PacketIndexIntervalStart == PacketIndexIntervalEnd.
	virtual ITable<FNetProfilerAggregatedStats>* CreateAggregation(uint32 ConnectionIndex, ENetProfilerConnectionMode Mode, uint32 PacketIndexIntervalStart, uint32 PacketIndexIntervalEnd, uint32 StartPosition, uint32 EndPosition) const = 0;
};

TRACESERVICES_API const INetProfilerProvider& ReadNetProfilerProvider(const IAnalysisSession& Session);

}
