/*
 * OpenClonk, http://www.openclonk.org
 *
 * Copyright (c) 2019, The OpenClonk Team and contributors
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

#include <C4Include.h>
#include "platform/StdFile.h"

#include <gtest/gtest.h>

TEST(StdFileTest, IsWildcardStringTest)
{
	EXPECT_TRUE(IsWildcardString("ab*cde"));
	EXPECT_TRUE(IsWildcardString("abcd?e"));
	EXPECT_TRUE(IsWildcardString("[abc]de"));
	EXPECT_FALSE(IsWildcardString("foobar"));
}

// Disabled because it fails: MakeTempFilename() picks the lowest unused
// <name>.NNN and hands it back without creating anything, so two callers that
// have not created their file yet are given the same name. That is the race
// behind the parallel `groups` pack failures, and ADR-019 in
// fork-notes/decisions.md records why the repair is a decision of its own
// rather than a patch in passing -- nine call sites, one of them an overload
// that writes through a buffer sized to the current string.
//
// It is here so that a fix has something to turn green, and so the defect is
// stated as a contract rather than as prose. Enable it by dropping the
// DISABLED_ prefix.
TEST(StdFileTest, DISABLED_MakeTempFilenameClaimsTheNameItHandsOut)
{
	char first[_MAX_PATH_LEN];
	SCopy("C4MakeTempFilenameTest.tmp", first, _MAX_PATH);
	MakeTempFilename(first);

	char second[_MAX_PATH_LEN];
	SCopy("C4MakeTempFilenameTest.tmp", second, _MAX_PATH);
	MakeTempFilename(second);

	EXPECT_STRNE(first, second);

	// A version that claims the name has files to clean up; today's does not.
	EraseItem(first);
	EraseItem(second);
}

// Same contract for the StdStrBuf overload, which is the one ADR-019 calls out
// as unsafe to extend: it writes through a buffer sized to the current string.
TEST(StdFileTest, DISABLED_MakeTempFilenameClaimsTheNameItHandsOutStdStrBuf)
{
	// Copy(), not the const char* constructor: that one takes a reference to
	// the literal, and the overload writes through getMData() -- which asserts
	// in a debug build and segfaults on read-only memory in a release one.
	StdStrBuf first;
	first.Copy("C4MakeTempFilenameTest.tmp");
	MakeTempFilename(&first);

	StdStrBuf second;
	second.Copy("C4MakeTempFilenameTest.tmp");
	MakeTempFilename(&second);

	EXPECT_STRNE(first.getData(), second.getData());

	EraseItem(first.getData());
	EraseItem(second.getData());
}

TEST(StdFileTest, WildcardMatchTest)
{
	EXPECT_TRUE(WildcardMatch("abc*", "abcdefg"));
	EXPECT_FALSE(WildcardMatch("abc*", "Xabcdefg"));
	EXPECT_TRUE(WildcardMatch("a?c*g", "abcdefg"));
	EXPECT_TRUE(WildcardMatch("a[1-9]?", "a5b"));
	EXPECT_TRUE(WildcardMatch("a[abc][A-Z]", "acX"));
	EXPECT_TRUE(WildcardMatch("[[]", "["));
	EXPECT_TRUE(WildcardMatch("[[-]", "-"));
}
