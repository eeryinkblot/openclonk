/*
 * OpenClonk, http://www.openclonk.org
 *
 * Copyright (c) 2026, The OpenClonk Team and contributors
 *
 * Distributed under the terms of the ISC license; see accompanying file
 * "COPYING" for details.
 *
 * "Clonk" is a registered trademark of Matthes Bender, used with permission.
 * See accompanying file "TRADEMARK" for details.
 *
 * To redistribute this file separately, substitute the full license texts
 * for the above references.
 */

// Tests C4Real, the fixed-point type gameplay positions are computed in.
//
// These are deliberately written against exact bit patterns rather than
// approximate values. C4Real exists because floating point is not reproducible
// across compilers, and clients exchange inputs rather than state -- so a
// change that shifts any of these results by one unit does not produce a
// slightly different game, it produces a desync between two machines running
// different builds. An assertion of the form EXPECT_NEAR would pass through
// exactly the change that matters.
//
// Everything asserted here is derived from the definitions in C4Real.h, not
// captured from a run. Where a value looks arbitrary, the derivation is in the
// comment above it.

#include <gtest/gtest.h>

#include "C4Include.h"
#include "lib/C4Real.h"

namespace
{

// The scaling factor is the whole contract of the representation: 16 fractional
// bits, so 1.0 is 65536. Every other expectation in this file depends on it.
TEST(C4RealTest, ScaleIsSixteenFractionalBits)
{
	EXPECT_EQ(65536, FIXED_FPF);
	EXPECT_EQ(16, FIXED_SHIFT);

	EXPECT_EQ(0, itofix(0).val);
	EXPECT_EQ(65536, itofix(1).val);
	EXPECT_EQ(-65536, itofix(-1).val);
	EXPECT_EQ(196608, itofix(3).val);
	EXPECT_EQ(-196608, itofix(-3).val);
}

TEST(C4RealTest, IntegerRoundTrip)
{
	for (int i = -1000; i <= 1000; ++i)
	{
		EXPECT_EQ(i, fixtoi(itofix(i))) << "round trip failed for " << i;
	}
}

// itofix(x, prec) is x/prec. The two-argument constructor splits the division
// to keep the intermediate in 32 bits when prec is small:
//     val = x * (FPF / prec) + (x * (FPF % prec)) / prec
// so the results below are not simply x * 65536 / prec, and the difference is
// visible wherever prec does not divide 65536.
TEST(C4RealTest, PrecisionConversionIsExactWhereItCanBe)
{
	// 65536 / 2 = 32768 exactly.
	EXPECT_EQ(32768, itofix(1, 2).val);
	// 65536 / 4 = 16384 exactly.
	EXPECT_EQ(16384, itofix(1, 4).val);

	// 3 does not divide 65536: 1 * (65536/3) + (1 * (65536%3))/3
	//                        = 21845      + (1 * 1)/3
	//                        = 21845, i.e. 0.333328..., one unit short of exact.
	EXPECT_EQ(21845, itofix(1, 3).val);
}

// The three named helpers take different routes to a half and must all land on
// the same bit pattern. C4REAL100(50) goes through the split division above,
// C4REAL256 multiplies before dividing, C4REAL10 splits again with a different
// remainder. Anything that changes one of them and not the others is a bug in
// the one that moved.
TEST(C4RealTest, NamedConstructorsAgreeOnAHalf)
{
	EXPECT_EQ(32768, C4REAL100(50).val);
	EXPECT_EQ(32768, C4REAL256(128).val);
	EXPECT_EQ(32768, C4REAL10(5).val);
	EXPECT_EQ(32768, itofix(1, 2).val);
}

// to_int() rounds towards positive infinity, not towards nearest-even and not
// towards zero:
//     r = val >> (SHIFT-1); r += 1; r >>= 1;
// The negative half is the case that catches a change of rounding mode, since
// -0.5 goes to 0 rather than to -1.
TEST(C4RealTest, RoundingGoesTowardsPositiveInfinity)
{
	EXPECT_EQ(1, fixtoi(itofix(1, 2)));    //  0.5 -> 1
	EXPECT_EQ(0, fixtoi(-itofix(1, 2)));   // -0.5 -> 0

	EXPECT_EQ(1, fixtoi(itofix(3, 4)));    //  0.75 -> 1
	EXPECT_EQ(-1, fixtoi(-itofix(3, 4)));  // -0.75 -> -1

	EXPECT_EQ(0, fixtoi(itofix(1, 4)));    //  0.25 -> 0
	EXPECT_EQ(0, fixtoi(-itofix(1, 4)));   // -0.25 -> 0
}

// Multiplication is (int64(a) * b) / FPF, truncating towards zero. The third
// case is the one worth having: a third times a third is not exactly a ninth,
// and the direction of the error is part of the contract.
TEST(C4RealTest, MultiplicationTruncatesTowardsZero)
{
	EXPECT_EQ(itofix(6).val, (itofix(2) * itofix(3)).val);
	EXPECT_EQ(32768, (itofix(1) * itofix(1, 2)).val);

	// 21845 * 21845 / 65536 = 7281.66..., truncated to 7281.
	// An exact ninth would be 7281.77..., so the result is one unit low.
	EXPECT_EQ(7281, (itofix(1, 3) * itofix(1, 3)).val);

	// Negative operands truncate towards zero as well, not towards -infinity.
	EXPECT_EQ(-7281, (itofix(1, 3) * -itofix(1, 3)).val);
}

TEST(C4RealTest, DivisionIsTheInverseOfMultiplication)
{
	EXPECT_EQ(itofix(2).val, (itofix(6) / itofix(3)).val);
	EXPECT_EQ(itofix(2).val, (itofix(1) / itofix(1, 2)).val);
	EXPECT_EQ(-itofix(2).val, (itofix(6) / -itofix(3)).val);
}

// The integer and C4Real overloads are different operations -- *= on an int32_t
// multiplies the raw value, while the C4Real form computes
// (int64(a) * b) / FPF -- but for a whole-number operand the rescale cancels
// exactly, so they have to agree. They are separate code paths, so nothing but
// a test enforces that.
//
// Note what this is *not*: an inexact value stays inexact through either route.
// itofix(1,3) is 21845, one unit short of a third, and tripling it gives 65535
// rather than 65536. The error is in the conversion, not in the multiplication,
// and it does not grow.
TEST(C4RealTest, IntegerAndRealOperandsAgreeForWholeNumbers)
{
	C4Real a = itofix(1, 3);
	EXPECT_EQ(65535, (a * 3).val);
	EXPECT_EQ(65535, (a * itofix(3)).val);
	EXPECT_EQ((a * 3).val, (a * itofix(3)).val);

	EXPECT_EQ((a / 3).val, (a / itofix(3)).val);

	C4Real b = C4REAL100(50);
	EXPECT_EQ((b * 7).val, (b * itofix(7)).val);
}

// Sin and Cos read a table of 9001 entries covering 0 to 90 degrees in
// hundredths. The endpoints are exact by construction -- SineTable[0] is 0 and
// SineTable[9000] is 65536 -- so the quadrant arithmetic can be checked without
// depending on any interior value of the table.
TEST(C4RealTest, SineAndCosineAreExactAtTheQuadrantBoundaries)
{
	EXPECT_EQ(0, Sin(itofix(0)).val);
	EXPECT_EQ(65536, Sin(itofix(90)).val);
	EXPECT_EQ(0, Sin(itofix(180)).val);
	EXPECT_EQ(-65536, Sin(itofix(270)).val);
	EXPECT_EQ(0, Sin(itofix(360)).val);

	EXPECT_EQ(65536, Cos(itofix(0)).val);
	EXPECT_EQ(0, Cos(itofix(90)).val);
	EXPECT_EQ(-65536, Cos(itofix(180)).val);
	EXPECT_EQ(0, Cos(itofix(270)).val);
	EXPECT_EQ(65536, Cos(itofix(360)).val);
}

// Negative angles are folded differently by the two functions -- sin_deg uses
// v = 18000 - v, cos_deg uses v = -v -- which is where an off-by-one in the
// folding would show up.
TEST(C4RealTest, NegativeAnglesFoldCorrectly)
{
	EXPECT_EQ(-Sin(itofix(90)).val, Sin(itofix(-90)).val);
	EXPECT_EQ(Cos(itofix(90)).val, Cos(itofix(-90)).val);
	EXPECT_EQ(Cos(itofix(180)).val, Cos(itofix(-180)).val);
}

// Sin and Cos must stay consistent with each other, since gameplay code uses
// both to decompose a velocity. Checked over a full turn rather than at a few
// points, and with an exact bound: the table has 16 fractional bits, so the
// identity holds to within a few units of rounding, never more.
TEST(C4RealTest, SineAndCosineAgreeOverAFullTurn)
{
	for (int deg = -360; deg <= 360; ++deg)
	{
		C4Real s = Sin(itofix(deg));
		C4Real c = Cos(itofix(deg));
		// sin^2 + cos^2 = 1, to within the table's resolution.
		int32_t sum = (s * s + c * c).val;
		EXPECT_NEAR(65536, sum, 8) << "at " << deg << " degrees";

		// Cos(x) == Sin(x + 90) is an identity of the table, not an
		// approximation, because both read the same entries.
		EXPECT_EQ(c.val, Sin(itofix(deg + 90)).val) << "at " << deg << " degrees";
	}
}

// Comparison is on the raw value, so ordering must survive the conversions.
TEST(C4RealTest, ComparisonFollowsValue)
{
	EXPECT_TRUE(itofix(1, 3) < itofix(1, 2));
	EXPECT_TRUE(-itofix(1) < itofix(0));
	EXPECT_TRUE(itofix(5) == itofix(5));
	EXPECT_TRUE(itofix(5) != itofix(1, 2));
	EXPECT_FALSE(!itofix(1));
	EXPECT_TRUE(!itofix(0));
}

}  // namespace
