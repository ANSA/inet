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
#include "inet/routing/bgpv4/bgpmessage/BgpHeader_m.h"
#include "inet/routing/bgpv4/bgpmessage/BgpHeaderSerializer.h"

namespace inet::bgp {

Register_Serializer(BgpHeader, BgpHeaderSerializer);

Register_Serializer(BgpKeepAliveMessage, BgpHeaderSerializer);
Register_Serializer(BgpOpenMessage, BgpHeaderSerializer);
//Register_Serializer(BgpUpdateMessage, BgpHeaderSerializer);

void BgpHeaderSerializer::serialize(MemoryOutputStream& stream, const Ptr<const Chunk>& chunk) const
{
    throw cRuntimeError("BgpHeaderSerializer not fully implemented yet.");

    const auto& bgpHeader = staticPtrCast<const BgpHeader>(chunk);
    // each message has a fixed-size header:
    // -> Marker (16 bytes = 128 bits): MUST be set to all ones
    // -> Length (2 bytes = 16 bits): total length of the message, including the header in bytes (19 <= length <= 4096)
    // -> Type (1 byte = 8 bits): type code o the message
    stream.writeUint64Be(1);
    stream.writeUint64Be(1);
    BgpType type = bgpHeader->getType();

    switch (type) {
        case BGP_OPEN: {
            const auto& bgpOpenMessage = CHK(dynamicPtrCast<const BgpOpenMessage>(chunk));

            int optionalParameterLength = bgpOpenMessage->getOptionalParametersArraySize() * sizeof(BgpOptionalParameters);

            // finalizing the header
            stream.writeUint16Be(B(BGP_HEADER_OCTETS).get() + B(BGP_HEADER_OCTETS).get() + optionalParameterLength);
            stream.writeByte(type);

            // -> Version: (1 byte = 8 bits): current BGP version is 4
            // -> My Autonomous System (2 byte = 16 bits): autonomous system number of the sender
            // -> Holt Time (2 byte = 16 bits)
            // -> BGP Identifier (4 bytes = 32 bits)
            // -> Optional ParametersLength (1 byte = 8 bits): total length of the Optional Parameters field in bytes
            // -> Optional Parameters
            //      -> parameter type (1 byte = 8 bits)
            //      -> parameter length (1 byte = 8 bits)
            //      -> parameter value (variable length)

            stream.writeByte(bgpOpenMessage->getVersion());
            stream.writeUint16Be(bgpOpenMessage->getMyAS());
            stream.writeUint16Be(bgpOpenMessage->getHoldTime().raw());
            stream.writeUint32Be(bgpOpenMessage->getBGPIdentifier().getInt());
            stream.writeByte(optionalParameterLength);

            if(optionalParameterLength != 0){
                for(int i = 0; i < bgpOpenMessage->getOptionalParametersArraySize(); ++i){
                    stream.writeByte(bgpOpenMessage->getOptionalParameters(i).parameterType);
                    stream.writeByte(bgpOpenMessage->getOptionalParameters(i).parameterLength);
                    stream.writeUint16Be(bgpOpenMessage->getOptionalParameters(i).parameterValues.authCode);
                    stream.writeUint64Be(bgpOpenMessage->getOptionalParameters(i).parameterValues.authenticationData);
                }
            }


            break;
        }
        case BGP_UPDATE: {
            //const auto& bgpUpdateMessage = CHK(dynamicPtrCast<const BgpUpdateMessage>(chunk));
            stream.writeUint16Be(B(BGP_HEADER_OCTETS).get() + B(BGP_HEADER_OCTETS).get());
            stream.writeByte(type);
            break;
        }
        case BGP_NOTIFICATION: {
            break;
        }
        case BGP_KEEPALIVE: {
            break;
        }
    }
}

const Ptr<Chunk> BgpHeaderSerializer::deserialize(MemoryInputStream& stream) const
{
}

} // namespace inet::bgp




















