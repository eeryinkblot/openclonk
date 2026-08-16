/*
* OpenClonk, http://www.openclonk.org
*
* Copyright (c) 2016, The OpenClonk Team and contributors
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

#include <stdarg.h>
#include <gmock/gmock.h>

class TestLog
{
public:
	virtual ~TestLog();
	static void setHandler(TestLog *new_handler);

protected:
	virtual bool Log(const char *msg) = 0;
	virtual bool DebugLog(const char *msg) = 0;
	virtual bool LogFatal(const char *msg) = 0;
	virtual bool LogSilent(const char *msg) = 0;

	virtual bool LogF(const char *format, va_list args) = 0;
	virtual bool DebugLogF(const char *format, va_list args) = 0;
	virtual bool LogSilentF(const char *format, va_list args) = 0;

	TestLog();

private:
	static TestLog *handler;

	friend bool Log(const char*);
	friend bool DebugLog(const char*);
	friend bool LogFatal(const char*);
	friend bool LogSilent(const char*);
	friend bool LogF(const char*, ...);
	friend bool DebugLogF(const char*, ...);
	friend bool LogSilentF(const char*, ...);
};

class LogMock : public TestLog
{
public:
	MOCK_METHOD(bool, Log, (const char*), (override));
	MOCK_METHOD(bool, DebugLog, (const char*), (override));
	MOCK_METHOD(bool, LogFatal, (const char*), (override));
	MOCK_METHOD(bool, LogSilent, (const char*), (override));
	MOCK_METHOD(bool, LogF, (const char*, va_list), (override));
	MOCK_METHOD(bool, DebugLogF, (const char*, va_list), (override));
	MOCK_METHOD(bool, LogSilentF, (const char*, va_list), (override));
};
