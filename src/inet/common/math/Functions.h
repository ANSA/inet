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

#ifndef __INET_MATH_FUNCTIONS_H_
#define __INET_MATH_FUNCTIONS_H_

#include <algorithm>
#include "inet/common/math/IFunction.h"
#include "inet/common/math/Interpolators.h"

namespace inet {

namespace math {

using namespace inet::units::values;

template<typename R, typename D>
class INET_API DomainLimitedFunction;

template<typename R, typename D, int DIMS, typename RI, typename DI>
class INET_API IntegratedFunction;

template<typename R, typename D>
class INET_API AdditionFunction;

template<typename R, typename D>
class INET_API SubtractionFunction;

template<typename R, typename D>
class INET_API MultiplicationFunction;

template<typename R, typename D>
class INET_API DivisionFunction;

template<typename R, typename D>
class INET_API FunctionBase : public IFunction<R, D>
{
  public:
    virtual Interval<R> getRange() const override {
        return Interval<R>(getLowerBoundary<R>(), getUpperBoundary<R>());
    }

    virtual typename D::I getDomain() const override {
        return typename D::I(D::P::getLowerBoundaries(), D::P::getUpperBoundaries());
    }

    virtual R getMin() const override { return getMin(getDomain()); }
    virtual R getMin(const typename D::I& i) const override {
        R result(getUpperBoundary<R>());
        this->partition(i, [&] (const typename D::I& i1, const IFunction<R, D> *f) {
            result = std::min(f->getMin(i1), result);
        });
        return result;
    }

    virtual R getMax() const override { return getMax(getDomain()); }
    virtual R getMax(const typename D::I& i) const override {
        R result(getLowerBoundary<R>());
        this->partition(i, [&] (const typename D::I& i1, const IFunction<R, D> *f) {
            result = std::max(f->getMax(i1), result);
        });
        return result;
    }

    virtual R getMean() const override { return getMean(getDomain()); }
    virtual R getMean(const typename D::I& i) const override {
        return getIntegral(i) / i.getVolume();
    }

    virtual R getIntegral() const override { return getIntegral(getDomain()); }
    virtual R getIntegral(const typename D::I& i) const override {
        R result(0);
        this->partition(i, [&] (const typename D::I& i1, const IFunction<R, D> *f) {
            double volume = i1.getVolume();
            R value = f->getMean(i1);
            if (!(value == R(0) && std::isinf(volume)))
                result += volume * value;
        });
        return result;
    }

    virtual const Ptr<const IFunction<R, D>> add(const Ptr<const IFunction<R, D>>& o) const override {
        return makeShared<AdditionFunction<R, D>>(const_cast<FunctionBase<R, D> *>(this)->shared_from_this(), o);
    }

    virtual const Ptr<const IFunction<R, D>> subtract(const Ptr<const IFunction<R, D>>& o) const override {
        return makeShared<SubtractionFunction<R, D>>(const_cast<FunctionBase<R, D> *>(this)->shared_from_this(), o);
    }

    virtual const Ptr<const IFunction<R, D>> multiply(const Ptr<const IFunction<double, D>>& o) const override {
        return makeShared<MultiplicationFunction<R, D>>(const_cast<FunctionBase<R, D> *>(this)->shared_from_this(), o);
    }

    virtual const Ptr<const IFunction<double, D>> divide(const Ptr<const IFunction<R, D>>& o) const override {
        return makeShared<DivisionFunction<R, D>>(const_cast<FunctionBase<R, D> *>(this)->shared_from_this(), o);
    }
};

template<typename R, typename D, int DIMS, typename RI, typename DI>
Ptr<const IFunction<RI, DI>> integrate(const Ptr<const IFunction<R, D>>& f) {
    return makeShared<IntegratedFunction<R, D, DIMS, RI, DI>>(f);
}

template<typename R, typename D>
class INET_API DomainLimitedFunction : public FunctionBase<R, D>
{
  protected:
    const Ptr<const IFunction<R, D>> f;
    const typename D::I domain;

  public:
    DomainLimitedFunction(const Ptr<const IFunction<R, D>>& f, const typename D::I& domain) : f(f), domain(domain) { }

    virtual Interval<R> getRange() const override { return Interval<R>(this->getMin(domain), this->getMax(domain)); };
    virtual typename D::I getDomain() const override { return domain; };

    virtual R getValue(const typename D::P& p) const override {
        ASSERT(domain.contains(p));
        return f->getValue(p);
    }

    virtual void partition(const typename D::I& i, const std::function<void (const typename D::I&, const IFunction<R, D> *)> g) const override {
        const auto& i1 = i.intersect(domain);
        if (!i1.isEmpty())
            f->partition(i1, g);
    }
};

template<typename R, typename D>
class INET_API ConstantFunction : public FunctionBase<R, D>
{
  protected:
    const R r;

  public:
    ConstantFunction(R r) : r(r) { }

    virtual R getConstantValue() const { return r; }

    virtual Interval<R> getRange() const { return Interval<R>(r, r); }

    virtual R getValue(const typename D::P& p) const override { return r; }

    virtual void partition(const typename D::I& i, const std::function<void (const typename D::I&, const IFunction<R, D> *)> f) const override {
        f(i, this);
    }

    virtual R getMin(const typename D::I& i) const override { return r; }
    virtual R getMax(const typename D::I& i) const override { return r; }
    virtual R getMean(const typename D::I& i) const override { return r; }
    virtual R getIntegral(const typename D::I& i) const override { return r == R(0) ? r : r * i.getVolume(); }
};

template<typename R, typename X>
class INET_API OneDimensionalBoxcarFunction : public FunctionBase<R, Domain<X>>
{
  protected:
    const X lower;
    const X upper;
    const R r;

  public:
    OneDimensionalBoxcarFunction(X lower, X upper, R r) : lower(lower), upper(upper), r(r) {
        ASSERT(r > R(0));
    }

