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

#ifndef INC_ErrorHandler
#define INC_ErrorHandler

#include "script/C4Aul.h"
#include <gmock/gmock.h>

class ErrorHandler : public C4AulErrorHandler
{
public:
	ErrorHandler()
	{
		::ScriptEngine.RegisterErrorHandler(this);
	}
	MOCK_METHOD(void, OnError, (const char*), (override));
	MOCK_METHOD(void, OnWarning, (const char*), (override));
};

#endif
