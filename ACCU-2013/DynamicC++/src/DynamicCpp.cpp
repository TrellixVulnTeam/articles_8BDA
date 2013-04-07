//
// DynamicCpp.cpp
//
// This sample demonstrates and compares boost::any, boost::variant, 
// boost::type_erasure, folly::dynamic and Poco::Dynamic::Var classess.
//
// Copyright (c) 2013, Aleph ONE Software Engineering LLC and Contributors.
//
// Permission is hereby granted, free of charge, to any person or organization
// obtaining a copy of the software and accompanying documentation covered by
// this license (the "Software") to use, reproduce, display, distribute,
// execute, and transmit the Software, and to prepare derivative works of the
// Software, and to permit third-parties to whom the Software is furnished to
// do so, all subject to the following:
// 
// The copyright notices in the Software and this entire statement, including
// the above license grant, this restriction and the following disclaimer,
// must be included in all copies of the Software, in whole or in part, and
// all derivative works of the Software, unless such copies or derivative
// works are solely in the form of machine-executable object code generated by
// a source language processor.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
// SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
// FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
// ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
// DEALINGS IN THE SOFTWARE.
//


#include "DynamicCpp.h"
#include <stdlib.h>

#include "variant.h"
#include "TypeErasure.h"
#include "DynamicVar.h"
#ifndef _WIN32
#include "folly_dynamic.h"
#endif
#include "Poco/SharedPtr.h"

using Poco::Dynamic::Var;
using boost::variant;

typedef std::vector<std::string> StrVec;
typedef Poco::SharedPtr<std::vector<std::string>> StrVecPtr;

const StrVecPtr getVec()
{
	static const char num[] = "123456789";
	int len = 8;

	StrVec* strvec = new StrVec(100000);
	StrVec::iterator it = strvec->begin();
	StrVec::iterator end = strvec->end();
	for (; it != end; ++it)
	{
		for (int i = 0; i < 9; ++i)
			it->append(1, num[rand() % len]);
	}

	return strvec;
}


int main(int argc, char** argv)
{
	StrVecPtr strvec1 = getVec();

	//doDynamicVar(*strvec1);
	//doVariant(*strvec1);
	//doTypeErasure(*strvec1);
#ifndef _WIN32
	doFollyDynamic(*strvec1);
#endif

	typeErasureTutorial();

	return 0;
}