    virtual Interval<R> getRange() const { return Interval<R>(R(0), r); }

    virtual R getValue(const Point<X>& p) const override {
        return std::get<0>(p) < lower || std::get<0>(p) >= upper ? R(0) : r;
    }

    virtual void partition(const Interval<X>& i, const std::function<void (const Interval<X>&, const IFunction<R, Domain<X>> *)> f) const override {
        const auto& i1 = i.intersect(Interval<X>(getLowerBoundary<X>(), Point<X>(lower)));
        if (!i1.isEmpty()) {
            ConstantFunction<R, Domain<X>> g(R(0));
            f(i1, &g);
        }
        const auto& i2 = i.intersect(Interval<X>(Point<X>(lower), Point<X>(upper)));
        if (!i2.isEmpty()) {
            ConstantFunction<R, Domain<X>> g(r);
            f(i2, &g);
        }
        const auto& i3 = i.intersect(Interval<X>(Point<X>(upper), getUpperBoundary<X>()));
        if (!i3.isEmpty()) {
            ConstantFunction<R, Domain<X>> g(R(0));
            f(i3, &g);
        }
    }
};

template<typename R, typename X, typename Y>
class INET_API TwoDimensionalBoxcarFunction : public FunctionBase<R, Domain<X, Y>>
{
  protected:
    const X lowerX;
    const X upperX;
    const Y lowerY;
    const Y upperY;
    const R r;

  protected:
    void callf(const Interval<X, Y>& i, const std::function<void (const Interval<X, Y>&, const IFunction<R, Domain<X, Y>> *)> f, R r) const {
        if (!i.isEmpty()) {
            ConstantFunction<R, Domain<X, Y>> g(r);
            f(i, &g);
        }
    }

  public:
    TwoDimensionalBoxcarFunction(X lowerX, X upperX, Y lowerY, Y upperY, R r) : lowerX(lowerX), upperX(upperX), lowerY(lowerY), upperY(upperY), r(r) {
        ASSERT(r > R(0));
    }

    virtual Interval<R> getRange() const { return Interval<R>(R(0), r); }

    virtual R getValue(const Point<X, Y>& p) const override {
        return std::get<0>(p) < lowerX || std::get<0>(p) >= upperX || std::get<1>(p) < lowerY || std::get<1>(p) >= upperY ? R(0) : r;
    }

    virtual void partition(const Interval<X, Y>& i, const std::function<void (const Interval<X, Y>&, const IFunction<R, Domain<X, Y>> *)> f) const override {
        callf(i.intersect(Interval<X, Y>(Point<X, Y>(getLowerBoundary<X>(), getLowerBoundary<Y>()), Point<X, Y>(X(lowerX), Y(lowerY)))), f, R(0));
        callf(i.intersect(Interval<X, Y>(Point<X, Y>(X(lowerX), getLowerBoundary<Y>()), Point<X, Y>(X(upperX), Y(lowerY)))), f, R(0));
        callf(i.intersect(Interval<X, Y>(Point<X, Y>(X(upperX), getLowerBoundary<Y>()), Point<X, Y>(getUpperBoundary<X>(), Y(lowerY)))), f, R(0));

        callf(i.intersect(Interval<X, Y>(Point<X, Y>(getLowerBoundary<X>(), Y(lowerY)), Point<X, Y>(X(lowerX), Y(upperY)))), f, R(0));
        callf(i.intersect(Interval<X, Y>(Point<X, Y>(X(lowerX), Y(lowerY)), Point<X, Y>(X(upperX), Y(upperY)))), f, r);
        callf(i.intersect(Interval<X, Y>(Point<X, Y>(X(upperX), Y(lowerY)), Point<X, Y>(getUpperBoundary<X>(), Y(upperY)))), f, R(0));

        callf(i.intersect(Interval<X, Y>(Point<X, Y>(getLowerBoundary<X>(), Y(upperY)), Point<X, Y>(X(lowerX), getUpperBoundary<Y>()))), f, R(0));
        callf(i.intersect(Interval<X, Y>(Point<X, Y>(X(lowerX), Y(upperY)), Point<X, Y>(X(upperX), getUpperBoundary<Y>()))), f, R(0));
        callf(i.intersect(Interval<X, Y>(Point<X, Y>(X(upperX), Y(upperY)), Point<X, Y>(getUpperBoundary<X>(), getUpperBoundary<Y>()))), f, R(0));
    }
};

template<typename R, typename D>
class INET_API LinearInterpolatedFunction : public FunctionBase<R, D>
{
  protected:
    const typename D::P lower; // value is ignored in all but one dimension
    const typename D::P upper; // value is ignored in all but one dimension
    const R rLower;
    const R rUpper;
    const int dimension;

  public:
    LinearInterpolatedFunction(typename D::P lower, typename D::P upper, R rLower, R rUpper, int dimension) : lower(lower), upper(upper), rLower(rLower), rUpper(rUpper), dimension(dimension) { }

    virtual const typename D::P& getLower() const { return lower; }
    virtual const typename D::P& getUpper() const { return upper; }
    virtual R getRLower() const { return rLower; }
    virtual R getRUpper() const { return rUpper; }
    virtual int getDimension() const { return dimension; }

    virtual double getA() const { return toDouble(rUpper - rLower) / toDouble(upper.get(dimension) - lower.get(dimension)); }
    virtual double getB() const { return (toDouble(rLower) * upper.get(dimension) - toDouble(rUpper) * lower.get(dimension)) / (upper.get(dimension) - lower.get(dimension)); }

    virtual Interval<R> getRange() const { return Interval<R>(std::min(rLower, rUpper), std::max(rLower, rUpper)); }
    virtual typename D::I getDomain() const { return typename D::I(lower, upper); };

