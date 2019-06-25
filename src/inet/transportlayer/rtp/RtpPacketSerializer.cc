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
#include "inet/transportlayer/rtp/RtpPacket_m.h"
#include "inet/transportlayer/rtp/RtpPacketSerializer.h"

namespace inet::rtp {

Register_Serializer(RtpHeader, RtpPacketSerializer);

void RtpPacketSerializer::serialize(MemoryOutputStream& stream, const Ptr<const Chunk>& chunk) const
{
    const auto& rtpHeader = staticPtrCast<const RtpHeader>(chunk);
    stream.writeBit((rtpHeader->getVersion() & 2) == 2);
    stream.writeBit((rtpHeader->getVersion() & 1) == 1);
    stream.writeBit(rtpHeader->getPadding());
    stream.writeBit(rtpHeader->getExtension());
    stream.writeUint4(rtpHeader->getCsrcArraySize());
    stream.writeBit(rtpHeader->getMarker());

    uint8_t m = 64;
    for(uint8_t i = 0; i < 7; ++i){
        stream.writeBit((rtpHeader->getPayloadType() & m) == m);
        m /= 2;
    }

    stream.writeUint16Be(rtpHeader->getSequenceNumber());
    stream.writeUint32Be(rtpHeader->getTimeStamp());
    stream.writeUint32Be(rtpHeader->getSsrc());

    for(uint8_t i = 0; i < rtpHeader->getCsrcArraySize(); ++i){
        stream.writeUint32Be(rtpHeader->getCsrc(i));
    }
}

const Ptr<Chunk> RtpPacketSerializer::deserialize(MemoryInputStream& stream) const
{
    auto rtpHeader = makeShared<RtpHeader>();
    uint8_t version = stream.readBit() ? 2 : 0;
    version += stream.readBit() ? 1 : 0;
    rtpHeader->setVersion(version);
    rtpHeader->setPadding(stream.readBit());
    rtpHeader->setExtension(stream.readBit());
    rtpHeader->setCsrcArraySize(stream.readUint4());
    rtpHeader->setMarker(stream.readBit());

    uint8_t m = 64;
    uint8_t payloadType = 0;
    for(uint8_t i = 0; i < 7; ++i){
        payloadType += stream.readBit() ? m : 0;
        m /= 2;
    }
    rtpHeader->setPayloadType(payloadType);

    rtpHeader->setSequenceNumber(stream.readUint16Be());
    rtpHeader->setTimeStamp(stream.readUint32Be());
    rtpHeader->setSsrc(stream.readUint32Be());

    for(uint8_t i = 0; i < rtpHeader->getCsrcArraySize(); ++i){
        rtpHeader->setCsrc(i, stream.readUint32Be());
    }
    return rtpHeader;
}

} // namespace inet::rtp


















