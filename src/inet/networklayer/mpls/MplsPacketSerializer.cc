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
#include "inet/networklayer/mpls/MplsPacket_m.h"
#include "inet/networklayer/mpls/MplsPacketSerializer.h"

namespace inet {

Register_Serializer(MplsHeader, MplsPacketSerializer);

void MplsPacketSerializer::serialize(MemoryOutputStream& stream, const Ptr<const Chunk>& chunk) const
{
    const auto& mplsHeader = staticPtrCast<const MplsHeader>(chunk);
    size_t size = mplsHeader->getLabelsArraySize();
    for(int i = 0; i < size; ++i){
        int m = 524288; // 2^19
        for(int e = 0; e < 20; ++e){
            stream.writeBit((mplsHeader->getLabels(i).getLabel() & m) == m);
            m /= 2;
        }

        m = 4; // 2^3
        for(int e = 0; e < 3; ++e){
            stream.writeBit((mplsHeader->getLabels(i).getLabel() & m) == m);
            m /= 2;
        }

        stream.writeBit(i == size - 1);

        stream.writeByte(mplsHeader->getLabels(i).getTtl());
    }
}

const Ptr<Chunk> MplsPacketSerializer::deserialize(MemoryInputStream& stream) const
{
    auto mplsHeader = makeShared<MplsHeader>();
    bool mbool = false;
    int i = 0;
    while(!mbool){
        mplsHeader->setLabelsArraySize(mplsHeader->getLabelsArraySize() + 1);
        MplsLabel mplsLabel = mplsHeader->getLabels(i);

        long label = 0;
        int m = 524288; // 2^19
        for(int e = 0; e < 20; ++e){
            label += stream.readBit() & m;
            m /= 2;
        }
        mplsLabel.setLabel(label);

        short tc = 0;
        m = 4; // 2^3
        for(int e = 0; e < 3; ++e){
            tc += stream.readBit() & m;
            m /= 2;
        }
        mplsLabel.setTc(tc);

        mbool = stream.readBit();

        mplsLabel.setTtl(stream.readByte());

        mplsHeader->setLabels(i, mplsLabel);
        ++i;
    }
}

} // namespace inet


