    virtual R getValue(const typename D::P& p) const override {
        double alpha = (p - lower).get(dimension) / (upper - lower).get(dimension);
        return rLower * (1 - alpha) + rUpper * alpha;
    }

    virtual void partition(const typename D::I& i, const std::function<void (const typename D::I&, const IFunction<R, D> *)> f) const override {
        f(i, this);
    }

    virtual R getMin(const typename D::I& i) const override {
        return std::min(getValue(i.getLower()), getValue(i.getUpper()));
    }

    virtual R getMax(const typename D::I& i) const override {
        return std::max(getValue(i.getLower()), getValue(i.getUpper()));
    }

    virtual R getMean(const typename D::I& i) const override {
        return getValue((i.getLower() + i.getUpper()) / 2);
    }
};

//template<typename R, typename D>
//class INET_API BilinearInterpolatedFunction : public FunctionBase<R, D>
//{
//  public:
//    virtual R getValue(const typename D::P& p) const override {
//        throw cRuntimeError("TODO");
//    }
//
//    virtual void partition(const typename D::I& i, const std::function<void (const typename D::I&, const IFunction<R, D> *)> f) const override {
//        throw cRuntimeError("TODO");
//    }
//};

template<typename R, typename X>
class INET_API OneDimensionalInterpolatedFunction : public FunctionBase<R, Domain<X>>
{
  protected:
    const std::map<X, std::pair<R, const IInterpolator<X, R> *>> rs;

  public:
    OneDimensionalInterpolatedFunction(const std::map<X, R>& rs, const IInterpolator<X, R> *interpolator) : rs([&] () {
        std::map<X, std::pair<R, const IInterpolator<X, R> *>> result;
        for (auto it : rs)
            result[it.first] = {it.second, interpolator};
        return result;
    } ()) { }

    OneDimensionalInterpolatedFunction(const std::map<X, std::pair<R, const IInterpolator<X, R> *>>& rs) : rs(rs) { }

    virtual R getValue(const Point<X>& p) const override {
        X x = std::get<0>(p);
        auto it = rs.equal_range(x);
        auto& lt = it.first;
        auto& ut = it.second;
        if (lt != rs.end() && lt->first == x)
            return lt->second.first;
        else {
            ASSERT(lt != rs.end() && ut != rs.end());
            lt--;
            const auto interpolator = lt->second.second;
            return interpolator->getValue(lt->first, lt->second.first, ut->first, ut->second.first, x);
        }
    }

    virtual void partition(const Interval<X>& i, const std::function<void (const Interval<X>&, const IFunction<R, Domain<X>> *)> f) const override {
        // loop from less or equal than lower to greater or equal than upper inclusive both ends
        auto lt = rs.lower_bound(std::get<0>(i.getLower()));
        auto ut = rs.upper_bound(std::get<0>(i.getUpper()));
        if (lt->first > std::get<0>(i.getLower()))
            lt--;
        if (ut == rs.end())
            ut--;
        for (auto it = lt; it != ut; it++) {
            auto jt = it;
            jt++;
            auto i1 = i.intersect(Interval<X>(Point<X>(it->first), Point<X>(jt->first)));
            if (!i1.isEmpty()) {
                const auto interpolator = it->second.second;
                if (dynamic_cast<const EitherInterpolator<X, R> *>(interpolator)) {
                    ConstantFunction<R, Domain<X>> g(it->second.first);
                    f(i1, &g);
                }
                else if (dynamic_cast<const SmallerInterpolator<X, R> *>(interpolator)) {
                    ConstantFunction<R, Domain<X>> g(it->second.first); // TODO: what about the ends?
                    f(i1, &g);
                }
                else if (dynamic_cast<const GreaterInterpolator<X, R> *>(interpolator)) {
                    ConstantFunction<R, Domain<X>> g(jt->second.first); // TODO: what about the ends?
                    f(i1, &g);
                }
                else if (dynamic_cast<const LinearInterpolator<X, R> *>(interpolator)) {
                    LinearInterpolatedFunction<R, Domain<X>> g(Point<X>(it->first), Point<X>(jt->first), it->second.first, jt->second.first, 0);
                    simplifyAndCall(i1, &g, f);
                }
                else
                    throw cRuntimeError("TODO");
            }
        }
    }
};

//template<typename R, typename X, typename Y>
//class INET_API TwoDimensionalInterpolatedFunction : public Function<R, X, Y>
//{
//  protected:
//    const IInterpolator<T, R>& xi;
//    const IInterpolator<T, R>& yi;
//    const std::vector<std::tuple<X, Y, R>> rs;
//
//  public:
//    TwoDimensionalInterpolatedFunction(const IInterpolator<T, R>& xi, const IInterpolator<T, R>& yi, const std::vector<std::tuple<X, Y, R>>& rs) :
//        xi(xi), yi(yi), rs(rs) { }
//
//    virtual R getValue(const Point<T>& p) const override {
//        throw cRuntimeError("TODO");
//    }
//
//    virtual void partition(const Interval<X, Y>& i, const std::function<void (const Interval<X, Y>&, const IFunction<R, Domain<X, Y>> *)> f) const override {
//        throw cRuntimeError("TODO");
//    }
//};

//template<typename R, typename D0, typename D>
//class INET_API FunctionInterpolatingFunction : public Function<R, D>
//{
//  protected:
//    const IInterpolator<R, D0>& i;
//    const std::map<D0, const IFunction<R, D> *> fs;
//
//  public:
//    FunctionInterpolatingFunction(const IInterpolator<R, D0>& i, const std::map<D0, const IFunction<R, D> *>& fs) : i(i), fs(fs) { }
//
//    virtual R getValue(const Point<D0, D>& p) const override {
//        D0 x = std::get<0>(p);
//        auto lt = fs.lower_bound(x);
//        auto ut = fs.upper_bound(x);
//        ASSERT(lt != fs.end() && ut != fs.end());
//        typename D::P q;
//        return i.get(lt->first, lt->second->getValue(q), ut->first, ut->second->getValue(q), x);
//    }
//
//    virtual void partition(const Interval<D0, D>& i, const std::function<void (const Interval<D0, D>&, const IFunction<R, D> *)> f) const override {
//        throw cRuntimeError("TODO");
//    }
//};

//template<typename R, typename D>
//class INET_API GaussFunction : public Function<R, D>
//{
//  protected:
//    const R mean;
//    const R stddev;
//
//  public:
//    GaussFunction(R mean, R stddev) : mean(mean), stddev(stddev) { }
//
//    virtual R getValue(const typename D::P& p) const override {
//        throw cRuntimeError("TODO");
//    }
//
//    virtual void partition(const typename D::I& i, const std::function<void (const typename D::I&, const IFunction<R, D> *)> f) const override {
//        throw cRuntimeError("TODO");
//    }
//};

template<typename R, typename X, typename Y>
class INET_API OrthogonalCombinatorFunction : public FunctionBase<R, Domain<X, Y>>
{
  protected:
    const Ptr<const IFunction<R, Domain<X>>> f;
    const Ptr<const IFunction<double, Domain<Y>>> g;

