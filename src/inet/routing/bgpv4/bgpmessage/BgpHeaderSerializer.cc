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
#include "inet/routing/bgpv4/bgpmessage/BgpUpdate.h"
#include "inet/routing/bgpv4/bgpmessage/BgpHeaderSerializer.h"

namespace inet::bgp {

Register_Serializer(BgpHeader, BgpHeaderSerializer);

Register_Serializer(BgpKeepAliveMessage, BgpHeaderSerializer);
Register_Serializer(BgpOpenMessage, BgpHeaderSerializer);
Register_Serializer(BgpUpdateMessage, BgpHeaderSerializer);

namespace {

bool serializeFlagsAndType(MemoryOutputStream& stream, const BgpUpdateAttributeType& flagsAndType) {
    auto& flags = flagsAndType.flags;
    stream.writeBit(flags.optionalBit);
    stream.writeBit(flags.transitiveBit);
    stream.writeBit(flags.partialBit);
    stream.writeBit(flags.estendedLengthBit);
    stream.writeBitRepeatedly(false, 4);
    stream.writeByte(flagsAndType.typeCode);
    return flags.estendedLengthBit;
}

/*bool deserializeFlags(MemoryInputStream& stream, const Ptr<BgpHeader> bgpHeader, BgpUpdateAttributeType& typeAndFlags, BgpUpdateAttributeTypeCode& type) {
    auto& flags = typeAndFlags.flags;
    bool optionalBit = stream.readBit();
    bool transitiveBit = stream.readBit();
    bool partialBit = stream.readBit();
    bool extendedLengthBit = stream.readBit();
    // if (optional bit == 0) -> transitive bit != 1, partial bit != 0
    if (!optionalBit && (!transitiveBit || partialBit))
        bgpHeader->markIncorrect();
    if (!stream.readBitRepeatedly(false, 4))
        bgpHeader->markIncorrect();
    flags.optionalBit = optionalBit;
    flags.transitiveBit = transitiveBit;
    flags.partialBit = partialBit;
    flags.estendedLengthBit = extendedLengthBit;
    type = static_cast<BgpUpdateAttributeTypeCode>(stream.readByte());
    typeAndFlags.typeCode = type;
    return extendedLengthBit;
}*/

bool deserializeFlagsAndType(MemoryInputStream& stream, bool& optionalBit, bool& transitiveBit, bool& partialBit, bool& extendedLengthBit, BgpUpdateAttributeTypeCode& type) {
    bool incorrect = false;
    optionalBit = stream.readBit();
    transitiveBit = stream.readBit();
    partialBit = stream.readBit();
    extendedLengthBit = stream.readBit();
    if (optionalBit)
        incorrect = true;   // only well-known types are supported
    if (optionalBit && (!transitiveBit || partialBit))
        incorrect = true;
    if (!stream.readBitRepeatedly(false, 4))
        incorrect = true;
    type = static_cast<BgpUpdateAttributeTypeCode>(stream.readByte());
    return incorrect;
}

void deserializePathAttribute(MemoryInputStream& stream, const Ptr<BgpHeader> bgpHeader, BgpUpdatePathAttributes& pathAttribute) {
    auto& flagsAndType = pathAttribute.getTypeForUpdate();
    auto& flags = flagsAndType.flags;
    flags.optionalBit = stream.readBit();
    flags.transitiveBit = stream.readBit();
    flags.partialBit = stream.readBit();
    flags.estendedLengthBit = stream.readBit();
    flagsAndType.typeCode = static_cast<BgpUpdateAttributeTypeCode>(stream.readByte());
    if (flags.optionalBit)
        bgpHeader->markIncorrect();   // only well-known types are supported
    if (!flags.optionalBit && (!flags.transitiveBit || flags.partialBit)) // if (optional bit == 0) -> transitive bit != 1, partial bit != 0
        bgpHeader->markIncorrect();
    if (!stream.readBitRepeatedly(false, 4))
        bgpHeader->markIncorrect();
    if (flags.estendedLengthBit)
        pathAttribute.setLength(stream.readUint16Be());
    else
        pathAttribute.setLength(stream.readByte());
}

}

void BgpHeaderSerializer::serialize(MemoryOutputStream& stream, const Ptr<const Chunk>& chunk) const
{
    throw cRuntimeError("BgpHeaderSerializer not fully implemented yet.");

    const auto& bgpHeader = staticPtrCast<const BgpHeader>(chunk);
    // each message has a fixed-size header:
    // -> Marker (16 bytes = 128 bits): MUST be set to all ones
    // -> Length (2 bytes = 16 bits): total length of the message, including the header in bytes (19 <= length <= 4096)
    // -> Type (1 byte = 8 bits): type code o the message
    stream.writeBitRepeatedly(true, 128);

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
            stream.writeIpv4Address(bgpOpenMessage->getBGPIdentifier());
            stream.writeByte(optionalParameterLength);

            for (size_t i = 0; i < bgpOpenMessage->getOptionalParametersArraySize(); ++i) {
                auto& optionalParameters = bgpOpenMessage->getOptionalParameters(i);
                stream.writeByte(optionalParameters.parameterType);
                stream.writeByte(optionalParameters.parameterLength);
                stream.writeByte(optionalParameters.parameterValues.authCode);
                stream.writeUint64Be(optionalParameters.parameterValues.authenticationData);
                for (int k = 0; k < (optionalParameters.parameterLength - 1 - 8); ++k)
                    stream.writeByte(0);
            }
            break;
        }
        case BGP_UPDATE: {
            const auto& bgpUpdateMessage = staticPtrCast<const BgpUpdateMessage>(chunk);
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
                if (!pathAttributeList.getNextHop().getValue().isUnspecified())
                    length += (2 + 1 + 4);   // nextHop: attribute type (2), attribute length (1), attribute value (IPv4 address) (4)
                if (pathAttributeList.getLocalPrefArraySize() > 0)
                    pathAttributesLength += (2 + 1 + 4);    // local_pref: attribute type (2), attribute length (1), four-octet unsigned integer (count * 4)
                if (pathAttributeList.getAtomicAggregateArraySize() > 0)
                    pathAttributesLength += (2 + 1);  // FIXME: ATOMIC_AGGREGATE is a well-known discretionary attribute of length 0.
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
             *
             *  if (optional bit == 0) -> transitive bit != 1, partial bit != 0
             *
             *  The same attribute (attribute with the same type) cannot appear more than once
             *  within the Path Attributes field of a particular UPDATE message.
             *
             *  The sender of an UPDATE message SHOULD order path attributes within the UPDATE message in ascending order of attribute type.
             *  The receiver of an UPDATE message MUST be prepared to handle path attributes within UPDATE messages that are out of order.
             */
            if (bgpUpdateMessage->getPathAttributeListArraySize() > 0) {
                auto& pathAttributeList = bgpUpdateMessage->getPathAttributeList(0);

                // ORIGIN
                auto& origin = pathAttributeList.getOrigin();
                if (serializeFlagsAndType(stream, origin.getType()))
                    stream.writeUint16Be(origin.getLength());
                else
                    stream.writeByte(origin.getLength());
                for (uint16_t i = 0; i < origin.getLength() - 1; ++i)
                    stream.writeByte(0);
                stream.writeByte(origin.getValue()); // attribute value

                // AS_PATH
                /**
                 * sequence of AS path segments, each of them represented by a triplet:
                 *      -> path segment type (1 byte)
                 *      -> path segment length (1 byte): number of ASes in the path segment value field
                 *      -> path segment value (path segment length * 2 bytes)
                 */
                if (pathAttributeList.getAsPathArraySize() > 0) {
                    auto& asPath = pathAttributeList.getAsPath(0);
                    if (serializeFlagsAndType(stream, asPath.getType()))
                        stream.writeUint16Be(asPath.getLength());
                    else
                        stream.writeByte(asPath.getLength());

                    uint16_t remaining = asPath.getLength();
                    // attribute value:
                    for (size_t k = 0; k < asPath.getValueArraySize(); ++k) {
                        auto& value = asPath.getValue(k);
                        stream.writeByte(value.getType());  // path segment type
                        stream.writeByte(value.getLength());    // path segment length
                        remaining -= (2 + value.getLength() * 2);
                        for (size_t e = 0; e < value.getLength(); ++e)
                            stream.writeUint16Be(value.getAsValue(e));  // path segment value
                    }
                    if (remaining != 0)
                        throw cRuntimeError("Attribute Length field and actual length do not match.");

                }

                // NEXT_HOP
                /**
                 * (unicast) IP address
                 */
                if (!pathAttributeList.getNextHop().getValue().isUnspecified()) {
                    auto& nextHop = pathAttributeList.getNextHop();
                    if (serializeFlagsAndType(stream, nextHop.getType()))
                        stream.writeUint16Be(nextHop.getLength());
                    else
                        stream.writeByte(nextHop.getLength());

                    stream.writeIpv4Address(nextHop.getValue()); // attribute value
                }

                // LOCAL_PREF
                /**
                 * it is a four-octet unsigned integer
                 */
                if (pathAttributeList.getLocalPrefArraySize() > 0) {
                    auto& localPref = pathAttributeList.getLocalPref(0);
                    if (serializeFlagsAndType(stream, localPref.getType()))
                        stream.writeUint16Be(localPref.getLength());
                    else
                        stream.writeByte(localPref.getLength());
                    stream.writeUint32Be(localPref.getValue()); // attribute value
                }

                // ATOMIC_AGGREGATE
                /**
                 * discretionary attribute of length 0
                 */
                if (pathAttributeList.getAtomicAggregateArraySize() > 0) {
                    auto& atomicAggregate = pathAttributeList.getAtomicAggregate(0);
                    if (serializeFlagsAndType(stream, atomicAggregate.getType()))
                        stream.writeUint16Be(atomicAggregate.getLength());
                    else
                        stream.writeByte(atomicAggregate.getLength());
                    if (atomicAggregate.getLength() != 0)
                        throw cRuntimeError("Attribute Length field of Atomic Aggregate should be 0.");
                    // stream.writeByte(pathAttributeList.getAtomicAggregate(0).getValue()); // attribute value  FIXME length should be 0 and this field should not be present here
                }
            }

            // Network Layer Reachability Information
            /**
             * list of IP address prefixes:
             *      -> length (1 byte)
             *      -> prefix (length bits long padded to byte boundary)
             */
            auto& nlri = bgpUpdateMessage->getNLRI();
            stream.writeByte(nlri.length);
            stream.writeIpv4Address(nlri.prefix);
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
    auto bgpHeader = makeShared<BgpHeader>();
    bool incorrect = false;
    if (!stream.readBitRepeatedly(true, 128))
        incorrect = true;

    uint16_t length = stream.readUint16Be();
    BgpType type = static_cast<BgpType>(stream.readByte());

    switch (type) {
        case BGP_OPEN: {
            auto bgpOpenMessage = makeShared<BgpOpenMessage>();
            if (incorrect)
                bgpOpenMessage->markIncorrect();
            bgpOpenMessage->setChunkLength(B(length));
            bgpOpenMessage->setType(type);
            bgpOpenMessage->setVersion(stream.readByte());
            bgpOpenMessage->setMyAS(stream.readUint16Be());
            bgpOpenMessage->setHoldTime(SimTime(stream.readUint32Be(), SIMTIME_S));
            bgpOpenMessage->setBGPIdentifier(stream.readIpv4Address());
            uint8_t optionalParameterLength = stream.readByte();

            uint8_t count = 0;
            while (count < optionalParameterLength) {
                bgpOpenMessage->setOptionalParametersArraySize(bgpOpenMessage->getOptionalParametersArraySize() + 1);
                BgpOptionalParameters optionalParameters = BgpOptionalParameters();
                optionalParameters.parameterType = stream.readByte();
                optionalParameters.parameterLength = stream.readByte();
                optionalParameters.parameterValues.authCode = stream.readByte();
                optionalParameters.parameterValues.authenticationData = stream.readUint64Be();
                count += 11;
                for (uint8_t k = 0; k < (optionalParameters.parameterLength - 1 - 8); ++k) {
                    stream.readByte();
                    ++count;
                }
                bgpOpenMessage->setOptionalParameters(bgpOpenMessage->getOptionalParametersArraySize() - 1, optionalParameters);
            }
            return bgpOpenMessage;
        }
        case BGP_UPDATE: {
            auto bgpUpdateMessage = makeShared<BgpUpdateMessage>();
            if (incorrect)
                bgpUpdateMessage->markIncorrect();
            bgpUpdateMessage->setChunkLength(B(length));
            bgpUpdateMessage->setType(type);
            uint32_t withdrawnRoutesLength = stream.readUint32Be();
            uint32_t count = 0;
            while (count < withdrawnRoutesLength) {
                bgpUpdateMessage->setWithdrawnRoutesArraySize(bgpUpdateMessage->getWithdrawnRoutesArraySize() + 1);
                BgpUpdateWithdrawnRoutes withdrawnRoutes = BgpUpdateWithdrawnRoutes();
                withdrawnRoutes.length = stream.readByte();
                withdrawnRoutes.prefix = stream.readIpv4Address();
                count += 5;
            }
            uint32_t pathAttributesLength = stream.readUint32Be();
            count = 0;
            if (pathAttributesLength > 0) {
                BgpUpdatePathAttributeList pathAttributeList = BgpUpdatePathAttributeList();
                while (count < pathAttributesLength) {
                    BgpUpdatePathAttributes pathAttribute = BgpUpdatePathAttributes();
                    deserializePathAttribute(stream, bgpUpdateMessage, pathAttribute);

                    switch (pathAttribute.getType().typeCode) {
                        case ORIGIN: {
                            BgpUpdatePathAttributesOrigin origin = static_cast<BgpUpdatePathAttributesOrigin&>(pathAttribute);
                            origin.setValue(static_cast<BgpSessionType>(stream.readByte()));
                            count += (1 + (origin.getType().flags.estendedLengthBit == 0 ? 1 : 2) + origin.getLength());
                            pathAttributeList.setOrigin(origin);
                            break;
                        }
                        case AS_PATH: {
                            BgpUpdatePathAttributesAsPath asPath = static_cast<BgpUpdatePathAttributesAsPath&>(pathAttribute);
                            uint16_t asPathCount = 0;
                            while (asPathCount < asPath.getLength()) {
                                asPath.setValueArraySize(asPath.getValueArraySize() + 1);
                                BgpAsPathSegment value = BgpAsPathSegment();
                                value.setType(static_cast<BgpPathSegmentType>(stream.readByte()));
                                value.setLength(stream.readByte());
                                asPathCount += (2 + value.getLength() * 2);
                                for (size_t e = 0; e < value.getLength(); ++e)
                                    value.setAsValue(e, stream.readUint16Be());
                            }
                            count += (1 + (asPath.getType().flags.estendedLengthBit == 0 ? 1 : 2) + asPath.getLength());
                            pathAttributeList.setAsPathArraySize(1);
                            pathAttributeList.setAsPath(0, asPath);
                            break;
                        }
                        case NEXT_HOP: {
                            BgpUpdatePathAttributesNextHop nextHop = static_cast<BgpUpdatePathAttributesNextHop&>(pathAttribute);
                            nextHop.setValue(stream.readIpv4Address());
                            count += (1 + (nextHop.getType().flags.estendedLengthBit == 0 ? 1 : 2) + nextHop.getLength());
                            pathAttributeList.setNextHop(nextHop);
                        }
                        case LOCAL_PREF: {
                            BgpUpdatePathAttributesLocalPref localPref = static_cast<BgpUpdatePathAttributesLocalPref&>(pathAttribute);
                            localPref.setValue(stream.readUint32Be());
                            count += (1 + (localPref.getType().flags.estendedLengthBit == 0 ? 1 : 2) + localPref.getLength());
                            pathAttributeList.setLocalPrefArraySize(1);
                            pathAttributeList.setLocalPref(0, localPref);
                        }
                        case ATOMIC_AGGREGATE: {
                            BgpUpdatePathAttributesAtomicAggregate atomicAggregate = static_cast<BgpUpdatePathAttributesAtomicAggregate&>(pathAttribute);
                            count += (1 + (atomicAggregate.getType().flags.estendedLengthBit == 0 ? 1 : 2) + atomicAggregate.getLength());
                            pathAttributeList.setAtomicAggregateArraySize(1);
                            pathAttributeList.setAtomicAggregate(0, atomicAggregate);
                        }
                        default:
                            bgpHeader->markIncorrect();
                    }
                }
                bgpUpdateMessage->setPathAttributeList(pathAttributeList);
            }
            break;
        }
        case BGP_KEEPALIVE: {
            auto bgpKeepAliveMessage = makeShared<BgpKeepAliveMessage>();
            if (incorrect)
                bgpKeepAliveMessage->markIncorrect();
            bgpKeepAliveMessage->setChunkLength(B(length));
            bgpKeepAliveMessage->setType(type);
            break;
        }
        default: {
            bgpHeader->setChunkLength(B(1));
            bgpHeader->markIncorrect();
            return bgpHeader;
        }
    }
}

} // namespace inet::bgp




















