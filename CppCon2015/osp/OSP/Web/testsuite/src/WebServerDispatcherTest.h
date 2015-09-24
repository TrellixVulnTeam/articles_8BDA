//
// WebServerDispatcherTest.h
//
// $Id: //poco/1.6/OSP/Web/testsuite/src/WebServerDispatcherTest.h#1 $
//
// Definition of the WebServerDispatcherTest class.
//
// Copyright (c) 2007-2014, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// SPDX-License-Identifier: Apache-2.0
//


#ifndef WebServerDispatcherTest_INCLUDED
#define WebServerDispatcherTest_INCLUDED


#include "Poco/OSP/Web/Web.h"
#include "CppUnit/TestCase.h"


class WebServerDispatcherTest: public CppUnit::TestCase
{
public:
	WebServerDispatcherTest(const std::string& name);
	~WebServerDispatcherTest();

	void testVirtualDirectoryRoot();
	void testVirtualDirectoryAllow();
	void testVirtualDirectoryFail();
	void testRemoveDir();

	void setUp();
	void tearDown();

	static CppUnit::Test* suite();
};


#endif // WebServerDispatcherTest_INCLUDED