  public:
    OrthogonalCombinatorFunction(const Ptr<const IFunction<R, Domain<X>>>& f, const Ptr<const IFunction<double, Domain<Y>>>& g) : f(f), g(g) { }

    virtual R getValue(const Point<X, Y>& p) const override {
        return f->getValue(std::get<0>(p)) * g->getValue(std::get<1>(p));
    }

    virtual void partition(const Interval<X, Y>& i, const std::function<void (const Interval<X, Y>&, const IFunction<R, Domain<X, Y>> *)> h) const override {
        Interval<X> ix(Point<X>(std::get<0>(i.getLower())), Point<X>(std::get<0>(i.getUpper())), (i.getClosed() & 0b10) >> 1);
        Interval<Y> iy(Point<Y>(std::get<1>(i.getLower())), Point<Y>(std::get<1>(i.getUpper())), (i.getClosed() & 0b01) >> 0);
        f->partition(ix, [&] (const Interval<X>& ixf, const IFunction<R, Domain<X>> *if1) {
            g->partition(iy, [&] (const Interval<Y>& iyg, const IFunction<double, Domain<Y>> *if2) {
                Point<X, Y> lower(std::get<0>(ixf.getLower()), std::get<0>(iyg.getLower()));
                Point<X, Y> upper(std::get<0>(ixf.getUpper()), std::get<0>(iyg.getUpper()));
                unsigned int closed = (ixf.getClosed() << 1) | (iyg.getClosed() << 0);
                if (auto cif1 = dynamic_cast<const ConstantFunction<R, Domain<X>> *>(if1)) {
                    if (auto cif2 = dynamic_cast<const ConstantFunction<double, Domain<Y>> *>(if2)) {
                        ConstantFunction<R, Domain<X, Y>> g(cif1->getConstantValue() * cif2->getConstantValue());
                        h(Interval<X, Y>(lower, upper, closed), &g);
                    }
                    else if (auto lif2 = dynamic_cast<const LinearInterpolatedFunction<double, Domain<Y>> *>(if2)) {
                        LinearInterpolatedFunction<R, Domain<X, Y>> g(lower, upper, lif2->getValue(iyg.getLower()) * cif1->getConstantValue(), lif2->getValue(iyg.getUpper()) * cif1->getConstantValue(), 1);
                        simplifyAndCall(Interval<X, Y>(lower, upper, closed), &g, h);
                    }
                    else
                        throw cRuntimeError("TODO");
                }
                else if (auto lif1 = dynamic_cast<const LinearInterpolatedFunction<R, Domain<X>> *>(if1)) {
                    if (auto cif2 = dynamic_cast<const ConstantFunction<double, Domain<Y>> *>(if2)) {
                        LinearInterpolatedFunction<R, Domain<X, Y>> g(lower, upper, lif1->getValue(ixf.getLower()) * cif2->getConstantValue(), lif1->getValue(ixf.getUpper()) * cif2->getConstantValue(), 0);
                        simplifyAndCall(Interval<X, Y>(lower, upper, closed), &g, h);
                    }
                    else {
                        // QuadraticFunction<double, Domain<X, Y>> g(); // TODO:
                        throw cRuntimeError("TODO");
                    }
                }
                else
                    throw cRuntimeError("TODO");
            });
        });
    }
};

template<typename R, typename D>
class INET_API ShiftFunction : public FunctionBase<R, D>
{
  protected:
    const Ptr<const IFunction<R, D>> f;
    const typename D::P s;

  public:
    ShiftFunction(const Ptr<const IFunction<R, D>>& f, const typename D::P& s) : f(f), s(s) { }

    virtual R getValue(const typename D::P& p) const override {
        return f->getValue(p - s);
    }

