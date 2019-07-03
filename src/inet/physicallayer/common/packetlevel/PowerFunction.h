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

#ifndef __INET_POWERFUNCTION_H_
#define __INET_POWERFUNCTION_H_

#include "inet/common/geometry/common/Coord.h"
#include "inet/common/math/Functions.h"
#include "inet/physicallayer/contract/packetlevel/IRadioMedium.h"

namespace inet {

namespace physicallayer {

using namespace inet::math;

class INET_API AttenuationFunction : public FunctionBase<double, Domain<simtime_t, Hz>>
{
  protected:
    const IRadioMedium *radioMedium = nullptr;
    const double transmitterAntennaGain;
    const double receiverAntennaGain;
    const Coord transmissionPosition;
    const Coord receptionPosition;
    const Hz frequencyQuantization;
    m distance;

  protected:
    virtual double getAttenuation(Hz frequency) const {
        auto propagationSpeed = radioMedium->getPropagation()->getPropagationSpeed();
        auto pathLoss = radioMedium->getPathLoss()->computePathLoss(propagationSpeed, frequency, distance);
        auto obstacleLoss = radioMedium->getObstacleLoss() ? radioMedium->getObstacleLoss()->computeObstacleLoss(frequency, transmissionPosition, receptionPosition) : 1;
        return std::min(1.0, transmitterAntennaGain * receiverAntennaGain * pathLoss * obstacleLoss);
    }

  public:
    AttenuationFunction(const IRadioMedium *radioMedium, const double transmitterAntennaGain, const double receiverAntennaGain, const Coord transmissionPosition, const Coord receptionPosition, const Hz frequencyQuantization) :
        radioMedium(radioMedium), transmitterAntennaGain(transmitterAntennaGain), receiverAntennaGain(receiverAntennaGain), transmissionPosition(transmissionPosition), receptionPosition(receptionPosition), frequencyQuantization(frequencyQuantization)
    {
        distance = m(transmissionPosition.distance(receptionPosition));
    }

    virtual double getValue(const Point<simtime_t, Hz>& p) const override {
        Hz frequency = frequencyQuantization * round(unit(std::get<1>(p) / frequencyQuantization).get());
        return getAttenuation(frequency);
    }

    virtual void partition(const Interval<simtime_t, Hz>& i, const std::function<void (const Interval<simtime_t, Hz>&, const IFunction<double, Domain<simtime_t, Hz>> *)> f) const override {
        Hz minFrequency = frequencyQuantization * floor(unit(std::get<1>(i.getLower()) / frequencyQuantization).get());
        Hz maxFrequency = frequencyQuantization * ceil(unit(std::get<1>(i.getUpper()) / frequencyQuantization).get());
        for (Hz frequency = minFrequency; frequency < maxFrequency; frequency += frequencyQuantization) {
            ConstantFunction<double, Domain<simtime_t, Hz>> g(getAttenuation(frequency));
            Point<simtime_t, Hz> lower(std::get<0>(i.getLower()), std::max(std::get<1>(i.getLower()), frequency));
            Point<simtime_t, Hz> upper(std::get<0>(i.getUpper()), std::min(std::get<1>(i.getUpper()), frequency + frequencyQuantization));
            Interval<simtime_t, Hz> i1(lower, upper);
            if (i1.isValid())
                f(i1, &g);
        }
    }
};

/**
 * This mathematical function provides the transmission signal power for any given
 * space, time, and frequency coordinate vector.
 */
class INET_API ReceptionPowerFunction : public FunctionBase<WpHz, Domain<m, m, m, simtime_t, Hz>>
{
  protected:
    const Ptr<const IFunction<WpHz, math::Domain<simtime_t, Hz>>> transmissionPowerFunction;
    const Ptr<const IFunction<double, Domain<Quaternion>>> transmitterAntennaGainFunction;
    const Ptr<const IFunction<double, Domain<mps, m, Hz>>> pathLossFunction;
    const Ptr<const IFunction<double, Domain<m, m, m, m, m, m, Hz>>> obstacleLossFunction;
    const Point<m, m, m> startPosition;
    const Quaternion startOrientation;
    const mps propagationSpeed;
    const Hz frequencyQuantization;

