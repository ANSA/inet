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
#include "inet/transportlayer/rtp/RtcpPacket_m.h"
#include "inet/transportlayer/rtp/RtcpPacketSerializer.h"

// FIXME: I got lost with all the castings, not sure if it is correct

namespace inet {
namespace rtp {

Register_Serializer(RtcpPacket, RtcpPacketSerializer);

Register_Serializer(RtcpReceiverReportPacket, RtcpPacketSerializer);
Register_Serializer(RtcpSdesPacket, RtcpPacketSerializer);
Register_Serializer(RtcpByePacket, RtcpPacketSerializer);
Register_Serializer(RtcpSenderReportPacket, RtcpPacketSerializer);

void RtcpPacketSerializer::serialize(MemoryOutputStream& stream, const Ptr<const Chunk>& chunk) const
{
    throw cRuntimeError("RtcpPacketSerializer not fully implemented yet.");
    const auto& rtcpPacket = staticPtrCast<const RtcpPacket>(chunk);

    B start_position = B(stream.getLength());

    stream.writeNBitsOfUint64Be(rtcpPacket->getVersion(), 2);
    stream.writeBit(rtcpPacket->getPadding());
    stream.writeNBitsOfUint64Be(rtcpPacket->getCount(), 5);
    stream.writeByte(rtcpPacket->getPacketType());
    stream.writeUint16Be(rtcpPacket->getRtcpLength());
    switch(rtcpPacket->getPacketType()){
        case RTCP_PT_SR: {
            const auto& rtcpSenderReportPacket = staticPtrCast<const RtcpSenderReportPacket>(chunk);
            stream.writeUint32Be(rtcpSenderReportPacket->getSsrc());
            stream.writeUint64Be(rtcpSenderReportPacket->getSenderReport().getNTPTimeStamp());
            stream.writeUint32Be(rtcpSenderReportPacket->getSenderReport().getRTPTimeStamp());
            stream.writeUint32Be(rtcpSenderReportPacket->getSenderReport().getPacketCount());
            stream.writeUint32Be(rtcpSenderReportPacket->getSenderReport().getByteCount());
            int size = rtcpSenderReportPacket->getCount();
            for(int i = 0; i < size; ++i){
                const ReceptionReport* receptionReport = static_cast<const ReceptionReport*>(rtcpSenderReportPacket->getReceptionReports()[i]);
                if(receptionReport != nullptr){
                    stream.writeUint32Be(receptionReport->getSsrc());
                    stream.writeByte(receptionReport->getFractionLost());
                    stream.writeNBitsOfUint64Be(receptionReport->getPacketsLostCumulative(), 24);
                    stream.writeUint32Be(receptionReport->getSequenceNumber());
                    stream.writeUint32Be(receptionReport->getJitter());
                    stream.writeUint32Be(receptionReport->getLastSR());
                    stream.writeUint32Be(receptionReport->getDelaySinceLastSR());
                }
            }
            std::cout << "length of stream of rtcpSenderReportPacket: " << stream.getLength() - start_position << endl;
            std::cout << "chunkLength: " << rtcpSenderReportPacket->getChunkLength() << endl;
            std::cout << "my count: " << B(4) + B(24) + B(size * 24) << endl;
            ASSERT(rtcpSenderReportPacket->getChunkLength() == B(4) + B(24) + B(size * 24));
            break;
        }
        case RTCP_PT_RR: {
            const auto& rtcpReceiverReportPacket = staticPtrCast<const RtcpReceiverReportPacket>(chunk);
            stream.writeUint32Be(rtcpReceiverReportPacket->getSsrc());
            int size = rtcpReceiverReportPacket->getCount();
            for(int i = 0; i < size; ++i){
                const ReceptionReport* receptionReport = static_cast<const ReceptionReport*>(rtcpReceiverReportPacket->getReceptionReports()[i]);
                if(receptionReport != nullptr){
                    stream.writeUint32Be(receptionReport->getSsrc());
                    stream.writeByte(receptionReport->getFractionLost());
                    stream.writeNBitsOfUint64Be(receptionReport->getPacketsLostCumulative(), 24);
                    stream.writeUint32Be(receptionReport->getSequenceNumber());
                    stream.writeUint32Be(receptionReport->getJitter());
                    stream.writeUint32Be(receptionReport->getLastSR());
                    stream.writeUint32Be(receptionReport->getDelaySinceLastSR());
                }
            }
            std::cout << "length of stream of rtcpReceiverReportPacket: " << stream.getLength() - start_position << endl;
            std::cout << "chunkLength: " << rtcpReceiverReportPacket->getChunkLength() << endl;
            std::cout << "my count: " << B(4) + B(4) + B(size * 24) << endl;
            ASSERT(rtcpReceiverReportPacket->getChunkLength() == B(4) + B(4) + B(size * 24));
            break;
        }
        case RTCP_PT_SDES: {
            const auto& rtcpSdesPacket = staticPtrCast<const RtcpSdesPacket>(chunk);
            int num_chunks = rtcpSdesPacket->getCount();
            for(int i = 0; i < num_chunks; ++i){
                const SdesChunk* sdesChunk = static_cast<const SdesChunk*>(rtcpSdesPacket->getSdesChunks()[i]);
                stream.writeUint32Be(sdesChunk->getSsrc());
                for(int e = 0; e < sdesChunk->size(); ++e){
                    const SdesItem* sdesItem = static_cast<const SdesItem*>(sdesChunk->get(e));
                    stream.writeByte(sdesItem->getType());
                    // FIXME: as far as I see, the length field in the implementation contains the 2 bytes of the
                    // length field itself and the type field as well. In the real protocol however, the length
                    // field only represents the length of the text.
                    // in Sdes.cc:
                    //      int SdesItem::getLength() const
                    //          in this function 2 is added to the returned length
                    //      SdesItem::SdesItem(SdesItemType type, const char *content) : cObject()
                    //          in the constructor 2 is added to the initial length
                    // this way the getLength() might return 4 more than needed
                    stream.writeByte(sdesItem->getLength());
                    stream.writeStringOfMaxNBytes(sdesItem->getContent(), 255);
                }
                // FIXME: don't know whether the item with the type field of 0 is also included in the chunk
                // therefore I add it here to the bitstream -> needs to reviewed later.
                stream.writeByte(0);

                // padded to 32 bit boundary
                // +1 is the item with type field of 0 indicating the end of the chunk -> might not needed if it is
                // included in sdesChunk by default.
                stream.writeByteRepeatedly(0, (sdesChunk->getLength() + 1) % 4);
            }
            std::cout << "length of stream of rtcpSdesPacket: " << stream.getLength() - start_position << endl;
            std::cout << "chunkLength: " << rtcpSdesPacket->getChunkLength() << endl;
            ASSERT(rtcpSdesPacket->getChunkLength() == (stream.getLength() - start_position));
            break;
        }
        case RTCP_PT_BYE: {
            const auto& rtcpByePacket = staticPtrCast<const RtcpByePacket>(chunk);
            stream.writeUint32Be(rtcpByePacket->getSsrc());
            // FIXME: RtcpByePacket in this implementation has no field for holding a string value
            // the count field is however set to 1, but I think this count field should only
            // hold the length of the text, which is 0 in this case.
            stream.writeByte(0);
            // padding
            stream.writeByteRepeatedly(0, 3);
            std::cout << "length of stream of rtcpByePacket: " << stream.getLength() - start_position << endl;
            std::cout << "chunkLength: " << rtcpByePacket->getChunkLength() << endl;
            ASSERT(rtcpByePacket->getChunkLength() == (stream.getLength() - start_position));
            break;
        }
        default: {
            throw cRuntimeError("Can not serialize RTCP packet: type %d not supported.", rtcpPacket->getPacketType());
            break;
        }
    }

}

const Ptr<Chunk> RtcpPacketSerializer::deserialize(MemoryInputStream& stream) const
{
    auto rtcpPacket = makeShared<RtcpPacket>();
    rtcpPacket->setVersion(stream.readNBitsToUint64Be(2));
    rtcpPacket->setPadding(stream.readBit());
    rtcpPacket->setCount(stream.readNBitsToUint64Be(5));
    rtcpPacket->setPacketType((RtcpPacketType)stream.readByte());
    rtcpPacket->setRtcpLength(stream.readUint16Be());
    switch(rtcpPacket->getPacketType()){
        case RTCP_PT_SR: {
            auto rtcpSenderReportPacket = makeShared<RtcpSenderReportPacket>();
            rtcpSenderReportPacket->setVersion(rtcpPacket->getVersion());
            rtcpSenderReportPacket->setPadding(rtcpPacket->getPadding());
            rtcpSenderReportPacket->setCount(rtcpPacket->getCount());
            rtcpSenderReportPacket->setPacketType(rtcpPacket->getPacketType());
            rtcpSenderReportPacket->setRtcpLength(rtcpPacket->getRtcpLength());
            rtcpSenderReportPacket->setSsrc(stream.readUint32Be());
            rtcpSenderReportPacket->getSenderReportForUpdate().setNTPTimeStamp(stream.readUint64Be());
            rtcpSenderReportPacket->getSenderReportForUpdate().setRTPTimeStamp(stream.readUint32Be());
            rtcpSenderReportPacket->getSenderReportForUpdate().setPacketCount(stream.readUint32Be());
            rtcpSenderReportPacket->getSenderReportForUpdate().setByteCount(stream.readUint32Be());
            int size = rtcpSenderReportPacket->getCount();
            for(int i = 0; i < size; ++i){
                ReceptionReport receptionReport;
                receptionReport.setSsrc(stream.readUint32Be());
                receptionReport.setFractionLost(stream.readByte());
                receptionReport.setPacketsLostCumulative(stream.readNBitsToUint64Be(24));
                receptionReport.setSequenceNumber(stream.readUint32Be());
                receptionReport.setJitter(stream.readUint32Be());
                receptionReport.setLastSR(stream.readUint32Be());
                receptionReport.setDelaySinceLastSR(stream.readUint32Be());
                rtcpSenderReportPacket->addReceptionReport(&receptionReport);
            }
            rtcpPacket = rtcpSenderReportPacket;
            break;
        }
        case RTCP_PT_RR: {
            auto rtcpReceiverReportPacket = makeShared<RtcpReceiverReportPacket>();
            rtcpReceiverReportPacket->setVersion(rtcpPacket->getVersion());
            rtcpReceiverReportPacket->setPadding(rtcpPacket->getPadding());
            rtcpReceiverReportPacket->setCount(rtcpPacket->getCount());
            rtcpReceiverReportPacket->setPacketType(rtcpPacket->getPacketType());
            rtcpReceiverReportPacket->setRtcpLength(rtcpPacket->getRtcpLength());
            rtcpReceiverReportPacket->setSsrc(stream.readUint32Be());
            int size = rtcpReceiverReportPacket->getCount();
            for(int i = 0; i < size; ++i){
                ReceptionReport receptionReport;
                receptionReport.setSsrc(stream.readUint32Be());
                receptionReport.setFractionLost(stream.readByte());
                receptionReport.setPacketsLostCumulative(stream.readNBitsToUint64Be(24));
                receptionReport.setSequenceNumber(stream.readUint32Be());
                receptionReport.setJitter(stream.readUint32Be());
                receptionReport.setLastSR(stream.readUint32Be());
                receptionReport.setDelaySinceLastSR(stream.readUint32Be());
                rtcpReceiverReportPacket->addReceptionReport(&receptionReport);
            }
            rtcpPacket = rtcpReceiverReportPacket;
            break;
        }
        case RTCP_PT_SDES: {
            auto rtcpSdesPacket = makeShared<RtcpSdesPacket>();
            rtcpSdesPacket->setVersion(rtcpPacket->getVersion());
            rtcpSdesPacket->setPadding(rtcpPacket->getPadding());
            rtcpSdesPacket->setCount(rtcpPacket->getCount());
            rtcpSdesPacket->setPacketType(rtcpPacket->getPacketType());
            rtcpSdesPacket->setRtcpLength(rtcpPacket->getRtcpLength());
            int num_chunks = rtcpSdesPacket->getCount();
            for(int i = 0; i < num_chunks; ++i){
                SdesChunk sdesChunk;
                sdesChunk.setSsrc(stream.readUint32Be());
                uint8_t itemType = stream.readByte();
                while(itemType != 0){
                    // reading out the length field, it is set by the constructor
                    uint8_t length = stream.readByte();
                    SdesItem sdesItem = SdesItem((SdesItem::SdesItemType)itemType, stream.readStringOfNBytes(length));
                    sdesChunk.addSDESItem(&sdesItem);
                    itemType = stream.readByte();
                }
                stream.readByteRepeatedly(0, (sdesChunk.getLength() + 1) % 4);
                rtcpSdesPacket->addSDESChunk(&sdesChunk);
            }
            rtcpPacket = rtcpSdesPacket;
            break;
        }
        case RTCP_PT_BYE: {
            auto rtcpByePacket = makeShared<RtcpByePacket>();
            rtcpByePacket->setVersion(rtcpPacket->getVersion());
            rtcpByePacket->setPadding(rtcpPacket->getPadding());
            rtcpByePacket->setCount(rtcpPacket->getCount());
            rtcpByePacket->setPacketType(rtcpPacket->getPacketType());
            rtcpByePacket->setRtcpLength(rtcpPacket->getRtcpLength());
            rtcpByePacket->setSsrc(stream.readUint32Be());
            stream.readUint32Be();
            rtcpPacket = rtcpByePacket;
            break;
        }
        default: {
            throw cRuntimeError("Can not deserialize RTCP packet: type %d not supported.", rtcpPacket->getPacketType());
            break;
        }
    }
    return rtcpPacket;
}

} // namespace rtp
} // namespace inet

