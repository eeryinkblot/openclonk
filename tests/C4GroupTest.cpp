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

/* Round-trip tests for the group archive format.
 *
 * C4Group is what every build packs game data with and what the engine reads
 * all of its content through, and nothing tested it before this file. The
 * cases below stay on the paths `cmake --build . --target groups` takes: add a
 * file, add a directory (which packs it as a child group), pack a directory
 * tree whole, and unpack it again.
 *
 * They exercise those paths rather than assert on their internals, so they
 * would not by themselves have caught the two defects the class has had --
 * both were silent, one of them undefined behaviour that keeps working by
 * accident. Run them under a sanitizer for that. */

#include <C4Include.h>
#include "c4group/C4Group.h"

#include "c4group/CStdFile.h"
#include "platform/StdFile.h"

#include <gtest/gtest.h>

#include <string>
#include <vector>

namespace
{
	void WriteFile(const std::string &path, const std::string &contents)
	{
		CStdFile file;
		ASSERT_TRUE(file.Create(path.c_str())) << "could not create " << path;
		ASSERT_TRUE(file.Write(contents.data(), contents.size()));
		ASSERT_TRUE(file.Close());
	}

	void ExpectEntry(C4Group &group, const char *entry_name, const std::string &contents)
	{
		StdStrBuf buffer;
		ASSERT_TRUE(group.LoadEntryString(entry_name, &buffer))
			<< entry_name << ": " << group.GetError();
		EXPECT_EQ(contents, std::string(buffer.getData(), buffer.getLength()));
	}
}

// Every test gets its own directory below the working directory, named after
// itself, and takes it with it. Nothing here writes outside that directory.
class C4GroupTest : public ::testing::Test
{
protected:
	void SetUp() override
	{
		dir = std::string("C4GroupTest.")
			+ ::testing::UnitTest::GetInstance()->current_test_info()->name();
		if (DirectoryExists(dir.c_str()))
		{
			EraseDirectory(dir.c_str());
		}
		ASSERT_TRUE(CreatePath(dir)) << "could not create " << dir;
	}

	void TearDown() override
	{
		// The sort list is a global that only one test sets, and it would
		// otherwise change what the next one packs.
		C4Group_SetSortList(nullptr);
		EraseDirectory(dir.c_str());
	}

	std::string Path(const char *name) const { return dir + DirSep + name; }

	std::string dir;
};

TEST_F(C4GroupTest, AddedFileRoundTrips)
{
	WriteFile(Path("Names.txt"), "Clonk\n");

	const std::string group_name = Path("Test.ocg");
	{
		C4Group group;
		ASSERT_TRUE(group.Open(group_name.c_str(), true)) << group.GetError();
		ASSERT_TRUE(group.Add(Path("Names.txt").c_str(), "Names.txt")) << group.GetError();
		ASSERT_TRUE(group.Close()) << group.GetError();
	}

	ASSERT_TRUE(FileExists(group_name.c_str()));
	EXPECT_TRUE(C4Group_IsGroup(group_name.c_str()));

	C4Group group;
	ASSERT_TRUE(group.Open(group_name.c_str())) << group.GetError();
	EXPECT_EQ(1, group.EntryCount());
	ExpectEntry(group, "Names.txt", "Clonk\n");
	EXPECT_TRUE(group.Close());
}

// The directory branch of C4Group::AddEntryOnDisk: the source is copied to a
// temporary name, packed, and added as a child group. This is where every
// nested .ocd inside a group comes from.
TEST_F(C4GroupTest, AddedDirectoryBecomesChildGroup)
{
	const std::string subdir = Path("Rock.ocd");
	ASSERT_TRUE(CreatePath(subdir));
	WriteFile(subdir + DirSep "DefCore.txt", "[DefCore]\nid=Rock\n");

	const std::string group_name = Path("Objects.ocd");
	{
		C4Group group;
		ASSERT_TRUE(group.Open(group_name.c_str(), true)) << group.GetError();
		ASSERT_TRUE(group.Add(subdir.c_str(), "Rock.ocd")) << group.GetError();
		ASSERT_TRUE(group.Close()) << group.GetError();
	}

	// Added, not moved: the directory is still there, and the temporary copy of
	// it is not.
	EXPECT_TRUE(DirectoryExists(subdir.c_str()));
	EXPECT_FALSE(FileExists((subdir + ".000").c_str()));

	C4Group group;
	ASSERT_TRUE(group.Open(group_name.c_str())) << group.GetError();
	EXPECT_EQ(1, group.EntryCount());

	C4Group child;
	ASSERT_TRUE(child.OpenAsChild(&group, "Rock.ocd")) << child.GetError();
	ExpectEntry(child, "DefCore.txt", "[DefCore]\nid=Rock\n");
	EXPECT_TRUE(child.Close());
	EXPECT_TRUE(group.Close());
}

