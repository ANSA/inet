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
#include "inet/routing/ospfv2/OspfPacketSerializer.h"
#include "inet/routing/ospfv2/router/OspfCommon.h"

namespace inet {

namespace ospf {

// register serializer for all five OSPF messages
Register_Serializer(OspfPacket, OspfPacketSerializer);
Register_Serializer(OspfHelloPacket, OspfPacketSerializer);
Register_Serializer(OspfDatabaseDescriptionPacket, OspfPacketSerializer);
Register_Serializer(OspfLinkStateRequestPacket, OspfPacketSerializer);
Register_Serializer(OspfLinkStateUpdatePacket, OspfPacketSerializer);
Register_Serializer(OspfLinkStateAcknowledgementPacket, OspfPacketSerializer);

void OspfPacketSerializer::serialize(MemoryOutputStream& stream, const Ptr<const Chunk>& chunk) const
{
    const auto& ospfPacket = staticPtrCast<const OspfPacket>(chunk);
    serializeOspfHeader(stream, ospfPacket);
    switch (ospfPacket->getType()) {
        case HELLO_PACKET: {
            const auto& helloPacket = staticPtrCast<const OspfHelloPacket>(ospfPacket);
            stream.writeIpv4Address(helloPacket->getNetworkMask());
            stream.writeUint16Be(helloPacket->getHelloInterval());
            stream.writeByte(ospfOptionToByte(helloPacket->getOptions()));
            stream.writeByte(helloPacket->getRouterPriority());
            stream.writeUint32Be(helloPacket->getRouterDeadInterval());
            stream.writeIpv4Address(helloPacket->getDesignatedRouter());
            stream.writeIpv4Address(helloPacket->getBackupDesignatedRouter());
            // iterate over each neighbor and write to stream
            for (size_t i = 0; i < helloPacket->getNeighborArraySize(); ++i) {
                stream.writeIpv4Address(helloPacket->getNeighbor(i));
            }
            break;
        }
        case DATABASE_DESCRIPTION_PACKET: {
            const auto& ddPacket = staticPtrCast<const OspfDatabaseDescriptionPacket>(ospfPacket);
            stream.writeUint16Be(ddPacket->getInterfaceMTU());
            stream.writeByte(ospfOptionToByte(ddPacket->getOptions()));
            auto& options = ddPacket->getDdOptions();
            stream.writeNBitsOfUint64Be(options.unused, 5);
            stream.writeBit(options.I_Init);
            stream.writeBit(options.M_More);
            stream.writeBit(options.MS_MasterSlave);
            stream.writeUint32Be(ddPacket->getDdSequenceNumber());
            // iterate over each LSA header and write to stream
            for (unsigned int i = 0; i < ddPacket->getLsaHeadersArraySize(); ++i) {
                serializeLsaHeader(stream, ddPacket->getLsaHeaders(i));
            }
            break;
        }
        case LINKSTATE_REQUEST_PACKET: {
            const auto& requestPacket = staticPtrCast<const OspfLinkStateRequestPacket>(ospfPacket);
            // iterate over each LS request and write to stream
            for (unsigned int i = 0; i < requestPacket->getRequestsArraySize(); ++i) {
                auto& req = requestPacket->getRequests(i);
                stream.writeUint32Be(req.lsType);
                stream.writeIpv4Address(req.linkStateID);
                stream.writeIpv4Address(req.advertisingRouter);
            }
            break;
        }
        case LINKSTATE_UPDATE_PACKET: {
            //const auto& updatePacket = staticPtrCast<const OspfLinkStateUpdatePacket>(ospfPacket);
            auto updatePacket = makeShared<OspfLinkStateUpdatePacket>();

            stream.writeUint32Be(updatePacket->getOspfLSAsArraySize());
            for (size_t i = 0; i < updatePacket->getOspfLSAsArraySize(); ++i) {
                const auto& ospfLSAs = updatePacket->getOspfLSAs(i);
                auto& lsaHeader = ospfLSAs->getHeader();
                serializeLsaHeader(stream, lsaHeader);
                LsaType type = lsaHeader.getLsType();
                switch (type) {
                    case ROUTERLSA_TYPE: {
                        const OspfRouterLsa& routerLsa = static_cast<const OspfRouterLsa&>(updatePacket->getOspfLSAs(i));
                        serializeRouterLsa(stream, routerLsa);
                        break;
                    }
                    case NETWORKLSA_TYPE: {
                        const OspfNetworkLsa* networkLsa = static_cast<const OspfNetworkLsa*>(ospfLSAs);
                        serializeNetworkLsa(stream, networkLsa);
                        break;
                    }
                    case SUMMARYLSA_NETWORKS_TYPE: {
                        const OspfSummaryLsa* summaryLsa = static_cast<const OspfSummaryLsa*>(ospfLSAs);
                        serializeSummaryLsa(stream, summaryLsa);
                        break;
                    }
                    case AS_EXTERNAL_LSA_TYPE: {
                        const OspfAsExternalLsa* asExternalLsa = static_cast<const OspfAsExternalLsa*>(ospfLSAs);
                        serializeAsExternalLsa(stream, asExternalLsa);
                        break;
                    }
                    default:
                        throw cRuntimeError("Cannot serialize BGP packet: type %d not supported.", type);
                }
            }
            break;
        }
        case LINKSTATE_ACKNOWLEDGEMENT_PACKET: {
            const auto& ackPacket = staticPtrCast<const OspfLinkStateAcknowledgementPacket>(ospfPacket);
            // iterate over each LSA header and write to stream
            for (unsigned int i = 0; i < ackPacket->getLsaHeadersArraySize(); ++i) {
                serializeLsaHeader(stream, ackPacket->getLsaHeaders(i));
            }
            break;
        }
        default:
            throw cRuntimeError("Unknown OSPF message type in OspfPacketSerializer");
    }
}

const Ptr<Chunk> OspfPacketSerializer::deserialize(MemoryInputStream& stream) const
{
    auto ospfPacket = makeShared<OspfPacket>();
    uint16_t packetLength = deserializeOspfHeader(stream, ospfPacket);

    OspfPacketType type = ospfPacket->getType();
    switch (type) {
        case HELLO_PACKET: {
            auto helloPacket = makeShared<OspfHelloPacket>();
            helloPacket->setChunkLength(ospfPacket->getChunkLength());
            helloPacket->setRouterID(ospfPacket->getRouterID());
            helloPacket->setAreaID(ospfPacket->getAreaID());
            helloPacket->setCrc(ospfPacket->getCrc());
            helloPacket->setCrcMode(ospfPacket->getCrcMode());
            helloPacket->setAuthenticationType(ospfPacket->getAuthenticationType());
            for (int i = 0; i < 8; ++i) {
                helloPacket->setAuthentication(i, ospfPacket->getAuthentication(i));
            }

            helloPacket->setNetworkMask(stream.readIpv4Address());
            helloPacket->setHelloInterval(stream.readUint16Be());
            helloPacket->setOptions(byteToOspfOption(stream.readUint8()));
            helloPacket->setRouterPriority(stream.readUint8());
            helloPacket->setRouterDeadInterval(stream.readUint32Be());
            helloPacket->setDesignatedRouter(stream.readIpv4Address());
            helloPacket->setBackupDesignatedRouter(stream.readIpv4Address());
            int numNeighbors = (B(packetLength) - OSPF_HEADER_LENGTH - OSPF_HELLO_HEADER_LENGTH).get() / 4;
            if (numNeighbors < 0)
                helloPacket->markIncorrect();
            helloPacket->setNeighborArraySize(numNeighbors);
            for (int i = 0; i< numNeighbors; i++) {
                helloPacket->setNeighbor(i, stream.readIpv4Address());
            }
            return helloPacket;
        }
        case DATABASE_DESCRIPTION_PACKET: {
            auto ddPacket = makeShared<OspfDatabaseDescriptionPacket>();
            ddPacket->setChunkLength(ospfPacket->getChunkLength());
            ddPacket->setRouterID(ospfPacket->getRouterID());
            ddPacket->setAreaID(ospfPacket->getAreaID());
            ddPacket->setCrc(ospfPacket->getCrc());
            ddPacket->setCrcMode(ospfPacket->getCrcMode());
            ddPacket->setAuthenticationType(ospfPacket->getAuthenticationType());
            for (int i = 0; i < 8; ++i) {
                ddPacket->setAuthentication(i, ospfPacket->getAuthentication(i));
            }

            ddPacket->setInterfaceMTU(stream.readUint16Be());
            ddPacket->setOptions(byteToOspfOption(stream.readUint8()));
            ddPacket->setDdOptions(byteToDdFlags(stream.readUint8()));
            ddPacket->setDdSequenceNumber(stream.readUint32Be());
            int numLsaHeaders = ((B(packetLength) - OSPF_HEADER_LENGTH - OSPF_DD_HEADER_LENGTH) / OSPF_LSA_HEADER_LENGTH).get();
            if (numLsaHeaders < 0)
                ddPacket->markIncorrect();
            ddPacket->setLsaHeadersArraySize(numLsaHeaders);
            for (int i = 0; i< numLsaHeaders; i++) {
                auto lsaHeader = new OspfLsaHeader();
                if (!deserializeLsaHeader(stream, lsaHeader))
                    ddPacket->markIncorrect();
                ddPacket->setLsaHeaders(i, *lsaHeader);
            }
            return ddPacket;
        }
        case LINKSTATE_REQUEST_PACKET: {
            auto requestPacket = makeShared<OspfLinkStateRequestPacket>();
            requestPacket->setChunkLength(ospfPacket->getChunkLength());
            requestPacket->setRouterID(ospfPacket->getRouterID());
            requestPacket->setAreaID(ospfPacket->getAreaID());
            requestPacket->setCrc(ospfPacket->getCrc());
            requestPacket->setCrcMode(ospfPacket->getCrcMode());
            requestPacket->setAuthenticationType(ospfPacket->getAuthenticationType());
            for (int i = 0; i < 8; ++i) {
                requestPacket->setAuthentication(i, ospfPacket->getAuthentication(i));
            }

            int numReq = (B(packetLength) - OSPF_HEADER_LENGTH).get() / OSPF_REQUEST_LENGTH.get();
            if (numReq < 0)
                requestPacket->markIncorrect();
            requestPacket->setRequestsArraySize(numReq);
            for (int i = 0; i < numReq; i++) {
                auto req = new LsaRequest();
                req->lsType = stream.readUint32Be();
                req->linkStateID = stream.readIpv4Address();
                req->advertisingRouter = stream.readIpv4Address();
                requestPacket->setRequests(i, *req);
            }
            return requestPacket;
        }
        case LINKSTATE_UPDATE_PACKET: {
            auto updatePacket = makeShared<OspfLinkStateUpdatePacket>();
            updatePacket->setChunkLength(ospfPacket->getChunkLength());
            updatePacket->setRouterID(ospfPacket->getRouterID());
            updatePacket->setAreaID(ospfPacket->getAreaID());
            updatePacket->setCrc(ospfPacket->getCrc());
            updatePacket->setCrcMode(ospfPacket->getCrcMode());
            updatePacket->setAuthenticationType(ospfPacket->getAuthenticationType());
            for (int i = 0; i < 8; ++i) {
                updatePacket->setAuthentication(i, ospfPacket->getAuthentication(i));
            }

            uint32_t numLSAs = stream.readUint32Be();
            updatePacket->setNumberOfLSAs(numLSAs);
            std::cout << "numLSAs: " << numLSAs << endl;
            int routerLsaCounter = 0;
            int networkLsaCounter = 0;
            int summaryLsaCounter = 0;
            int asExternalCounter = 0;
            for (uint32_t i = 0; i < numLSAs; i++) {
                auto lsaHeader = new OspfLsaHeader();
                if (!deserializeLsaHeader(stream, lsaHeader)){
                    std::cout << "1" << endl;
                    updatePacket->markIncorrect();}
                LsaType lsaType = lsaHeader->getLsType();
                switch (lsaType) {
                    case ROUTERLSA_TYPE: {
                        auto routerLsa = new OspfRouterLsa();
                        auto& header = routerLsa->getHeaderForUpdate();
                        header.setLsAge(lsaHeader->getLsAge());
                        header.setLsOptions(lsaHeader->getLsOptions());
                        header.setLsType(lsaHeader->getLsType());
                        header.setLinkStateID(lsaHeader->getLinkStateID());
                        header.setAdvertisingRouter(lsaHeader->getAdvertisingRouter());
                        header.setLsSequenceNumber(lsaHeader->getLsSequenceNumber());
                        header.setLsCrc(lsaHeader->getLsCrc());
                        header.setLsaLength(lsaHeader->getLsaLength());
                        if (!deserializeRouterLsa(stream, routerLsa)){
                            std::cout << "2" << endl;
                            updatePacket->markIncorrect();}
                        updatePacket->setRouterLSAsArraySize(routerLsaCounter + 1);
                        updatePacket->setRouterLSAs(routerLsaCounter, *routerLsa);
                        routerLsaCounter++;
                        break;
                    }
                    case NETWORKLSA_TYPE: {
                        auto networkLsa = new OspfNetworkLsa();
                        auto& header = networkLsa->getHeaderForUpdate();
                        header.setLsAge(lsaHeader->getLsAge());
                        header.setLsOptions(lsaHeader->getLsOptions());
                        header.setLsType(lsaHeader->getLsType());
                        header.setLinkStateID(lsaHeader->getLinkStateID());
                        header.setAdvertisingRouter(lsaHeader->getAdvertisingRouter());
                        header.setLsSequenceNumber(lsaHeader->getLsSequenceNumber());
                        header.setLsCrc(lsaHeader->getLsCrc());
                        header.setLsaLength(lsaHeader->getLsaLength());
                        if (!deserializeNetworkLsa(stream, networkLsa)){
                            std::cout << "3" << endl;
                            updatePacket->markIncorrect();}
                        updatePacket->setNetworkLSAsArraySize(networkLsaCounter + 1);
                        updatePacket->setNetworkLSAs(networkLsaCounter, *networkLsa);
                        networkLsaCounter++;
                        break;
                    }
                    case SUMMARYLSA_NETWORKS_TYPE: {
                        auto summaryLsa = new OspfSummaryLsa();
                        auto& header = summaryLsa->getHeaderForUpdate();
                        header.setLsAge(lsaHeader->getLsAge());
                        header.setLsOptions(lsaHeader->getLsOptions());
                        header.setLsType(lsaHeader->getLsType());
                        header.setLinkStateID(lsaHeader->getLinkStateID());
                        header.setAdvertisingRouter(lsaHeader->getAdvertisingRouter());
                        header.setLsSequenceNumber(lsaHeader->getLsSequenceNumber());
                        header.setLsCrc(lsaHeader->getLsCrc());
                        header.setLsaLength(lsaHeader->getLsaLength());
                        if (!deserializeSummaryLsa(stream, summaryLsa)){
                            std::cout << "4" << endl;
                            updatePacket->markIncorrect();}
                        updatePacket->setSummaryLSAsArraySize(summaryLsaCounter + 1);
                        updatePacket->setSummaryLSAs(summaryLsaCounter, *summaryLsa);
                        summaryLsaCounter++;
                        break;
                    }
                    case AS_EXTERNAL_LSA_TYPE: {
                        auto asExternalLsa = new OspfAsExternalLsa();
                        auto& header = asExternalLsa->getHeaderForUpdate();
                        header.setLsAge(lsaHeader->getLsAge());
                        header.setLsOptions(lsaHeader->getLsOptions());
                        header.setLsType(lsaHeader->getLsType());
                        header.setLinkStateID(lsaHeader->getLinkStateID());
                        header.setAdvertisingRouter(lsaHeader->getAdvertisingRouter());
                        header.setLsSequenceNumber(lsaHeader->getLsSequenceNumber());
                        header.setLsCrc(lsaHeader->getLsCrc());
                        header.setLsaLength(lsaHeader->getLsaLength());
                        if (!deserializeAsExternalLsa(stream, asExternalLsa)) {
                            std::cout << "5" << endl;
                            updatePacket->markIncorrect();}
                        updatePacket->setAsExternalLSAsArraySize(asExternalCounter + 1);
                        updatePacket->setAsExternalLSAs(asExternalCounter, *asExternalLsa);
                        asExternalCounter++;
                        break;
                    }
                    default:
                        throw cRuntimeError("Cannot deserialize OSPF Packet: lsa type %d not supported!", lsaType);
                }
            }
            return updatePacket;
        }
        case LINKSTATE_ACKNOWLEDGEMENT_PACKET: {
            auto ackPacket = makeShared<OspfLinkStateAcknowledgementPacket>();
            ackPacket->setChunkLength(ospfPacket->getChunkLength());
            ackPacket->setRouterID(ospfPacket->getRouterID());
            ackPacket->setAreaID(ospfPacket->getAreaID());
            ackPacket->setCrc(ospfPacket->getCrc());
            ackPacket->setCrcMode(ospfPacket->getCrcMode());
            ackPacket->setAuthenticationType(ospfPacket->getAuthenticationType());
            for (int i = 0; i < 8; ++i) {
                ackPacket->setAuthentication(i, ospfPacket->getAuthentication(i));
            }

            int numHeaders = (B(packetLength) - OSPF_HEADER_LENGTH).get() / OSPF_LSA_HEADER_LENGTH.get();
            if (numHeaders < 0)
                ackPacket->markIncorrect();
            ackPacket->setLsaHeadersArraySize(numHeaders);
            for (int i = 0; i < numHeaders; i++) {
                auto lsaHeader = new OspfLsaHeader();
                if (!deserializeLsaHeader(stream, lsaHeader))
                    ackPacket->markIncorrect();
                ackPacket->setLsaHeaders(i, *lsaHeader);
            }
            return ackPacket;
        }
        default:
            throw cRuntimeError("Unknown OSPF message type in OspfPacketSerializer");
    }
}

void OspfPacketSerializer::serializeOspfHeader(MemoryOutputStream& stream, const IntrusivePtr<const OspfPacket>& ospfPacket)
{
    stream.writeByte(ospfPacket->getVersion());
    stream.writeByte(ospfPacket->getType());
    stream.writeUint16Be(B(ospfPacket->getChunkLength()).get());
    stream.writeIpv4Address(ospfPacket->getRouterID());
    stream.writeIpv4Address(ospfPacket->getAreaID());
    auto crcMode = ospfPacket->getCrcMode();
    if (crcMode != CRC_DISABLED && crcMode != CRC_COMPUTED)
        throw cRuntimeError("Cannot serialize Ospf header without turned off or properly computed CRC, try changing the value of crcMode parameter for Ospf");
    stream.writeUint16Be(ospfPacket->getCrc());
    stream.writeUint16Be(ospfPacket->getAuthenticationType());
    // iterate over each authentication data and write to stream
    for (unsigned int i = 0; i < 8; ++i) {
        stream.writeByte(ospfPacket->getAuthentication(i));
    }
}

uint16_t OspfPacketSerializer::deserializeOspfHeader(MemoryInputStream& stream, IntrusivePtr<OspfPacket>& ospfPacket)
{
    int ospfVer = stream.readUint8();
    if (ospfVer != 2)
        ospfPacket->markIncorrect();
    ospfPacket->setVersion(ospfVer);

    int ospfType = stream.readUint8();
    if (ospfType > LINKSTATE_ACKNOWLEDGEMENT_PACKET)
        ospfPacket->markIncorrect();
    ospfPacket->setType(static_cast<OspfPacketType>(ospfType));

    uint16_t packetLength = stream.readUint16Be();
    ospfPacket->setChunkLength(b(packetLength * 8));
    ospfPacket->setRouterID(stream.readIpv4Address());
    ospfPacket->setAreaID(stream.readIpv4Address());
    ospfPacket->setCrc(stream.readUint16Be());
    ospfPacket->setCrcMode(CRC_COMPUTED);
    ospfPacket->setAuthenticationType(stream.readUint16Be());
    for (int i = 0; i < 8; ++i) {
        ospfPacket->setAuthentication(i, stream.readUint8());
    }

    return packetLength;
}

void OspfPacketSerializer::serializeLsaHeader(MemoryOutputStream& stream, const OspfLsaHeader& lsaHeader)
{
    stream.writeUint16Be(lsaHeader.getLsAge());
    stream.writeByte(ospfOptionToByte(lsaHeader.getLsOptions()));
    stream.writeByte(lsaHeader.getLsType());
    stream.writeIpv4Address(lsaHeader.getLinkStateID());
    stream.writeIpv4Address(lsaHeader.getAdvertisingRouter());
    stream.writeUint32Be(lsaHeader.getLsSequenceNumber());
    stream.writeUint16Be(lsaHeader.getLsCrc());
    stream.writeUint16Be(lsaHeader.getLsaLength());
}

bool OspfPacketSerializer::deserializeLsaHeader(MemoryInputStream& stream, OspfLsaHeader *lsaHeader)
{
    lsaHeader->setLsAge(stream.readUint16Be());
    lsaHeader->setLsOptions(byteToOspfOption(stream.readUint8()));
    lsaHeader->setLsType(static_cast<LsaType>(stream.readUint8()));
    lsaHeader->setLinkStateID(stream.readIpv4Address());
    lsaHeader->setAdvertisingRouter(stream.readIpv4Address());
    lsaHeader->setLsSequenceNumber(stream.readUint32Be());
    lsaHeader->setLsCrc(stream.readUint16Be());
    lsaHeader->setLsaLength(stream.readUint16Be());

    return true;
}

void OspfPacketSerializer::serializeRouterLsa(MemoryOutputStream& stream, const OspfRouterLsa& routerLsa)
{
    uint16_t flags = 0;
    if (routerLsa.getB_AreaBorderRouter())
        flags |= 1 << 8;
    if (routerLsa.getE_ASBoundaryRouter())
        flags |= 1 << 9;
    if (routerLsa.getV_VirtualLinkEndpoint())
        flags |= 1 << 10;
    stream.writeUint16Be(flags);

    uint16_t numLinks = routerLsa.getNumberOfLinks();
    stream.writeUint16Be(numLinks);
    // iterate over each link
    for (int32_t i = 0; i < numLinks; i++) {
        const Link& link = routerLsa.getLinks(i);
        stream.writeIpv4Address(link.getLinkID());
        stream.writeUint32Be(link.getLinkData());
        stream.writeUint8(link.getType());
        stream.writeUint8(link.getNumberOfTOS());
        stream.writeUint16Be(link.getLinkCost());
        uint32_t numTos = link.getTosDataArraySize();
        for (uint32_t j = 0; j < numTos; j++) {
            const TosData& tos = link.getTosData(j);
            stream.writeUint8(tos.tos);
            stream.writeUint8(tos.tosMetric[0]);
            stream.writeUint8(tos.tosMetric[1]);
            stream.writeUint8(tos.tosMetric[2]);
        }
    }
}

bool OspfPacketSerializer::deserializeRouterLsa(MemoryInputStream& stream, OspfRouterLsa *routerLsa)
{
    uint16_t flags = stream.readUint16Be();
    routerLsa->setB_AreaBorderRouter((flags & (1 << 8)) != 0);
    routerLsa->setE_ASBoundaryRouter((flags & (1 << 9)) != 0);
    routerLsa->setV_VirtualLinkEndpoint((flags & (1 << 10)) != 0);

    uint16_t numLinks = stream.readUint16Be();
    routerLsa->setNumberOfLinks(numLinks);
    routerLsa->setLinksArraySize(numLinks);
    B linksSize;
    for (uint32_t i = 0; i < numLinks; i++) {
        auto link = new Link();
        link->setLinkID(stream.readIpv4Address());
        link->setLinkData(stream.readUint32Be());
        link->setType(static_cast<LinkType>(stream.readUint8()));
        uint8_t numTos = stream.readUint8();
        link->setLinkCost(stream.readUint16Be());
        for (uint32_t j = 0; j < numTos; j++) {
            auto tos = new TosData();
            tos->tos = stream.readUint8();
            tos->tosMetric[0] = stream.readUint8();
            tos->tosMetric[1] = stream.readUint8();
            tos->tosMetric[2] = stream.readUint8();
            link->setTosData(j, *tos);
        }
        routerLsa->setLinks(i, *link);
        linksSize += OSPF_LINK_HEADER_LENGTH + B(numTos * OSPF_TOS_LENGTH.get());
    }

    B reportedLinksSize = B(routerLsa->getHeader().getLsaLength()) -
            OSPF_LSA_HEADER_LENGTH - OSPF_ROUTERLSA_HEADER_LENGTH;
    if (reportedLinksSize != linksSize)
        return false;

    return true;
}

void OspfPacketSerializer::serializeNetworkLsa(MemoryOutputStream& stream, const OspfNetworkLsa& networkLsa)
{
    stream.writeIpv4Address(networkLsa.getNetworkMask());
    for (uint32_t i = 0; i < networkLsa.getAttachedRoutersArraySize(); i++) {
        stream.writeIpv4Address(networkLsa.getAttachedRouters(i));
    }
}

bool OspfPacketSerializer::deserializeNetworkLsa(MemoryInputStream& stream, OspfNetworkLsa *networkLsa)
{
    networkLsa->setNetworkMask(stream.readIpv4Address());
    int numAttachedRouters = (B(networkLsa->getHeader().getLsaLength()) -
            OSPF_LSA_HEADER_LENGTH - OSPF_NETWORKLSA_MASK_LENGTH).get() / OSPF_NETWORKLSA_ADDRESS_LENGTH.get();
    if (numAttachedRouters < 0)
        return false;

    for (int i = 0; i < numAttachedRouters; i++) {
        networkLsa->setAttachedRouters(i, stream.readIpv4Address());
    }

    return true;
}

void OspfPacketSerializer::serializeSummaryLsa(MemoryOutputStream& stream, const OspfSummaryLsa& summaryLsa)
{
    stream.writeIpv4Address(summaryLsa.getNetworkMask());
    stream.writeUint32Be(summaryLsa.getRouteCost());
    for (uint32_t i = 0; i < summaryLsa.getTosDataArraySize(); i++) {
        const TosData& tos = summaryLsa.getTosData(i);
        stream.writeUint8(tos.tos);
        stream.writeUint8(tos.tosMetric[0]);
        stream.writeUint8(tos.tosMetric[1]);
        stream.writeUint8(tos.tosMetric[2]);
    }
}

bool OspfPacketSerializer::deserializeSummaryLsa(MemoryInputStream& stream, OspfSummaryLsa *summaryLsa)
{
    summaryLsa->setNetworkMask(stream.readIpv4Address());
    summaryLsa->setRouteCost(stream.readUint32Be());

    int numTos = (B(summaryLsa->getHeader().getLsaLength()) -
            OSPF_LSA_HEADER_LENGTH - OSPF_NETWORKLSA_MASK_LENGTH - B(4)).get() / OSPF_TOS_LENGTH.get();
    if (numTos < 0)
        return false;

    for (int i = 0; i < numTos; i++) {
        auto tos = new TosData();
        tos->tos = stream.readUint8();
        tos->tosMetric[0] = stream.readUint8();
        tos->tosMetric[1] = stream.readUint8();
        tos->tosMetric[2] = stream.readUint8();
        summaryLsa->setTosData(i, *tos);
    }

    return true;
}

void OspfPacketSerializer::serializeAsExternalLsa(MemoryOutputStream& stream, const OspfAsExternalLsa& asExternalLsa)
{
    stream.writeIpv4Address(asExternalLsa.getContents().getNetworkMask());

    uint32_t routeCost = asExternalLsa.getContents().getRouteCost();
    if (asExternalLsa.getContents().getE_ExternalMetricType())
        routeCost |= 1 << 31;
    stream.writeUint32Be(routeCost);

    stream.writeIpv4Address(asExternalLsa.getContents().getForwardingAddress());
    stream.writeUint32Be(asExternalLsa.getContents().getExternalRouteTag());

    for (uint32_t i = 0; i < asExternalLsa.getContents().getExternalTOSInfoArraySize(); i++) {
        const ExternalTosInfo& exTos = asExternalLsa.getContents().getExternalTOSInfo(i);
        uint8_t tos = exTos.tosData.tos;
        if (exTos.E_ExternalMetricType)
            tos |= 1 << 7;
        stream.writeUint8(tos);
        stream.writeUint8(exTos.tosData.tosMetric[0]);
        stream.writeUint8(exTos.tosData.tosMetric[1]);
        stream.writeUint8(exTos.tosData.tosMetric[2]);
        stream.writeIpv4Address(asExternalLsa.getContents().getForwardingAddress());
        stream.writeUint32Be(asExternalLsa.getContents().getExternalRouteTag());
    }
}

bool OspfPacketSerializer::deserializeAsExternalLsa(MemoryInputStream& stream, OspfAsExternalLsa *asExternalLsa)
{
    asExternalLsa->getContentsForUpdate().setNetworkMask(stream.readIpv4Address());
    uint32_t routeCost = stream.readUint32Be();
    asExternalLsa->getContentsForUpdate().setRouteCost(routeCost);
    asExternalLsa->getContentsForUpdate().setE_ExternalMetricType(((routeCost & (1 << 31)) != 0));
    asExternalLsa->getContentsForUpdate().setForwardingAddress(stream.readIpv4Address());
    asExternalLsa->getContentsForUpdate().setExternalRouteTag(stream.readUint32Be());

    int numExternalTos = (B(asExternalLsa->getHeader().getLsaLength()) -
            OSPF_LSA_HEADER_LENGTH - OSPF_ASEXTERNALLSA_HEADER_LENGTH).get() / OSPF_ASEXTERNALLSA_TOS_INFO_LENGTH.get();
    if (numExternalTos < 0)
        return false;
    for (int i = 0; i < numExternalTos; i++) {
        ExternalTosInfo *extTos = new ExternalTosInfo();

        TosData tos;
        tos.tos = stream.readUint8();
        tos.tosMetric[0] = stream.readUint8();
        tos.tosMetric[1] = stream.readUint8();
        tos.tosMetric[2] = stream.readUint8();
        extTos->tosData = tos;

        extTos->E_ExternalMetricType = ((tos.tos & (1 << 7)) != 0);
        extTos->forwardingAddress = stream.readIpv4Address();
        extTos->externalRouteTag = stream.readUint32Be();

        asExternalLsa->getContentsForUpdate().setExternalTOSInfo(i, *extTos);
    }

    return true;
}

void OspfPacketSerializer::serializeLsa(MemoryOutputStream& stream, const OspfLsa& lsa)
{
    //TODO implementation
    serializeLsaHeader(stream, lsa.getHeader());
    throw cRuntimeError("not implemented yet");
}

uint8_t OspfPacketSerializer::ospfOptionToByte(const OspfOptions& options)
{
    uint8_t c = 0;

    if (options.unused_1)
        c |= 1 << 0;
    if (options.E_ExternalRoutingCapability)
        c |= 1 << 1;
    if (options.MC_MulticastForwarding)
        c |= 1 << 2;
    if (options.NP_Type7LSA)
        c |= 1 << 3;
    if (options.EA_ForwardExternalLSAs)
        c |= 1 << 4;
    if (options.DC_DemandCircuits)
        c |= 1 << 5;
    if (options.unused_2)
        c |= 1 << 6;
    if (options.unused_3)
        c |= 1 << 7;

    return c;
}

const OspfOptions OspfPacketSerializer::byteToOspfOption(uint8_t c)
{
    OspfOptions option = {};

    option.unused_1 = (c & (1 << 0)) != 0;
    option.E_ExternalRoutingCapability = (c & (1 << 1)) != 0;
    option.MC_MulticastForwarding = (c & (1 << 2)) != 0;
    option.NP_Type7LSA = (c & (1 << 3)) != 0;
    option.EA_ForwardExternalLSAs = (c & (1 << 4)) != 0;
    option.DC_DemandCircuits = (c & (1 << 5)) != 0;
    option.unused_2 = (c & (1 << 6)) != 0;
    option.unused_3 = (c & (1 << 7)) != 0;

    return option;
}

uint8_t OspfPacketSerializer::ddFlagsToByte(const OspfDdOptions& options)
{
    uint8_t c = 0;

    if (options.unused_1)
        c |= 1 << 7;
    if (options.unused_2)
        c |= 1 << 6;
    if (options.unused_3)
        c |= 1 << 5;
    if (options.unused_4)
        c |= 1 << 4;
    if (options.unused_5)
        c |= 1 << 3;
    if (options.I_Init)
        c |= 1 << 2;
    if (options.M_More)
        c |= 1 << 1;
    if (options.MS_MasterSlave)
        c |= 1 << 0;

    return c;
}

const OspfDdOptions OspfPacketSerializer::byteToDdFlags(uint8_t c)
{
    OspfDdOptions option = {};

    option.unused_1 = (c & (1 << 7)) != 0;
    option.unused_2 = (c & (1 << 6)) != 0;
    option.unused_3 = (c & (1 << 5)) != 0;
    option.unused_4 = (c & (1 << 4)) != 0;
    option.unused_5 = (c & (1 << 3)) != 0;
    option.I_Init = (c & (1 << 2)) != 0;
    option.M_More = (c & (1 << 1)) != 0;
    option.MS_MasterSlave = (c & (1 << 0)) != 0;

    return option;
}

} // namespace ospf

} // namespace inet