    virtual void partition(const typename D::I& i, const std::function<void (const typename D::I&, const IFunction<R, D> *)> g) const override {
        f->partition(typename D::I(i.getLower() - s, i.getUpper() - s, i.getClosed()), [&] (const typename D::I& j, const IFunction<R, D> *jf) {
            if (auto cjf = dynamic_cast<const ConstantFunction<R, D> *>(jf))
                g(typename D::I(j.getLower() + s, j.getUpper() + s, j.getClosed()), jf);
            else if (auto ljf = dynamic_cast<const LinearInterpolatedFunction<R, D> *>(jf)) {
                LinearInterpolatedFunction<R, D> h(j.getLower() + s, j.getUpper() + s, ljf->getValue(j.getLower()), ljf->getValue(j.getUpper()), ljf->getDimension());
                simplifyAndCall(typename D::I(j.getLower() + s, j.getUpper() + s, j.getClosed()), &h, g);
            }
            else {
                ShiftFunction h(const_cast<IFunction<R, D> *>(jf)->shared_from_this(), s);
                simplifyAndCall(typename D::I(j.getLower() + s, j.getUpper() + s, j.getClosed()), &h, g);
            }
        });
    }
};

// TODO:
//template<typename R, typename D>
//class INET_API QuadraticFunction : public Function<R, D>
//{
//  protected:
//    const double a;
//    const double b;
//    const double c;
//
//  public:
//    QuadraticFunction(double a, double b, double c) : a(a), b(b), c(c) { }
//
//    virtual R getValue(const typename D::P& p) const override {
//        return R(0);
//    }
//
//    virtual void partition(const typename D::I& i, const std::function<void (const typename D::I&, const IFunction<R, D> *)> f) const override {
//        throw cRuntimeError("TODO");
//    }
//};

template<typename R, typename D>
class INET_API ReciprocalFunction : public FunctionBase<R, D>
{
  protected:
    // f(x) = (a * x + b) / (c * x + d)
    const double a;
    const double b;
    const double c;
    const double d;
    const int dimension;

  protected:
    double getIntegral(const typename D::P& p) const {
        // https://www.wolframalpha.com/input/?i=integrate+(a+*+x+%2B+b)+%2F+(c+*+x+%2B+d)
        double x = p.get(dimension);
        return (a * c * x + (b * c - a * d) * std::log(d + c * x)) / (c * c);
    }

  public:
    ReciprocalFunction(double a, double b, double c, double d, int dimension) : a(a), b(b), c(c), d(d), dimension(dimension) { }

    virtual int getDimension() const { return dimension; }

    virtual R getValue(const typename D::P& p) const override {
        double x = p.get(dimension);
        return R(a * x + b) / (c * x + d);
    }

    virtual void partition(const typename D::I& i, const std::function<void (const typename D::I&, const IFunction<R, D> *)> f) const override {
        f(i, this);
    }

    virtual R getMin(const typename D::I& i) const override {
        double x = -d / c;
        if (i.getLower().get(dimension) < x && x < i.getUpper().get(dimension))
            return getLowerBoundary<R>();
        else
            return std::min(getValue(i.getLower()), getValue(i.getUpper()));
    }

    virtual R getMax(const typename D::I& i) const override {
        double x = -d / c;
        if (i.getLower().get(dimension) < x && x < i.getUpper().get(dimension))
            return getUpperBoundary<R>();
        else
            return std::max(getValue(i.getLower()), getValue(i.getUpper()));
    }

    virtual R getMean(const typename D::I& i) const override {
        return R(getIntegral(i.getUpper()) - getIntegral(i.getLower())) / (i.getUpper().get(dimension) - i.getLower().get(dimension));
    }
};

template<typename R, typename D>
class INET_API AdditionFunction : public FunctionBase<R, D>
{
  protected:
    const Ptr<const IFunction<R, D>> f1;
    const Ptr<const IFunction<R, D>> f2;

  public:
    AdditionFunction(const Ptr<const IFunction<R, D>>& f1, const Ptr<const IFunction<R, D>>& f2) : f1(f1), f2(f2) { }

    virtual R getValue(const typename D::P& p) const override {
        return f1->getValue(p) + f2->getValue(p);
    }

    virtual void partition(const typename D::I& i, const std::function<void (const typename D::I&, const IFunction<R, D> *)> f) const override {
        f1->partition(i, [&] (const typename D::I& i1, const IFunction<R, D> *if1) {
            f2->partition(i1, [&] (const typename D::I& i2, const IFunction<R, D> *if2) {
                // TODO: use template specialization for compile time optimization
                if (auto cif1 = dynamic_cast<const ConstantFunction<R, D> *>(if1)) {
                    if (auto cif2 = dynamic_cast<const ConstantFunction<R, D> *>(if2)) {
                        ConstantFunction<R, D> g(cif1->getConstantValue() + cif2->getConstantValue());
                        f(i2, &g);
                    }
                    else if (auto lif2 = dynamic_cast<const LinearInterpolatedFunction<R, D> *>(if2)) {
                        LinearInterpolatedFunction<R, D> g(i2.getLower(), i2.getUpper(), lif2->getValue(i2.getLower()) + cif1->getConstantValue(), lif2->getValue(i2.getUpper()) + cif1->getConstantValue(), lif2->getDimension());
                        simplifyAndCall(i2, &g, f);
                    }
                    else
                        throw cRuntimeError("TODO");
                }
                else if (auto lif1 = dynamic_cast<const LinearInterpolatedFunction<R, D> *>(if1)) {
                    if (auto cif2 = dynamic_cast<const ConstantFunction<R, D> *>(if2)) {
                        LinearInterpolatedFunction<R, D> g(i2.getLower(), i2.getUpper(), lif1->getValue(i2.getLower()) + cif2->getConstantValue(), lif1->getValue(i2.getUpper()) + cif2->getConstantValue(), lif1->getDimension());
                        simplifyAndCall(i2, &g, f);
                    }
                    else if (auto lif2 = dynamic_cast<const LinearInterpolatedFunction<R, D> *>(if2)) {
                        if (lif1->getDimension() == lif2->getDimension()) {
                            LinearInterpolatedFunction<R, D> g(i2.getLower(), i2.getUpper(), lif1->getValue(i2.getLower()) + lif2->getValue(i2.getLower()), lif1->getValue(i2.getUpper()) + lif2->getValue(i2.getUpper()), lif1->getDimension());
                            simplifyAndCall(i2, &g, f);
                        }
                        else
                            throw cRuntimeError("TODO");
                    }
                    else
                        throw cRuntimeError("TODO");
                }
                else
                    throw cRuntimeError("TODO");
            });
        });
    }
};

template<typename R, typename D>
class INET_API SubtractionFunction : public FunctionBase<R, D>
{
  protected:
    const Ptr<const IFunction<R, D>> f1;
    const Ptr<const IFunction<R, D>> f2;

