//
// SystemWrapper.cpp
//
// $Id: //poco/1.4/JS/Core/src/SystemWrapper.cpp#5 $
//
// Library: JSCore
// Package: JSCore
// Module:  SystemWrapper
//
// Copyright (c) 2013-2014, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// SPDX-License-Identifier: Apache-2.0
//


#include "Poco/JS/Core/SystemWrapper.h"
#include "Poco/Environment.h"
#include "Poco/Thread.h"
#include "Poco/Process.h"
#include "Poco/Pipe.h"
#include "Poco/PipeStream.h"
#include "Poco/StreamCopier.h"


namespace Poco {
namespace JS {
namespace Core {


SystemWrapper::SystemWrapper()
{
}


SystemWrapper::~SystemWrapper()
{
}


v8::Handle<v8::ObjectTemplate> SystemWrapper::objectTemplate(v8::Isolate* pIsolate)
{
	v8::EscapableHandleScope handleScope(pIsolate);
	v8::Local<v8::ObjectTemplate> systemTemplate = v8::ObjectTemplate::New();
	systemTemplate->SetInternalFieldCount(1);
	systemTemplate->SetAccessor(v8::String::NewFromUtf8(pIsolate, "osName"), osName);
	systemTemplate->SetAccessor(v8::String::NewFromUtf8(pIsolate, "osDisplayName"), osDisplayName);
	systemTemplate->SetAccessor(v8::String::NewFromUtf8(pIsolate, "osArchitecture"), osArchitecture);
	systemTemplate->SetAccessor(v8::String::NewFromUtf8(pIsolate, "osVersion"), osVersion);
	systemTemplate->SetAccessor(v8::String::NewFromUtf8(pIsolate, "nodeName"), nodeName);
	systemTemplate->SetAccessor(v8::String::NewFromUtf8(pIsolate, "nodeId"), nodeId);
	systemTemplate->SetAccessor(v8::String::NewFromUtf8(pIsolate, "processorCount"), processorCount);
	systemTemplate->Set(v8::String::NewFromUtf8(pIsolate, "has"), v8::FunctionTemplate::New(pIsolate, has));
	systemTemplate->Set(v8::String::NewFromUtf8(pIsolate, "get"), v8::FunctionTemplate::New(pIsolate, get));
	systemTemplate->Set(v8::String::NewFromUtf8(pIsolate, "set"), v8::FunctionTemplate::New(pIsolate, set));
	systemTemplate->Set(v8::String::NewFromUtf8(pIsolate, "sleep"), v8::FunctionTemplate::New(pIsolate, sleep));
	systemTemplate->Set(v8::String::NewFromUtf8(pIsolate, "exec"), v8::FunctionTemplate::New(pIsolate, exec));
	return handleScope.Escape(systemTemplate);
}


void SystemWrapper::exec(const v8::FunctionCallbackInfo<v8::Value>& args)
{
	if (args.Length() < 1) return;
	std::string command = toString(args[0]);
	std::string output;
	try
	{
#ifdef _WIN32
		std::string shell("cmd.exe");
		std::string shellArg("/C");
#else
		std::string shell("/bin/sh");
		std::string shellArg("-c");
#endif
		Poco::Pipe outPipe;
		Poco::Process::Args shellArgs;
		shellArgs.push_back(shellArg);
		shellArgs.push_back(command);
		Poco::ProcessHandle ph(Poco::Process::launch(shell, shellArgs, 0, &outPipe, &outPipe));
		Poco::PipeInputStream istr(outPipe);
		Poco::StreamCopier::copyToString(istr, output);
		ph.wait();
		returnString(args, output);
	}
	catch (Poco::Exception& exc)
	{
		returnException(args, exc);
	}
}


void SystemWrapper::sleep(const v8::FunctionCallbackInfo<v8::Value>& args)
{
	if (args.Length() < 1) return;
	if (!args[0]->IsNumber()) return;
	double millisecs = args[0]->NumberValue();
	Poco::Thread::sleep(millisecs);
}


void SystemWrapper::get(const v8::FunctionCallbackInfo<v8::Value>& args)
{
	if (args.Length() < 1) return;
	std::string name = toString(args[0]);
	std::string deflt;
	if (args.Length() > 1) deflt = toString(args[1]);
	std::string value = Poco::Environment::get(name, deflt);
	returnString(args, value);
}


void SystemWrapper::has(const v8::FunctionCallbackInfo<v8::Value>& args)
{
	if (args.Length() < 1) return;
	std::string name = toString(args[0]);
	bool result = Poco::Environment::has(name);
	args.GetReturnValue().Set(result);
}


void SystemWrapper::set(const v8::FunctionCallbackInfo<v8::Value>& args)
{
	if (args.Length() < 2) return;
	std::string name = toString(args[0]);
	std::string value = toString(args[1]);
	Poco::Environment::set(name, value);
}


void SystemWrapper::osName(v8::Local<v8::String> name, const v8::PropertyCallbackInfo<v8::Value>& info)
{
	returnString(info, Poco::Environment::osName());
}


void SystemWrapper::osDisplayName(v8::Local<v8::String> name, const v8::PropertyCallbackInfo<v8::Value>& info)
{
	returnString(info, Poco::Environment::osDisplayName());
}


void SystemWrapper::osVersion(v8::Local<v8::String> name, const v8::PropertyCallbackInfo<v8::Value>& info)
{
	returnString(info, Poco::Environment::osVersion());
}


void SystemWrapper::osArchitecture(v8::Local<v8::String> name, const v8::PropertyCallbackInfo<v8::Value>& info)
{
	returnString(info, Poco::Environment::osArchitecture());
}


void SystemWrapper::nodeName(v8::Local<v8::String> name, const v8::PropertyCallbackInfo<v8::Value>& info)
{
	returnString(info, Poco::Environment::nodeName());
}


void SystemWrapper::nodeId(v8::Local<v8::String> name, const v8::PropertyCallbackInfo<v8::Value>& info)
{
	returnString(info, Poco::Environment::nodeId());
}


void SystemWrapper::processorCount(v8::Local<v8::String> name, const v8::PropertyCallbackInfo<v8::Value>& info)
{
	info.GetReturnValue().Set(Poco::Environment::processorCount());
}


} } } // namespace Poco::JS::Core
