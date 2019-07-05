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
#include "inet/linklayer/ieee8021d/common/Ieee8021dBpdu_m.h"
#include "inet/linklayer/ieee8021d/common/Ieee8021dBpduSerializer.h"

namespace inet {

Register_Serializer(Bpdu, Ieee8021dBpduSerializer);

void Ieee8021dBpduSerializer::serialize(MemoryOutputStream& stream, const Ptr<const Chunk>& chunk) const
{
    B start = B(stream.getLength());
    const auto& bpdu = staticPtrCast<const Bpdu>(chunk);
    stream.writeUint16Be(bpdu->getProtocolIdentifier());
    stream.writeByte(bpdu->getProtocolVersionIdentifier());
    stream.writeByte(bpdu->getBpduType());
    stream.writeBit(bpdu->getTcaFlag());
    stream.writeBitRepeatedly(false, 6);
    stream.writeBit(bpdu->getTcFlag());
    stream.writeMacAddress(bpdu->getRootAddress());
    stream.writeUint16Be(bpdu->getRootPriority());
    stream.writeUint64Be(bpdu->getRootPathCost());
    stream.writeMacAddress(bpdu->getBridgeAddress());
    stream.writeUint16Be(bpdu->getBridgePriority());
    stream.writeByte(bpdu->getPortNum());
    stream.writeByte(bpdu->getPortPriority());
    stream.writeUint16Be(bpdu->getMessageAge().inUnit(SIMTIME_S));
    stream.writeUint16Be(bpdu->getMaxAge().inUnit(SIMTIME_S));
    stream.writeUint16Be(bpdu->getHelloTime().inUnit(SIMTIME_S));
    stream.writeUint16Be(bpdu->getForwardDelay().inUnit(SIMTIME_S));
    // FIXME: temporary fix for rstp example
    if (bpdu->getChunkLength().get() > (stream.getLength() - start).get())
        stream.writeByteRepeatedly(0, (bpdu->getChunkLength() - (stream.getLength() - start)).get() / 8);
}

const Ptr<Chunk> Ieee8021dBpduSerializer::deserialize(MemoryInputStream& stream) const
{
    auto bpdu = makeShared<Bpdu>();
    bpdu->setProtocolIdentifier(stream.readUint16Be());
    bpdu->setProtocolVersionIdentifier(stream.readByte());
    bpdu->setBpduType(stream.readByte());
    bpdu->setTcaFlag(stream.readBit());
    if (!stream.readBitRepeatedly(false, 6))
        bpdu->markIncorrect();
    bpdu->setTcFlag(stream.readBit());
    bpdu->setRootAddress(stream.readMacAddress());
    bpdu->setRootPriority(stream.readUint16Be());
    bpdu->setRootPathCost(stream.readUint64Be());
    bpdu->setBridgeAddress(stream.readMacAddress());
    bpdu->setBridgePriority(stream.readUint16Be());
    bpdu->setPortNum(stream.readByte());
    bpdu->setPortPriority(stream.readByte());
    bpdu->setMessageAge(SimTime(stream.readUint16Be(), SIMTIME_S));
    bpdu->setMaxAge(SimTime(stream.readUint16Be(), SIMTIME_S));
    bpdu->setHelloTime(SimTime(stream.readUint16Be(), SIMTIME_S));
    bpdu->setForwardDelay(SimTime(stream.readUint16Be(), SIMTIME_S));
    return bpdu;
}

} // namespace inet