  protected:
    double getAttenuation(const Point<m, m, m, simtime_t, Hz>& p) const {
        m x = std::get<0>(p);
        m y = std::get<1>(p);
        m z = std::get<2>(p);
        m startX = std::get<0>(startPosition);
        m startY = std::get<1>(startPosition);
        m startZ = std::get<2>(startPosition);
        m dx = x - startX;
        m dy = y - startY;
        m dz = z - startZ;
        Hz frequency = frequencyQuantization * round(unit(std::get<4>(p) / frequencyQuantization).get());
        m distance = m(sqrt(dx * dx + dy * dy + dz * dz));
        auto direction = Quaternion::rotationFromTo(Coord::X_AXIS, Coord(dx.get(), dy.get(), dz.get()));
        auto antennaLocalDirection = startOrientation.inverse() * direction;
        double transmitterAntennaGain = distance == m(0) ? 1 : transmitterAntennaGainFunction->getValue(Point<Quaternion>(antennaLocalDirection));
        double pathLoss = pathLossFunction->getValue(Point<mps, m, Hz>(propagationSpeed, distance, frequency));
        double obstacleLoss = obstacleLossFunction != nullptr ? obstacleLossFunction->getValue(Point<m, m, m, m, m, m, Hz>(startX, startY, startZ, x, y, z, frequency)) : 1;
        return std::min(1.0, transmitterAntennaGain * pathLoss * obstacleLoss);
    }

  public:
    ReceptionPowerFunction(const Ptr<const IFunction<WpHz, math::Domain<simtime_t, Hz>>>& transmissionPowerFunction, const Ptr<const IFunction<double, Domain<Quaternion>>>& transmitterAntennaGainFunction, const Ptr<const IFunction<double, Domain<mps, m, Hz>>>& pathLossFunction, const Ptr<const IFunction<double, Domain<m, m, m, m, m, m, Hz>>>& obstacleLossFunction, const Point<m, m, m> startPosition, const Quaternion startOrientation, const mps propagationSpeed, const Hz frequencyQuantization) :
        transmissionPowerFunction(transmissionPowerFunction), transmitterAntennaGainFunction(transmitterAntennaGainFunction), pathLossFunction(pathLossFunction), obstacleLossFunction(obstacleLossFunction), startPosition(startPosition), startOrientation(startOrientation), propagationSpeed(propagationSpeed), frequencyQuantization(frequencyQuantization) { }

    virtual const Point<m, m, m>& getStartPosition() const { return startPosition; }

    virtual WpHz getValue(const Point<m, m, m, simtime_t, Hz>& p) const override {
        m x = std::get<0>(p);
        m y = std::get<1>(p);
        m z = std::get<2>(p);
        m startX = std::get<0>(startPosition);
        m startY = std::get<1>(startPosition);
        m startZ = std::get<2>(startPosition);
        m dx = x - startX;
        m dy = y - startY;
        m dz = z - startZ;
        simtime_t time = std::get<3>(p);
        Hz frequency = std::get<4>(p);
        m distance = m(sqrt(dx * dx + dy * dy + dz * dz));
        if (std::isinf(distance.get()))
            return WpHz(0);
        simtime_t propagationTime = s(distance / propagationSpeed).get();
        WpHz transmissionPower = transmissionPowerFunction->getValue(Point<simtime_t, Hz>(time - propagationTime, frequency));
        return transmissionPower * getAttenuation(p);
    }