  public:
    SubtractionFunction(const Ptr<const IFunction<R, D>>& f1, const Ptr<const IFunction<R, D>>& f2) : f1(f1), f2(f2) { }

    virtual R getValue(const typename D::P& p) const override {
        return f1->getValue(p) - f2->getValue(p);
    }

    virtual void partition(const typename D::I& i, const std::function<void (const typename D::I&, const IFunction<R, D> *)> f) const override {
        f1->partition(i, [&] (const typename D::I& i1, const IFunction<R, D> *if1) {
            f2->partition(i1, [&] (const typename D::I& i2, const IFunction<R, D> *if2) {
                if (auto cif1 = dynamic_cast<const ConstantFunction<R, D> *>(if1)) {
                    if (auto cif2 = dynamic_cast<const ConstantFunction<R, D> *>(if2)) {
                        ConstantFunction<R, D> g(cif1->getConstantValue() - cif2->getConstantValue());
                        f(i2, &g);
                    }
                    else if (auto lif2 = dynamic_cast<const LinearInterpolatedFunction<R, D> *>(if2)) {
                        LinearInterpolatedFunction<R, D> g(i2.getLower(), i2.getUpper(), lif2->getValue(i2.getLower()) - cif1->getConstantValue(), lif2->getValue(i2.getUpper()) - cif1->getConstantValue(), lif2->getDimension());
                        simplifyAndCall(i2, &g, f);
                    }
                    else
                        throw cRuntimeError("TODO");
                }
                else if (auto lif1 = dynamic_cast<const LinearInterpolatedFunction<R, D> *>(if1)) {
                    if (auto cif2 = dynamic_cast<const ConstantFunction<R, D> *>(if2)) {
                        LinearInterpolatedFunction<R, D> g(i2.getLower(), i2.getUpper(), lif1->getValue(i2.getLower()) - cif2->getConstantValue(), lif1->getValue(i2.getUpper()) - cif2->getConstantValue(), lif1->getDimension());
                        simplifyAndCall(i2, &g, f);
                    }
                    else if (auto lif2 = dynamic_cast<const LinearInterpolatedFunction<R, D> *>(if2)) {
                        if (lif1->getDimension() == lif2->getDimension()) {
                            LinearInterpolatedFunction<R, D> g(i2.getLower(), i2.getUpper(), lif1->getValue(i2.getLower()) - lif2->getValue(i2.getLower()), lif1->getValue(i2.getUpper()) - lif2->getValue(i2.getUpper()), lif1->getDimension());
                            simplifyAndCall(i2, &g, f);
                        }
                        else
                            throw cRuntimeError("TODO");
                    }
                    else
                        throw cRuntimeError("TODO");
                }
                else
                    throw cRuntimeError("TODO");
            });
        });
    }
};

template<typename R, typename D>
class INET_API MultiplicationFunction : public FunctionBase<R, D>
{
  protected:
    const Ptr<const IFunction<R, D>> f1;
    const Ptr<const IFunction<double, D>> f2;

  public:
    MultiplicationFunction(const Ptr<const IFunction<R, D>>& f1, const Ptr<const IFunction<double, D>>& f2) : f1(f1), f2(f2) { }

    virtual R getValue(const typename D::P& p) const override {
        return f1->getValue(p) * f2->getValue(p);
    }

    virtual void partition(const typename D::I& i, const std::function<void (const typename D::I&, const IFunction<R, D> *)> f) const override {
        f1->partition(i, [&] (const typename D::I& i1, const IFunction<R, D> *if1) {
            f2->partition(i1, [&] (const typename D::I& i2, const IFunction<double, D> *if2) {
                if (auto cif1 = dynamic_cast<const ConstantFunction<R, D> *>(if1)) {
                    if (auto cif2 = dynamic_cast<const ConstantFunction<double, D> *>(if2)) {
                        ConstantFunction<R, D> g(cif1->getConstantValue() * cif2->getConstantValue());
                        f(i2, &g);
                    }
                    else if (auto lif2 = dynamic_cast<const LinearInterpolatedFunction<double, D> *>(if2)) {
                        LinearInterpolatedFunction<R, D> g(i2.getLower(), i2.getUpper(), lif2->getValue(i2.getLower()) * cif1->getConstantValue(), lif2->getValue(i2.getUpper()) * cif1->getConstantValue(), lif2->getDimension());
                        simplifyAndCall(i2, &g, f);
                    }
                    else
                        throw cRuntimeError("TODO");
                }
                else if (auto lif1 = dynamic_cast<const LinearInterpolatedFunction<R, D> *>(if1)) {
                    if (auto cif2 = dynamic_cast<const ConstantFunction<double, D> *>(if2)) {
                        LinearInterpolatedFunction<R, D> g(i2.getLower(), i2.getUpper(), lif1->getValue(i2.getLower()) * cif2->getConstantValue(), lif1->getValue(i2.getUpper()) * cif2->getConstantValue(), lif1->getDimension());
                        simplifyAndCall(i2, &g, f);
                    }
                    else if (auto lif2 = dynamic_cast<const LinearInterpolatedFunction<double, D> *>(if2)) {
                        // QuadraticFunction<double, D> g(); // TODO:
                        throw cRuntimeError("TODO");
                    }
                    else
                        throw cRuntimeError("TODO");
                }
                else
                    throw cRuntimeError("TODO");
            });
        });
    }
};

template<typename R, typename D>
class INET_API DivisionFunction : public FunctionBase<double, D>
{
  protected:
    const Ptr<const IFunction<R, D>> f1;
    const Ptr<const IFunction<R, D>> f2;

