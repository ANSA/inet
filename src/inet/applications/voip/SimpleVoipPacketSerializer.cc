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
	stream.writeUint32Be(simpleVoipPacket->getTalkspurtID());
	stream.writeUint32Be(simpleVoipPacket->getTalkspurtNumPackets());
	stream.writeUint32Be(simpleVoipPacket->getPacketID());
	stream.writeUint64Be(simpleVoipPacket->getVoipTimestamp().raw());
	stream.writeUint64Be(simpleVoipPacket->getVoiceDuration().raw());
	ASSERT(simpleVoipPacket->getChunkLength() == B(28));
}

const Ptr<Chunk> SimpleVoipPacketSerializer::deserialize(MemoryInputStream& stream) const
{
	auto simpleVoipPacket = makeShared<SimpleVoipPacket>();
	simpleVoipPacket->setTalkspurtID(stream.readUint32Be());
	simpleVoipPacket->setTalkspurtNumPackets(stream.readUint32Be());
	simpleVoipPacket->setPacketID(stream.readUint32Be());
	simpleVoipPacket->setVoipTimestamp(SimTime().setRaw(stream.readUint64Be()));
	simpleVoipPacket->setVoiceDuration(SimTime().setRaw(stream.readUint64Be()));
	simpleVoipPacket->setChunkLength(B(28));
	return simpleVoipPacket;
}

} // namespace inet


















