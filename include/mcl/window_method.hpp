#pragma once
/**
	@file
	@brief window method
	@author MITSUNARI Shigeo(@herumi)
*/
#include <mcl/array.hpp>
#include <mcl/util.hpp>
#include <mcl/op.hpp>
#include <assert.h>

namespace mcl { namespace fp {

/*
	get w-bit size from x[0, bitSize)
	@param x [in] data
	@param bitSize [in] data size
	@param w [in] split size < UnitBitSize
*/
template<class T>
class ArrayIterator {
	const T *x_;
	size_t bitSize_;
	const size_t w_;
	const T mask_;
	size_t pos_;
public:
	static const size_t TbitSize = sizeof(T) * 8;
	ArrayIterator(const T *x, size_t bitSize, size_t w)
		: x_(x)
		, bitSize_(bitSize)
		, w_(w)
		, mask_(makeMask(w))
		, pos_(0)
	{
		assert(w_ <= TbitSize);
	}
	static T makeMask(size_t w)
	{
		return (w == TbitSize) ? ~T(0) : (T(1) << w) - 1;
	}
	bool hasNext() const { return bitSize_ > 0; }
	T getNext(size_t w = 0)
	{
		assert(hasNext() && w <= TbitSize);
		if (w == 0) w = w_;
		if (w > bitSize_) {
			w = bitSize_;
		}
		const T mask = w == w_ ? mask_ : makeMask(w);
		const size_t nextPos = pos_ + w;
		if (nextPos <= TbitSize) {
			T v = x_[0] >> pos_;
			if (nextPos < TbitSize) {
				pos_ = nextPos;
				v &= mask;
			} else {
				pos_ = 0;
				x_++;
			}
			bitSize_ -= w;
			return v;
		}
		T v = (x_[0] >> pos_) | (x_[1] << (TbitSize - pos_));
		v &= mask;
		pos_ = nextPos - TbitSize;
		bitSize_ -= w;
		x_++;
		return v;
	}
	// don't change iter
	bool peek1bit()
	{
		assert(hasNext());
		return (x_[0] >> pos_) & 1;
	}
	// ++iter
	void consume1bit()
	{
		assert(hasNext());
		const size_t nextPos = pos_ + 1;
		if (nextPos < TbitSize) {
			pos_ = nextPos;
		} else {
			pos_ = 0;
			x_++;
		}
		bitSize_ -= 1;
	}
};

template<class Ec>
class WindowMethod {
public:
	size_t bitSize_;
	size_t winSize_;
	mcl::Array<Ec> tbl_;
	WindowMethod(const Ec& x, size_t bitSize, size_t winSize)
	{
		init(x, bitSize, winSize);
	}
	WindowMethod()
		: bitSize_(0)
		, winSize_(0)
	{
	}
	/*
		@param x [in] base index
		@param bitSize [in] exponent bit length
		@param winSize [in] window size
	*/
	void init(bool *pb, const Ec& x, size_t bitSize, size_t winSize)
	{
		bitSize_ = bitSize;
		winSize_ = winSize;
		const size_t tblNum = (bitSize + winSize - 1) / winSize;
		const size_t r = size_t(1) << winSize;
		*pb = tbl_.resize(tblNum * r);
		if (!*pb) return;
		Ec t(x);
		for (size_t i = 0; i < tblNum; i++) {
			Ec* w = &tbl_[i * r];
			w[0].clear();
			for (size_t d = 1; d < r; d *= 2) {
				for (size_t j = 0; j < d; j++) {
					Ec::add(w[j + d], w[j], t);
				}
				Ec::dbl(t, t);
			}
			for (size_t j = 0; j < r; j++) {
				w[j].normalize();
			}
		}
	}
#ifndef CYBOZU_DONT_USE_EXCEPTION
	void init(const Ec& x, size_t bitSize, size_t winSize)
	{
		bool b;
		init(&b, x, bitSize, winSize);
		if (!b) throw cybozu::Exception("mcl:WindowMethod:init") << bitSize << winSize;
	}
#endif
	/*
		@param z [out] x multiplied by y
		@param y [in] exponent
	*/
	template<class tag2, size_t maxBitSize2, template<class tag2_, size_t maxBitSize2_> class FpT>
	void mul(Ec& z, const FpT<tag2, maxBitSize2>& y) const
	{
		fp::Block b;
		y.getBlock(b);
		powArray(z, b.p, b.n, false);
	}
	void mul(Ec& z, int64_t y) const
	{
#if MCL_SIZEOF_UNIT == 8
		Unit u = fp::abs_(y);
		powArray(z, &u, 1, y < 0);
#else
		uint64_t ua = fp::abs_(y);
		Unit u[2] = { uint32_t(ua), uint32_t(ua >> 32) };
		size_t un = u[1] ? 2 : 1;
		powArray(z, u, un, y < 0);
#endif
	}
	void mul(Ec& z, const mpz_class& y) const
	{
		powArray(z, gmp::getUnit(y), gmp::getUnitSize(y), y < 0);
	}
	void powArray(Ec& z, const Unit* y, size_t n, bool isNegative) const
	{
		z.clear();
		while (n > 0) {
			if (y[n - 1]) break;
			n--;
		}
		if (n == 0) return;
		assert((n << winSize_) <= tbl_.size());
		if ((n << winSize_) > tbl_.size()) return;
		assert(y[n - 1]);
		const size_t bitSize = (n - 1) * UnitBitSize + cybozu::bsr<Unit>(y[n - 1]) + 1;
		size_t i = 0;
		ArrayIterator<Unit> ai(y, bitSize, winSize_);
		do {
			Unit v = ai.getNext();
			if (v) {
				Ec::add(z, z, tbl_[(i << winSize_) + v]);
			}
			i++;
		} while (ai.hasNext());
		if (isNegative) {
			Ec::neg(z, z);
		}
	}
};

} } // mcl::fp