  public:
    DivisionFunction(const Ptr<const IFunction<R, D>>& f1, const Ptr<const IFunction<R, D>>& f2) : f1(f1), f2(f2) { }

    virtual double getValue(const typename D::P& p) const override {
        return unit(f1->getValue(p) / f2->getValue(p)).get();
    }

    virtual void partition(const typename D::I& i, const std::function<void (const typename D::I&, const IFunction<double, D> *)> f) const override {
        f1->partition(i, [&] (const typename D::I& i1, const IFunction<R, D> *if1) {
            f2->partition(i1, [&] (const typename D::I& i2, const IFunction<R, D> *if2) {
                if (auto cif1 = dynamic_cast<const ConstantFunction<R, D> *>(if1)) {
                    if (auto cif2 = dynamic_cast<const ConstantFunction<R, D> *>(if2)) {
                        ConstantFunction<double, D> g(unit(cif1->getConstantValue() / cif2->getConstantValue()).get());
                        f(i2, &g);
                    }
                    else if (auto lif2 = dynamic_cast<const LinearInterpolatedFunction<R, D> *>(if2)) {
                        ReciprocalFunction<double, D> g(0, toDouble(cif1->getConstantValue()), lif2->getA(), lif2->getB(), lif2->getDimension());
                        simplifyAndCall(i2, &g, f);
                    }
                    else
                        throw cRuntimeError("TODO");
                }
                else if (auto lif1 = dynamic_cast<const LinearInterpolatedFunction<R, D> *>(if1)) {
                    if (auto cif2 = dynamic_cast<const ConstantFunction<R, D> *>(if2)) {
                        LinearInterpolatedFunction<double, D> g(i2.getLower(), i2.getUpper(), unit(lif1->getValue(i2.getLower()) / cif2->getConstantValue()).get(), unit(lif1->getValue(i2.getUpper()) / cif2->getConstantValue()).get(), lif1->getDimension());
                        simplifyAndCall(i2, &g, f);
                    }
                    else if (auto lif2 = dynamic_cast<const LinearInterpolatedFunction<R, D> *>(if2)) {
                        if (lif1->getDimension() == lif2->getDimension()) {
                            ReciprocalFunction<double, D> g(lif1->getA(), lif1->getB(), lif2->getA(), lif2->getB(), lif2->getDimension());
                            simplifyAndCall(i2, &g, f);
                        }
                        else
                            throw cRuntimeError("TODO");
                    }
                    else
                        throw cRuntimeError("TODO");
                }
                else
                    throw cRuntimeError("TODO");
            });
        });
    }
};

template<typename R, typename D>
class INET_API SumFunction : public FunctionBase<R, D>
{
  protected:
    std::vector<Ptr<const IFunction<R, D>>> fs;

  public:
    SumFunction() { }
    SumFunction(const std::vector<Ptr<const IFunction<R, D>>>& fs) : fs(fs) { }

    const std::vector<Ptr<const IFunction<R, D>>>& getElements() const { return fs; }

    virtual void addElement(const Ptr<const IFunction<R, D>>& f) {
        fs.push_back(f);
    }

    virtual void removeElement(const Ptr<const IFunction<R, D>>& f) {
        fs.erase(std::remove(fs.begin(), fs.end(), f), fs.end());
    }

    virtual R getValue(const typename D::P& p) const override {
        R sum = R(0);
        for (auto f : fs)
            sum += f->getValue(p);
        return sum;
    }

    virtual void partition(const typename D::I& i, const std::function<void (const typename D::I&, const IFunction<R, D> *)> f) const override {
        ConstantFunction<R, D> g(R(0));
        partition(0, i, f, &g);
    }

    virtual void partition(int index, const typename D::I& i, const std::function<void (const typename D::I&, const IFunction<R, D> *)> f, const IFunction<R, D> *g) const {
        if (index == (int)fs.size())
            f(i, g);
        else {
            fs[index]->partition(i, [&] (const typename D::I& i1, const IFunction<R, D> *h) {
                if (auto cg = dynamic_cast<const ConstantFunction<R, D> *>(g)) {
                    if (auto ch = dynamic_cast<const ConstantFunction<R, D> *>(h)) {
                        ConstantFunction<R, D> j(cg->getConstantValue() + ch->getConstantValue());
                        partition(index + 1, i1, f, &j);
                    }
                    else if (auto lh = dynamic_cast<const LinearInterpolatedFunction<R, D> *>(h)) {
                        LinearInterpolatedFunction<R, D> j(i1.getLower(), i1.getUpper(), lh->getValue(i1.getLower()) + cg->getConstantValue(), lh->getValue(i1.getUpper()) + cg->getConstantValue(), lh->getDimension());
                        partition(index + 1, i1, f, &j);
                    }
                    else
                        throw cRuntimeError("TODO");
                }
                else if (auto lg = dynamic_cast<const LinearInterpolatedFunction<R, D> *>(g)) {
                    if (auto ch = dynamic_cast<const ConstantFunction<R, D> *>(h)) {
                        LinearInterpolatedFunction<R, D> j(i1.getLower(), i1.getUpper(), lg->getValue(i1.getLower()) + ch->getConstantValue(), lg->getValue(i1.getUpper()) + ch->getConstantValue(), lg->getDimension());
                        partition(index + 1, i1, f, &j);
                    }
                    else if (auto lh = dynamic_cast<const LinearInterpolatedFunction<R, D> *>(h)) {
                        if (lg->getDimension() == lh->getDimension()) {
                            LinearInterpolatedFunction<R, D> j(i1.getLower(), i1.getUpper(), lg->getValue(i1.getLower()) + lh->getValue(i1.getLower()), lg->getValue(i1.getUpper()) + lh->getValue(i1.getUpper()), lg->getDimension());
                            partition(index + 1, i1, f, &j);
                        }
                        else
                            throw cRuntimeError("TODO");
                    }
                    else
                        throw cRuntimeError("TODO");
                }
                else
                    throw cRuntimeError("TODO");
            });
        }
    }
};

template<typename R, typename X, typename Y, int DIMS, typename RI>
class IntegratedFunction<R, Domain<X, Y>, DIMS, RI, Domain<X>> : public FunctionBase<RI, Domain<X>>
{
    const Ptr<const IFunction<R, Domain<X, Y>>> f;

