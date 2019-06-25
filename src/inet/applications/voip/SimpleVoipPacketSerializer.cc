//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/.
//

#include "inet/common/packet/serializer/ChunkSerializerRegistry.h"
#include "inet/applications/voip/SimpleVoipPacket_m.h"
#include "inet/applications/voip/SimpleVoipPacketSerializer.h"

namespace inet {

Register_Serializer(SimpleVoipPacket, SimpleVoipPacketSerializer);

void SimpleVoipPacketSerializer::serialize(MemoryOutputStream& stream, const Ptr<const Chunk>& chunk) const
{
	const auto& simpleVoipPacket = staticPtrCast<const SimpleVoipPacket>(chunk);
	stream.writeUint16Be(simpleVoipPacket->getTalkspurtID());
	stream.writeUint16Be(simpleVoipPacket->getTalkspurtNumPackets());
	stream.writeUint16Be(simpleVoipPacket->getPacketID());

	//simtime_t timeStamp = simpleVoipPacket->getVoipTimestamp();
	//stream.writeUint64Be(timeStamp->inUnit(SIMTIME_AS));
	//SimTime(int64_t value, SimTimeUnit unit);
	//stream.writeUint32Be(timeStamp->getScaleExp());
	//static void setScaleExp(int e);
	// from scaleexp the following values can be calculated:
	//	dscale
	//	fscale
	//	invfscale
	stream.writeUint64Be(simpleVoipPacket->getVoipTimestamp().raw());

	//simtime_t voiceDuration = simpleVoipPacket->getVoiceDuration();
	//stream.writeUint64Be(voiceDuration->inUnit(SIMTIME_AS));
	stream.writeUint64Be(simpleVoipPacket->getVoiceDuration().raw());
}

const Ptr<Chunk> SimpleVoipPacketSerializer::deserialize(MemoryInputStream& stream) const
{
	auto simpleVoipPacket = makeShared<SimpleVoipPacket>();
	simpleVoipPacket->setTalkspurtID(stream.readUint16Be());
	simpleVoipPacket->setTalkspurtNumPackets(stream.readUint16Be());
	simpleVoipPacket->setPacketID(stream.readUint16Be());

	//simtime_t timeStamp = SimTime(stream.readUint64Be(), SIMTIME_AS);
	//simpleVoipPacket->setVoipTimestamp(timeStamp);
	simpleVoipPacket->setVoipTimestamp(SimTime().setRaw(stream.readUint64Be()));

	//simtime_t voiceDuration = SimTime(stream.readUint64Be(), SIMTIME_AS);
	//simpleVoipPacket->setVoiceDuration(voiceDuration);
	simpleVoipPacket->setVoiceDuration(SimTime().setRaw(stream.readUint64Be()));
}

} // namespace inet


