    virtual void partition(const Interval<m, m, m, simtime_t, Hz>& i, const std::function<void (const Interval<m, m, m, simtime_t, Hz>&, const IFunction<WpHz, Domain<m, m, m, simtime_t, Hz>> *)> f) const override {
        const auto& lower = i.getLower();
        const auto& upper = i.getUpper();
        if (std::get<0>(lower) == std::get<0>(upper) && std::get<1>(lower) == std::get<1>(upper) && std::get<2>(lower) == std::get<2>(upper)) {
            Hz minFrequency = frequencyQuantization * floor(unit(std::get<4>(i.getLower()) / frequencyQuantization).get());
            Hz maxFrequency = frequencyQuantization * ceil(unit(std::get<4>(i.getUpper()) / frequencyQuantization).get());
            m x = std::get<0>(lower);
            m y = std::get<1>(lower);
            m z = std::get<2>(lower);
            m startX = std::get<0>(startPosition);
            m startY = std::get<1>(startPosition);
            m startZ = std::get<2>(startPosition);
            m dx = x - startX;
            m dy = y - startY;
            m dz = z - startZ;
            m distance = m(sqrt(dx * dx + dy * dy + dz * dz));
            simtime_t propagationTime = s(distance / propagationSpeed).get();
            for (Hz frequency = minFrequency; frequency < maxFrequency; frequency += frequencyQuantization) {
                Point<simtime_t, Hz> l1(std::get<3>(lower) - propagationTime, std::max(std::get<4>(i.getLower()), frequency));
                Point<simtime_t, Hz> u1(std::get<3>(upper) - propagationTime, std::min(std::get<4>(i.getUpper()), frequency + frequencyQuantization));
                Interval<simtime_t, Hz> i1(l1, u1);
                double attenuation = getAttenuation(Point<m, m, m, simtime_t, Hz>(std::get<0>(lower), std::get<1>(lower), std::get<2>(lower), std::get<3>(lower), frequency));
                if (i1.isValid()) {
                    transmissionPowerFunction->partition(i1, [&] (const Interval<simtime_t, Hz>& i2, const IFunction<WpHz, Domain<simtime_t, Hz>> *g) {
                        Interval<m, m, m, simtime_t, Hz> i3(
                            Point<m, m, m, simtime_t, Hz>(std::get<0>(lower), std::get<1>(lower), std::get<2>(lower), std::get<0>(i2.getLower()) + propagationTime, std::get<1>(i2.getLower())),
                            Point<m, m, m, simtime_t, Hz>(std::get<0>(upper), std::get<1>(upper), std::get<2>(upper), std::get<0>(i2.getUpper()) + propagationTime, std::get<1>(i2.getUpper())));
                        if (auto cg = dynamic_cast<const ConstantFunction<WpHz, math::Domain<simtime_t, Hz>> *>(g)) {
                            ConstantFunction<WpHz, Domain<m, m, m, simtime_t, Hz>> h(cg->getConstantValue() * attenuation);
                            f(i3, &h);
                        }
                        else if (auto lg = dynamic_cast<const LinearInterpolatedFunction<WpHz, math::Domain<simtime_t, Hz>> *>(g)) {
                            LinearInterpolatedFunction<WpHz, Domain<m, m, m, simtime_t, Hz>> h(i3.getLower(), i3.getUpper(), lg->getValue(i2.getLower()) * attenuation, lg->getValue(i2.getUpper()) * attenuation, lg->getDimension() + 3);
                            f(i3, &h);
                        }
                        else
                            throw cRuntimeError("TODO");
                    });
                }
            }
        }
        else
            throw cRuntimeError("TODO");
    }
};

class INET_API PathLossFunction : public FunctionBase<double, Domain<mps, m, Hz>>
{
  protected:
    const IPathLoss *pathLoss;

  public:
    PathLossFunction(const IPathLoss *pathLoss) : pathLoss(pathLoss) { }

    virtual double getValue(const Point<mps, m, Hz>& p) const override {
        mps propagationSpeed = std::get<0>(p);
        Hz frequency = std::get<2>(p);
        m distance = std::get<1>(p);
        return pathLoss->computePathLoss(propagationSpeed, frequency, distance);
    }