  public:
    IntegratedFunction(const Ptr<const IFunction<R, Domain<X, Y>>>& f): f(f) { }

    virtual RI getValue(const Point<X>& p) const override {
        Point<X, Y> l1(std::get<0>(p), getLowerBoundary<Y>());
        Point<X, Y> u1(std::get<0>(p), getUpperBoundary<Y>());
        RI ri(0);
        Interval<X, Y> i1(l1, u1, DIMS);
        f->partition(i1, [&] (const Interval<X, Y>& i2, const IFunction<R, Domain<X, Y>> *g) {
            R r = g->getIntegral(i2);
            ri += RI(toDouble(r));
        });
        return ri;
    }

    virtual void partition(const Interval<X>& i, std::function<void (const Interval<X>&, const IFunction<RI, Domain<X>> *)> g) const override {
        Point<X, Y> l1(std::get<0>(i.getLower()), getLowerBoundary<Y>());
        Point<X, Y> u1(std::get<0>(i.getUpper()), getUpperBoundary<Y>());
        Interval<X, Y> i1(l1, u1);
        std::set<X> xs;
        f->partition(i1, [&] (const Interval<X, Y>& i2, const IFunction<R, Domain<X, Y>> *h) {
            xs.insert(std::get<0>(i2.getLower()));
            xs.insert(std::get<0>(i2.getUpper()));
        });
        bool first = true;
        X xLower;
        for (auto it : xs) {
            X xUpper = it;
            if (first)
                first = false;
            else {
                RI ri(0);
                // NOTE: use the lower X for both interval ends, because we assume a constant function and intervals are closed at the lower end
                Point<X, Y> l3(xLower, getLowerBoundary<Y>());
                Point<X, Y> u3(xLower, getUpperBoundary<Y>());
                Interval<X, Y> i3(l3, u3, DIMS);
                f->partition(i3, [&] (const Interval<X, Y>& i4, const IFunction<R, Domain<X, Y>> *h) {
                    if (dynamic_cast<const ConstantFunction<R, Domain<X, Y>> *>(h)) {
                        R r = h->getIntegral(i4);
                        ri += RI(toDouble(r));
                    }
                    else if (auto lh = dynamic_cast<const LinearInterpolatedFunction<R, Domain<X, Y>> *>(h)) {
                        if (lh->getDimension() == 1) {
                            R r = h->getIntegral(i4);
                            ri += RI(toDouble(r));
                        }
                        else
                            throw cRuntimeError("TODO");
                    }
                    else
                        throw cRuntimeError("TODO");
                });
                ConstantFunction<RI, Domain<X>> h(ri);
                Point<X> l5(xLower);
                Point<X> u5(xUpper);
                Interval<X> i5(l5, u5);
                g(i5, &h);
            }
            xLower = xUpper;
        }
    }
};

template<typename R, typename D, int DIMS, typename RI, typename DI>
class IntegratedFunction : public FunctionBase<RI, DI>
{
    const Ptr<const IFunction<R, D>> f;

  public:
    IntegratedFunction(const Ptr<const IFunction<R, D>>& f): f(f) { }

    virtual RI getValue(const typename DI::P& p) const override {
        auto l1 = D::P::getLowerBoundaries();
        auto u1 = D::P::getUpperBoundaries();
        p.template copyTo<typename D::P, DIMS>(l1);
        p.template copyTo<typename D::P, DIMS>(u1);
        RI ri(0);
        typename D::I i1(l1, u1, DIMS);
        f->partition(i1, [&] (const typename D::I& i2, const IFunction<R, D> *g) {
            R r = g->getIntegral(i2);
            ri += RI(toDouble(r));
        });
        return ri;
    }

    virtual void partition(const typename DI::I& i, std::function<void (const typename DI::I&, const IFunction<RI, DI> *)> g) const override {
        throw cRuntimeError("TODO");
    }
};

template<typename R, typename D>
class INET_API MemoizedFunction : public FunctionBase<R, D>
{
  protected:
    const Ptr<const IFunction<R, D>> f;

  public:
    MemoizedFunction(const Ptr<const IFunction<R, D>>& f) : f(f) {
        f->partition(f->getDomain(), [] (const typename D::I& i, const IFunction<R, D> *g) {
            // TODO: store all interval function pairs in a domain subdivision tree structure
        });
    }

    virtual R getValue(const typename D::P& p) const override {
        f->getValue(p);
    }

    virtual void partition(const typename D::I& i, std::function<void (const typename D::I&, const IFunction<R, D> *)> g) const override {
        // TODO: search in domain subdivision tree structure
        throw cRuntimeError("TODO");
    }
};

template<typename R, typename D>
void simplifyAndCall(const typename D::I& i, const LinearInterpolatedFunction<R, D> *f, const std::function<void (const typename D::I&, const IFunction<R, D> *)> g) {
    if (f->getRLower() == f->getRUpper()) {
        ConstantFunction<R, D> h(f->getRLower());
        g(i, &h);
    }
    else
        g(i, f);
}

template<typename R, typename D>
void simplifyAndCall(const typename D::I& i, const IFunction<R, D> *f, const std::function<void (const typename D::I&, const IFunction<R, D> *)> g) {
    g(i, f);
}

} // namespace math

} // namespace inet

#endif // #ifndef __INET_MATH_FUNCTIONS_H_