// What the groups target does to planet/: pack a tree in place, then unpack it
// again and expect the files back.
TEST_F(C4GroupTest, PackAndUnpackDirectoryRoundTrip)
{
	const std::string tree = Path("System.ocg");
	ASSERT_TRUE(CreatePath(tree));
	WriteFile(tree + DirSep "Object.c", "func Foo() { return 42; }\n");
	ASSERT_TRUE(CreatePath(tree + DirSep "Sub.ocd"));
	WriteFile(tree + DirSep "Sub.ocd" DirSep "Script.c", "func Bar() {}\n");

	ASSERT_TRUE(C4Group_PackDirectory(tree.c_str()));
	EXPECT_FALSE(DirectoryExists(tree.c_str()));
	ASSERT_TRUE(FileExists(tree.c_str()));
	EXPECT_TRUE(C4Group_IsGroup(tree.c_str()));
	// The bug that shipped empty archives for a year produced a group file of
	// zero length and reported success, so size is worth an assertion of its own.
	EXPECT_GT(FileSize(tree.c_str()), 0u);

	ASSERT_TRUE(C4Group_UnpackDirectory(tree.c_str()));
	ASSERT_TRUE(DirectoryExists(tree.c_str()));

	StdStrBuf buffer;
	C4Group group;
	ASSERT_TRUE(group.Open(tree.c_str())) << group.GetError();
	ExpectEntry(group, "Object.c", "func Foo() { return 42; }\n");
	// Unpacking is one level deep: the nested group stays packed.
	C4Group child;
	ASSERT_TRUE(child.OpenAsChild(&group, "Sub.ocd")) << child.GetError();
	ExpectEntry(child, "Script.c", "func Bar() {}\n");
	EXPECT_TRUE(child.Close());
	EXPECT_TRUE(group.Close());
}

// With a sort list set -- which is what the engine and c4group do, and nothing
// else does -- entries come out in the order the list prescribes rather than
// the order the directory iterator handed them over in. Adding a directory
// under a different name also takes the resort branch of AppendEntry2StdFile,
// the one that copies the child group aside first.
TEST_F(C4GroupTest, SortsEntriesByTheSortList)
{
	const std::string subdir = Path("Rock.ocd");
	ASSERT_TRUE(CreatePath(subdir));
	WriteFile(subdir + DirSep "Script.c", "func Foo() {}\n");
	WriteFile(subdir + DirSep "DefCore.txt", "[DefCore]\nid=Rock\n");
	WriteFile(subdir + DirSep "Graphics.png", "not really a png");

	C4Group_SetSortList(C4CFN_FLS);

	const std::string group_name = Path("Objects.ocd");
	{
		C4Group group;
		ASSERT_TRUE(group.Open(group_name.c_str(), true)) << group.GetError();
		ASSERT_TRUE(group.Add(subdir.c_str(), "Rock.ocd")) << group.GetError();
		ASSERT_TRUE(group.Close()) << group.GetError();
	}

	C4Group group;
	ASSERT_TRUE(group.Open(group_name.c_str())) << group.GetError();
	C4Group child;
	ASSERT_TRUE(child.OpenAsChild(&group, "Rock.ocd")) << child.GetError();

	// C4FLS_Def puts graphics first and the script last; alphabetical order
	// would have started with DefCore.txt.
	std::vector<std::string> names;
	for (const C4GroupEntry *entry = child.GetFirstEntry(); entry; entry = entry->Next)
	{
		names.emplace_back(entry->FileName);
	}
	EXPECT_EQ((std::vector<std::string>{"Graphics.png", "DefCore.txt", "Script.c"}), names);

	EXPECT_TRUE(child.Close());
	EXPECT_TRUE(group.Close());
}

// Reading a group that is not one has to fail rather than be believed.
TEST_F(C4GroupTest, RejectsAFileThatIsNotAGroup)
{
	const std::string not_a_group = Path("Broken.ocg");
	WriteFile(not_a_group, "this is not a group file");

	EXPECT_FALSE(C4Group_IsGroup(not_a_group.c_str()));

	C4Group group;
	EXPECT_FALSE(group.Open(not_a_group.c_str()));
	EXPECT_STRNE("", group.GetError());
}