    virtual void partition(const Interval<mps, m, Hz>& i, const std::function<void (const Interval<mps, m, Hz>&, const IFunction<double, Domain<mps, m, Hz>> *)> f) const override {
        throw cRuntimeError("TODO");
    }
};

class INET_API ObstacleLossFunction : public FunctionBase<double, Domain<m, m, m, m, m, m, Hz>>
{
  protected:
    const IObstacleLoss *obstacleLoss;

  public:
    ObstacleLossFunction(const IObstacleLoss *obstacleLoss) : obstacleLoss(obstacleLoss) { }

    virtual double getValue(const Point<m, m, m, m, m, m, Hz>& p) const override {
        Coord transmissionPosition(std::get<0>(p).get(), std::get<1>(p).get(), std::get<2>(p).get());
        Coord receptionPosition(std::get<3>(p).get(), std::get<4>(p).get(), std::get<5>(p).get());
        Hz frequency = std::get<6>(p);
        return obstacleLoss->computeObstacleLoss(frequency, transmissionPosition, receptionPosition);
    }

    virtual void partition(const Interval<m, m, m, m, m, m, Hz>& i, const std::function<void (const Interval<m, m, m, m, m, m, Hz>&, const IFunction<double, Domain<m, m, m, m, m, m, Hz>> *)> f) const override {
        throw cRuntimeError("TODO");
    }
};

class INET_API AntennaGainFunction : public IFunction<double, Domain<Quaternion>>
{
  protected:
    const IAntennaGain *antennaGain;

  public:
    AntennaGainFunction(const IAntennaGain *antennaGain) : antennaGain(antennaGain) { }

    virtual double getValue(const Point<Quaternion>& p) const override {
        return antennaGain->computeGain(std::get<0>(p));
    }

    virtual void partition(const Interval<Quaternion>& i, const std::function<void (const Interval<Quaternion>&, const IFunction<double, Domain<Quaternion>> *)> f) const override {
        throw cRuntimeError("TODO");
    }

    template<int DIMS, typename RI, typename DI>
    Ptr<const IFunction<RI, DI>> integrate() const { throw cRuntimeError("TODO"); }

    virtual Interval<double> getRange() const { throw cRuntimeError("TODO"); }
    virtual Interval<Quaternion> getDomain() const { throw cRuntimeError("TODO"); }
    virtual Ptr<const IFunction<double, Domain<Quaternion>>> limitDomain(const Interval<Quaternion>& i) const { throw cRuntimeError("TODO"); }

    virtual double getMin() const { throw cRuntimeError("TODO"); }
    virtual double getMin(const Interval<Quaternion>& i) const { throw cRuntimeError("TODO"); }

    virtual double getMax() const { throw cRuntimeError("TODO"); }
    virtual double getMax(const Interval<Quaternion>& i) const { throw cRuntimeError("TODO"); }

    virtual double getMean() const { throw cRuntimeError("TODO"); }
    virtual double getMean(const Interval<Quaternion>& i) const { throw cRuntimeError("TODO"); }

    virtual double getIntegral() const { throw cRuntimeError("TODO"); }
    virtual double getIntegral(const Interval<Quaternion>& i) const { throw cRuntimeError("TODO"); }

    virtual const Ptr<const IFunction<double, Domain<Quaternion>>> add(const Ptr<const IFunction<double, Domain<Quaternion>>>& o) const override { throw cRuntimeError("TODO"); }
    virtual const Ptr<const IFunction<double, Domain<Quaternion>>> subtract(const Ptr<const IFunction<double, Domain<Quaternion>>>& o) const override { throw cRuntimeError("TODO"); }
    virtual const Ptr<const IFunction<double, Domain<Quaternion>>> multiply(const Ptr<const IFunction<double, Domain<Quaternion>>>& o) const override { throw cRuntimeError("TODO"); }
    virtual const Ptr<const IFunction<double, Domain<Quaternion>>> divide(const Ptr<const IFunction<double, Domain<Quaternion>>>& o) const override { throw cRuntimeError("TODO"); }
};

} // namespace physicallayer

} // namespace inet

#endif // #ifndef __INET_POWERFUNCTION_H_

