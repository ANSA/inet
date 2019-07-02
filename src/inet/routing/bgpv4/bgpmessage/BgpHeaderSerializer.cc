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

            // finalizing the header
            int optionalParameterLength = 0;
            for (size_t i = 0; i < bgpOpenMessage->getOptionalParametersArraySize(); ++i)
                optionalParameterLength += (1 + 1 + bgpOpenMessage->getOptionalParameters(i).parameterLength);

            // length = header + version + myAs + holdTime + bgpId + optParamLen + optParameters
            stream.writeUint16Be(BGP_HEADER_OCTETS.get() + 1 + 2 + 2 + 4 + 1 + optionalParameterLength);    // length
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

            for(size_t i = 0; i < bgpOpenMessage->getOptionalParametersArraySize(); ++i){
                auto& optionalParameters = bgpOpenMessage->getOptionalParameters(i);
                stream.writeByte(optionalParameters.parameterType);
                stream.writeByte(optionalParameters.parameterLength);
                stream.writeByte(optionalParameters.parameterValues.authCode);
                stream.writeUint64Be(optionalParameters.parameterValues.authenticationData);
                for (int i = 0; i < (optionalParameters.parameterLength - 1 - 8); ++i)
                    stream.writeByte(0);
            }
            break;
        }
        case BGP_UPDATE: {
            const auto& bgpUpdateMessage = staticPtrCast<const BgpUpdateMessage_Base>(chunk);
            // finalizing the header
            int length = 0;
            int withdrawnRoutesLength = 0;
            int pathAttributesLength = 0;
            length += 2;    // withdrawn routes length
            // withdrawn routes: | length (1 byte) | prefix (length bits padded to 8 bit boundary) |
            for (size_t i = 0; i < bgpUpdateMessage->getWithdrawnRoutesArraySize(); ++i) {
                auto& withdrawnRoutes = bgpUpdateMessage->getWithdrawnRoutes(i);
                withdrawnRoutesLength += (1 + b(withdrawnRoutes.length).get() / 8);
                withdrawnRoutesLength += ((b(withdrawnRoutes.length).get() % 8) > 0 ? 1 : 0);
            }
            length += 2;    // total path attributes length
            if (bgpUpdateMessage->getPathAttributeListArraySize() > 0) {
                auto& pathAttributeList = bgpUpdateMessage->getPathAttributeList(0);
                pathAttributesLength += (2 + 1 + 1);  // origin: attribute type (2), attribute length (1), attribute value (1)
                if (pathAttributeList.getAsPathArraySize() > 0)
                    pathAttributesLength += (2 + 1);  // asPath: attribute type (2), attribute length (1), attribute value (...)
                for (size_t k = 0; k < pathAttributeList.getAsPathArraySize(); ++k)
                    pathAttributesLength += (1 + 1 + pathAttributeList.getAsPath(k).getValueArraySize() * 2);   // segment type (1), path segment length (1), path segment value (2 * number of path segment values)
                length += (2 + 1 + 4);   // nextHop: attribute type (2), attribute length (1), attribute value (IPv4 address) (4)
                if (pathAttributeList.getLocalPrefArraySize() > 0)
                    pathAttributesLength += (2 + 1 + pathAttributeList.getLocalPrefArraySize() * 4);    // local_pref: attribute type (2), attribute length (1), four-octet unsigned integer (count * 4)
                if (pathAttributeList.getAtomicAggregateArraySize() > 0)
                    pathAttributesLength += (2 + 1 + pathAttributeList.getAtomicAggregateArraySize());  // FIXME: ATOMIC_AGGREGATE is a well-known discretionary attribute of length 0.
            }
            length += (1 + 4);  // nlri: length (1), prefix (IPv4 address) (4)

            stream.writeUint16Be(BGP_HEADER_OCTETS.get() + length + withdrawnRoutesLength + pathAttributesLength);    // length
            stream.writeByte(type);

            stream.writeUint32Be(withdrawnRoutesLength);
            // Withdraw Routes
            /**
             * list of IP address prefixes for the routes that are being withdrawn from service
             * each IP address prefix is encoded as a 2-tuple:
             *      -> length (1 byte)
             *      -> prefix (length bits long padded to byte boundary)
             */
            for (size_t i = 0; i < bgpUpdateMessage->getWithdrawnRoutesArraySize(); ++i) {
                auto& withdrawnRoutes = bgpUpdateMessage->getWithdrawnRoutes(i);
                stream.writeByte(withdrawnRoutes.length);
                stream.writeIpv4Address(withdrawnRoutes.prefix);
            }

            stream.writeUint32Be(pathAttributesLength);
            // Path Attributes
            /**
             * sequence of path attributes, each path attribute is a triple:
             *      -> attribute type (2 bytes):
             *              -> attribute flags (1 byte):
             *                      -> bit 0: defines whether the attribute is optional (1) or well-known (0)
             *                      -> bit 1: defines whether the attribute is transitive (1) or non-transitive (0)
             *                      -> bit 2: defines whether the attribute is partial (1) or complete (0)
             *                      -> bit 3: extended length bit
             *                      -> bit 4..7: unused, set to 0
             *              -> attribute type code (1 byte)
             *       -> attribute length: contains the length of the attribute value in bytes
             *              -> length field is 1 byte if the extended length bit is set to 0
             *              -> length field is 2 bytes if the extended length bit is set to 1
             *       -> attribute value (attribute length bytes)
             *  if (optiona bit == 1) -> transitive bit != 1, partial bit != 0
             */
            if (bgpUpdateMessage->getPathAttributeListArraySize() > 0) {
                auto& pathAttributeList = bgpUpdateMessage->getPathAttributeList(0);

                // ORIGIN
                stream.writeBit(false); // well-known
                stream.writeBit(true);  // transitive
                stream.writeBit(false); // complete
                stream.writeBit(false); // length field is 1 byte long
                stream.writeBitRepeatedly(false, 4);    // unused

                stream.writeByte(ORIGIN);   // attribute type
                stream.writeByte(1);    // attribute length
                stream.writeByte(pathAttributeList.getOrigin().getValue()); // attribute value

                // AS_PATH
                /**
                 * sequence of AS path segments, each of them represented by a triplet:
                 *      -> path segment type (1 byte)
                 *      -> path segment length (1 byte): number of ASes in the path segment value field
                 *      -> path segment value (path segment length * 2 bytes)
                 */
                for (size_t i = 0; i < pathAttributeList.getAsPathArraySize(); ++i) {
                    stream.writeBit(false); // well-known
                    stream.writeBit(true);  // transitive
                    stream.writeBit(false); // complete
                    stream.writeBit(false); // length field is 1 byte long
                    stream.writeBitRepeatedly(false, 4);    // unused

                    stream.writeByte(AS_PATH);  // attribute type

                    auto& asPath = pathAttributeList.getAsPath(i);
                    int valueListLength = 0;
                    for (size_t k = 0; k < asPath.getValueArraySize(); ++k)
                        valueListLength += (1 + 1 + asPath.getValue(k).getLength() * 2);

                    stream.writeByte(valueListLength);   // attribute length
                    // attribute value:
                    for (size_t k = 0; k < asPath.getValueArraySize(); ++k) {
                        auto& value = asPath.getValue(k);
                        stream.writeByte(value.getType());
                        stream.writeByte(value.getLength());
                        for (size_t e = 0; e < value.getLength(); ++e)
                            stream.writeUint16Be(value.getAsValue(e));
                    }

                }

                // NEXT_HOP
                stream.writeBit(false); // well-known
                stream.writeBit(true);  // transitive
                stream.writeBit(false); // complete
                stream.writeBit(false); // length field is 1 byte long
                stream.writeBitRepeatedly(false, 4);    // unused

                stream.writeByte(NEXT_HOP);   // attribute type
                stream.writeByte(4);    // attribute length
                stream.writeIpv4Address(pathAttributeList.getNextHop().getValue()); // attribute value

                // LOCAL_PREF
                /**
                 * it is a four-octet unsigned integer
                 */
                stream.writeBit(false); // well-known
                stream.writeBit(true);  // transitive
                stream.writeBit(false); // complete
                stream.writeBit(false); // length field is 1 byte long
                stream.writeBitRepeatedly(false, 4);    // unused

                stream.writeByte(NEXT_HOP);   // attribute type
                stream.writeByte(4);    // attribute length
                stream.writeUint32Be(pathAttributeList.getLocalPref(0).getValue()); // attribute value

                // ATOMIC_AGGREGATE
                /**
                 * discretionary attribute of length 0
                 */
                stream.writeBit(false); // well-known
                stream.writeBit(true);  // transitive
                stream.writeBit(false); // complete
                stream.writeBit(false); // length field is 1 byte long
                stream.writeBitRepeatedly(false, 4);    // unused

                stream.writeByte(ATOMIC_AGGREGATE);   // attribute type
                stream.writeByte(1);    // attribute length FIXME should be 0
                stream.writeByte(pathAttributeList.getAtomicAggregate(0).getValue()); // attribute value
            }

            // Network Layer Reachability Information
            /**
             * list of IP address prefixes:
             *      -> length (1 byte)
             *      -> prefix (length bits long padded to byte boundary)
             */
            stream.writeByte(bgpUpdateMessage->getNLRI().length);
            stream.writeIpv4Address(bgpUpdateMessage->getNLRI().prefix);
            break;
        }
        case BGP_KEEPALIVE: {
            // finalizing the header
            stream.writeUint16Be(BGP_HEADER_OCTETS.get());  // length
            stream.writeByte(type); // type
            break;
        }
        default:
            throw cRuntimeError("Cannot serialize BGP packet: type %d not supported.", type);
    }
}

const Ptr<Chunk> BgpHeaderSerializer::deserialize(MemoryInputStream& stream) const
{
}

} // namespace inet::bgp




















