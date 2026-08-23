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

/* Distance(), StrToI32() and the wraparound helper they use.
 *
 * Both functions used to rely on signed overflow: Distance() squared two 64-bit
 * differences and tested the result for having gone negative, and StrToI32()
 * accumulated a literal that need not fit. The behaviour these tests pin is the
 * behaviour those versions had in practice, so the same answers keep coming out
 * of code that no longer asks the compiler for a favour -- with one deliberate
 * exception, marked below, where the old answer was not a distance at all. */

#include <C4Include.h>
#include "lib/Standard.h"

#include <gtest/gtest.h>

TEST(StandardTest, DistanceIsTheEuclideanDistance)
{
	EXPECT_EQ(0, Distance(0, 0, 0, 0));
	EXPECT_EQ(5, Distance(0, 0, 3, 4));
	EXPECT_EQ(5, Distance(3, 4, 0, 0));
	EXPECT_EQ(5, Distance(-3, -4, 0, 0));
	EXPECT_EQ(100, Distance(1000, 500, 1100, 500));
	// Truncated rather than rounded: the ++/-- pair after the sqrt corrects a
	// floating-point result that landed on the wrong side, it does not round.
	EXPECT_EQ(141, Distance(0, 0, 100, 100)); // sqrt(2) * 100 = 141.42
	EXPECT_EQ(1, Distance(0, 0, 1, 1));       // sqrt(2)       =   1.41
}

TEST(StandardTest, DistanceRejectsWhatItCannotReturn)
{
	// -1 has always been this function's answer for a distance it cannot
	// express; it used to arrive there by testing a signed overflow after the
	// fact, which is undefined and is the check a compiler may delete.
	//
	// The largest representable distance is INT32_MAX, so a separation beyond
	// that is refused on either axis...
	EXPECT_EQ(-1, Distance(0, 0, INT32_MAX, INT32_MAX));
	EXPECT_EQ(-1, Distance(INT32_MIN, 0, INT32_MAX, 0));
	EXPECT_EQ(-1, Distance(0, INT32_MIN, 0, INT32_MAX));
	// ...while everything that does fit still comes back.
	EXPECT_EQ(INT32_MAX, Distance(0, 0, INT32_MAX, 0));
	EXPECT_EQ(INT32_MAX, Distance(0, 0, 0, INT32_MAX));
}

TEST(StandardTest, StrToI32ParsesAndReportsWhereItStopped)
{
	const char *end = nullptr;
	EXPECT_EQ(0, StrToI32("0", 10, &end));
	EXPECT_EQ(42, StrToI32("42", 10, &end));
	EXPECT_EQ(42, StrToI32("+42", 10, &end));
	EXPECT_EQ(-42, StrToI32("-42", 10, &end));
	EXPECT_EQ(255, StrToI32("ff", 16, &end));
	EXPECT_EQ(255, StrToI32("FF", 16, &end));
	EXPECT_EQ(5, StrToI32("101", 2, &end));

	const char *text = "123abc";
	EXPECT_EQ(123, StrToI32(text, 10, &end));
	EXPECT_EQ(text + 3, end); // stops at the first character it cannot use

	// Nothing to parse: the scan end goes back to the start, so a caller can
	// tell "no digits" from "a zero".
	const char *nothing = "-";
	EXPECT_EQ(0, StrToI32(nothing, 10, &end));
	EXPECT_EQ(nothing, end);
}

TEST(StandardTest, StrToI32TruncatesALiteralThatDoesNotFit)
{
	// Truncation to 32 bits is what this has always done. It is pinned here
	// because it is now produced deliberately, in unsigned arithmetic, rather
	// than by signed overflow -- and because content in the wild may depend on
	// the answer.
	const char *end = nullptr;
	EXPECT_EQ(INT32_MAX, StrToI32("2147483647", 10, &end));
	EXPECT_EQ(INT32_MIN, StrToI32("2147483648", 10, &end));
	EXPECT_EQ(INT32_MIN, StrToI32("-2147483648", 10, &end));
	EXPECT_EQ(INT32_MAX, StrToI32("-2147483649", 10, &end));
	EXPECT_EQ(-1, StrToI32("4294967295", 10, &end));
	EXPECT_EQ(0, StrToI32("4294967296", 10, &end));
	EXPECT_EQ(-1, StrToI32("ffffffff", 16, &end));
}

TEST(StandardTest, WrapToInt32IsTwosComplement)
{
	EXPECT_EQ(0, WrapToInt32(0u));
	EXPECT_EQ(1, WrapToInt32(1u));
	EXPECT_EQ(INT32_MAX, WrapToInt32(uint32_t(INT32_MAX)));
	EXPECT_EQ(INT32_MIN, WrapToInt32(uint32_t(INT32_MAX) + 1u));
	EXPECT_EQ(-1, WrapToInt32(0xffffffffu));
	// The property the script engine needs: it is the inverse of the cast the
	// other way, for every value.
	for (int32_t v : {INT32_MIN, INT32_MIN + 1, -1, 0, 1, INT32_MAX - 1, INT32_MAX})
		EXPECT_EQ(v, WrapToInt32(uint32_t(v)));
}
