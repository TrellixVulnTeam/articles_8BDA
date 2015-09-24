//
// JSExecutor.cpp
//
// $Id: //poco/1.4/JS/Core/src/JSExecutor.cpp#7 $
//
// Library: JSCore
// Package: JSCore
// Module:  JSExecutor
//
// Copyright (c) 2013-2014, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// SPDX-License-Identifier: Apache-2.0
//


#include "Poco/JS/Core/JSExecutor.h"
#include "Poco/JS/Core/SystemWrapper.h"
#include "Poco/JS/Core/DateTimeWrapper.h"
#include "Poco/JS/Core/LocalDateTimeWrapper.h"
#include "Poco/JS/Core/ApplicationWrapper.h"
#include "Poco/JS/Core/URIWrapper.h"
#include "Poco/JS/Core/TimerWrapper.h"
#include "Poco/Delegate.h"
#include "Poco/URIStreamOpener.h"
#include "Poco/StreamCopier.h"
#include <memory>


namespace Poco {
namespace JS {
namespace Core {


//
// JSExecutor
//


Poco::ThreadLocal<JSExecutor*> JSExecutor::_pCurrentExecutor;


JSExecutor::JSExecutor(const std::string& source, const Poco::URI& sourceURI, Poco::UInt64 memoryLimit):
	_source(source),
	_sourceURI(sourceURI),
	_memoryLimit(memoryLimit)
{
	_importStack.push_back(sourceURI);
}


JSExecutor::~JSExecutor()
{
	_script.Reset();
	_scriptContext.Reset();
	_globalContext.Reset();
	_globalObject.Reset();
	
	WeakPersistentWrapperRegistry::cleanupIsolate(_pooledIso.isolate());
}


JSExecutor::Ptr JSExecutor::current()
{
	return Ptr(_pCurrentExecutor.get(), true);
}


void JSExecutor::stop()
{
}


void JSExecutor::setup()
{
	v8::Isolate* pIsolate = _pooledIso.isolate();
	v8::HandleScope handleScope(pIsolate);

	v8::ResourceConstraints resourceConstraints;
	resourceConstraints.ConfigureDefaults(_memoryLimit, 1);
	if (!v8::SetResourceConstraints(pIsolate, &resourceConstraints))
	{
		throw Poco::SystemException("cannot set resource constraints");
	}

	v8::Local<v8::Context> globalContext = v8::Context::New(pIsolate);
	v8::Context::Scope globalContextScope(globalContext);
	_globalContext.Reset(pIsolate, globalContext);
	
	v8::Local<v8::ObjectTemplate> globalObject = v8::ObjectTemplate::New(pIsolate);
	_globalObject.Reset(pIsolate, globalObject);
	registerGlobals(globalObject, pIsolate);

	v8::Local<v8::Context> scriptContext = v8::Context::New(pIsolate, 0, globalObject);
	_scriptContext.Reset(pIsolate, scriptContext);
}


void JSExecutor::compile()
{
	v8::Isolate* pIsolate = _pooledIso.isolate();
	v8::HandleScope handleScope(pIsolate);

	v8::Local<v8::String> sourceURI = v8::String::NewFromUtf8(pIsolate, _sourceURI.toString().c_str());
	v8::Local<v8::String> source = v8::String::NewFromUtf8(pIsolate, _source.c_str());
	v8::TryCatch tryCatch;
	v8::ScriptOrigin scriptOrigin(sourceURI);
	v8::Local<v8::Script> script = v8::Script::Compile(source, &scriptOrigin);
	if (script.IsEmpty() || tryCatch.HasCaught())
	{
		reportError(tryCatch);
	}
	else
	{
		_script.Reset(pIsolate, script);
	}
}


void JSExecutor::runImpl()
{
	*_pCurrentExecutor = this;
	
	v8::Isolate* pIsolate = _pooledIso.isolate();
	v8::Isolate::Scope isoScope(pIsolate);
	v8::HandleScope handleScope(pIsolate);

	bool mustUpdateGlobals = true;
	if (_globalObject.IsEmpty())
	{
		setup();
		mustUpdateGlobals = false;
	}

	v8::Local<v8::Context> globalContext = v8::Local<v8::Context>::New(pIsolate, _globalContext);
	v8::Context::Scope globalContextScope(globalContext);
	
	v8::Local<v8::ObjectTemplate> global(v8::Local<v8::ObjectTemplate>::New(pIsolate, _globalObject));
	if (mustUpdateGlobals)
	{
		updateGlobals(global, pIsolate);
	}
	v8::Local<v8::Context> scriptContext = v8::Local<v8::Context>::New(pIsolate, _scriptContext);
	v8::Context::Scope contextScope(scriptContext);

	if (_script.IsEmpty())
	{
		compile();
	}

	if (!_script.IsEmpty())
	{
		v8::TryCatch tryCatch;
		v8::Local<v8::Script> script(v8::Local<v8::Script>::New(pIsolate, _script));
		v8::Local<v8::Value> result = script->Run();
		if (result.IsEmpty() || tryCatch.HasCaught())
		{
			reportError(tryCatch);
		}
	}
	
	scriptCompleted();
}


void JSExecutor::run()
{
	runImpl();
}


void JSExecutor::call(v8::Handle<v8::Function>& function, v8::Handle<v8::Value>& receiver, int argc, v8::Handle<v8::Value> argv[])
{
	v8::Isolate* pIsolate = _pooledIso.isolate();
	v8::Isolate::Scope isoScope(pIsolate);
	v8::HandleScope handleScope(pIsolate);

	v8::Local<v8::Context> context(v8::Local<v8::Context>::New(pIsolate, _scriptContext));
	v8::Context::Scope contextScope(context);

	callInContext(function, receiver, argc, argv);
}


void JSExecutor::callInContext(v8::Handle<v8::Function>& function, v8::Handle<v8::Value>& receiver, int argc, v8::Handle<v8::Value> argv[])
{
	v8::TryCatch tryCatch;
	function->Call(receiver, argc, argv);
	if (tryCatch.HasCaught())
	{
		reportError(tryCatch);
	}
}


void JSExecutor::call(v8::Persistent<v8::Object>& jsObject, const std::string& method, const std::string& args)
{
	v8::Isolate* pIsolate = _pooledIso.isolate();
	v8::Isolate::Scope isoScope(pIsolate);
	v8::HandleScope handleScope(pIsolate);

	v8::Local<v8::Context> context(v8::Local<v8::Context>::New(pIsolate, _scriptContext));
	v8::Context::Scope contextScope(context);

	v8::Local<v8::String> jsMethod = v8::String::NewFromUtf8(pIsolate, method.c_str());
	v8::Local<v8::String> jsArgs = v8::String::NewFromUtf8(pIsolate, args.c_str());

	v8::Local<v8::Object> localObject(v8::Local<v8::Object>::New(pIsolate, jsObject));

	if (localObject->Has(jsMethod))
	{
		v8::Local<v8::Value> jsValue = localObject->Get(jsMethod);
		if (jsValue->IsFunction())
		{
			v8::Local<v8::Function> jsFunction = v8::Local<v8::Function>::Cast(jsValue);
		
			v8::TryCatch tryCatch;
			v8::Local<v8::Value> argv[1];
			argv[0] = v8::JSON::Parse(jsArgs);
			jsFunction->Call(localObject, 1, argv);
			if (tryCatch.HasCaught())
			{
				reportError(tryCatch);
			}
		}
	}
}


void JSExecutor::call(v8::Persistent<v8::Function>& function)
{
	v8::Isolate* pIsolate = _pooledIso.isolate();
	v8::Isolate::Scope isoScope(pIsolate);
	v8::HandleScope handleScope(pIsolate);

	v8::Local<v8::Context> context(v8::Local<v8::Context>::New(pIsolate, _scriptContext));
	v8::Context::Scope contextScope(context);

	v8::Local<v8::Object> global = context->Global();

	v8::Local<v8::Function> localFunction(v8::Local<v8::Function>::New(pIsolate, function));
	v8::Local<v8::Value> args[1];
	v8::TryCatch tryCatch;
	localFunction->Call(global, 0, args);
	if (tryCatch.HasCaught())
	{
		reportError(tryCatch);
	}
}


void JSExecutor::includeScript(const std::string& uri)
{
	v8::Isolate* pIsolate = _pooledIso.isolate();
	v8::HandleScope handleScope(pIsolate);

	v8::Local<v8::Context> context(v8::Local<v8::Context>::New(pIsolate, _scriptContext));
	v8::Context::Scope contextScope(context);

	Poco::URI includeURI(_sourceURI, uri);
	std::auto_ptr<std::istream> istr(Poco::URIStreamOpener::defaultOpener().open(includeURI));
	std::string source;
	Poco::StreamCopier::copyToString(*istr, source);

	v8::Local<v8::String> sourceURI = v8::String::NewFromUtf8(pIsolate, includeURI.toString().c_str());
	v8::Local<v8::String> sourceObject = v8::String::NewFromUtf8(pIsolate, source.c_str());
	v8::TryCatch tryCatch;
	v8::ScriptOrigin scriptOrigin(sourceURI);
	v8::Local<v8::Script> scriptObject = v8::Script::Compile(sourceObject, &scriptOrigin);
	if (scriptObject.IsEmpty() || tryCatch.HasCaught())
	{
		reportError(tryCatch);
	}
	else
	{
		v8::Local<v8::Value> result = scriptObject->Run();
		if (result.IsEmpty() || tryCatch.HasCaught())
		{
			reportError(tryCatch);
		}
	}
}


void JSExecutor::registerGlobals(v8::Local<v8::ObjectTemplate>& global, v8::Isolate* pIsolate)
{
	Poco::JS::Core::DateTimeWrapper dateTimeWrapper;
	global->Set(v8::String::NewFromUtf8(pIsolate, "DateTime"), dateTimeWrapper.constructor(pIsolate));

	Poco::JS::Core::LocalDateTimeWrapper localDateTimeWrapper;
	global->Set(v8::String::NewFromUtf8(pIsolate, "LocalDateTime"), localDateTimeWrapper.constructor(pIsolate));

	Poco::JS::Core::SystemWrapper systemWrapper;
	v8::Local<v8::Object> systemObject = systemWrapper.wrapNative(pIsolate);
	global->Set(v8::String::NewFromUtf8(pIsolate, "system"), systemObject);

	Poco::JS::Core::ApplicationWrapper applicationWrapper;
	v8::Local<v8::Object> applicationObject = applicationWrapper.wrapNative(pIsolate);
	global->Set(v8::String::NewFromUtf8(pIsolate, "application"), applicationObject);

	Poco::JS::Core::URIWrapper uriWrapper;
	v8::Local<v8::Object> uriObject = uriWrapper.wrapNative(pIsolate);
	global->Set(v8::String::NewFromUtf8(pIsolate, "uri"), uriObject);

	global->Set(v8::String::NewFromUtf8(pIsolate, "include"), v8::FunctionTemplate::New(pIsolate, include));
	global->Set(v8::String::NewFromUtf8(pIsolate, "require"), v8::FunctionTemplate::New(pIsolate, require));
	
	v8::Local<v8::Object> moduleObject = v8::Object::New(pIsolate);
	moduleObject->Set(v8::String::NewFromUtf8(pIsolate, "id"), v8::String::NewFromUtf8(pIsolate, _sourceURI.toString().c_str()));
	v8::Local<v8::Object> importsObject = v8::Object::New(pIsolate);
	moduleObject->Set(v8::String::NewFromUtf8(pIsolate, "imports"), importsObject);
	v8::Local<v8::Object> exportsObject = v8::Object::New(pIsolate);
	moduleObject->Set(v8::String::NewFromUtf8(pIsolate, "exports"), exportsObject);
	global->Set(v8::String::NewFromUtf8(pIsolate, "module"), moduleObject);
}


void JSExecutor::updateGlobals(v8::Local<v8::ObjectTemplate>& global, v8::Isolate* pIsolate)
{
}


void JSExecutor::handleError(const ErrorInfo& errorInfo)
{
}


void JSExecutor::reportError(v8::TryCatch& tryCatch)
{
	ErrorInfo errorInfo;
	errorInfo.uri = _sourceURI.toString();
	errorInfo.lineNo = 0;
	v8::Local<v8::Value> exception = tryCatch.Exception();
	if (!exception.IsEmpty())
	{
		v8::String::Utf8Value str(exception);
		errorInfo.message = *str;
	}
	v8::Local<v8::Message> message = tryCatch.Message();
	if (!message.IsEmpty())
	{
		v8::String::Utf8Value str(message->GetScriptResourceName());
		errorInfo.uri = *str;
		errorInfo.lineNo = message->GetLineNumber();
	}
	reportError(errorInfo);
}


void JSExecutor::reportError(const ErrorInfo& errorInfo)
{
	try
	{
		scriptError(this, errorInfo);
	}
	catch (...)
	{
	}
	handleError(errorInfo);
}


void JSExecutor::scriptCompleted()
{
}


void JSExecutor::include(const v8::FunctionCallbackInfo<v8::Value>& args)
{
	v8::EscapableHandleScope handleScope(args.GetIsolate());

	if (args.Length() != 1) return;
	std::string uri(Poco::JS::Core::Wrapper::toString(args[0]));
	
	JSExecutor* pCurrentExecutor = _pCurrentExecutor.get();
	if (!pCurrentExecutor) return;

	try
	{
		pCurrentExecutor->includeScript(uri);
	}
	catch (Poco::Exception& exc)
	{
		Poco::JS::Core::Wrapper::returnException(args, exc);
	}
}


void JSExecutor::require(const v8::FunctionCallbackInfo<v8::Value>& args)
{
	v8::EscapableHandleScope handleScope(args.GetIsolate());

	if (args.Length() != 1) return;
	std::string uri(Poco::JS::Core::Wrapper::toString(args[0]));
	
	JSExecutor* pCurrentExecutor = _pCurrentExecutor.get();
	if (!pCurrentExecutor) return;

	try
	{
		pCurrentExecutor->importModule(args, uri);
	}
	catch (Poco::Exception& exc)
	{
		Poco::JS::Core::Wrapper::returnException(args, exc);
	}
}


void JSExecutor::importModule(const v8::FunctionCallbackInfo<v8::Value>& args, const std::string& uri)
{
	class ImportScope
	{
	public:
		ImportScope(std::vector<Poco::URI>& stack, const Poco::URI& uri):
			_stack(stack)
		{
			_stack.push_back(uri);
		}
		
		~ImportScope()
		{
			_stack.pop_back();
		}
		
	private:
		std::vector<Poco::URI>& _stack;
	};

	// Resolve URI
	poco_assert_dbg (!_importStack.empty());
	Poco::URI moduleURI = _importStack.back();
	moduleURI.resolve(uri);
	ImportScope importScope(_importStack, moduleURI);
	std::string moduleURIString = moduleURI.toString();

	// Set up import context	
	v8::Isolate* pIsolate = _pooledIso.isolate();
	v8::EscapableHandleScope handleScope(pIsolate);

	v8::Local<v8::Context> scriptContext(v8::Local<v8::Context>::New(pIsolate, _scriptContext));
	v8::Context::Scope scriptContextScope(scriptContext);
	
	// Get global/root module and imports
	v8::Local<v8::Object> global = scriptContext->Global();
	v8::Local<v8::Object> globalModule = v8::Local<v8::Object>::Cast(global->Get(v8::String::NewFromUtf8(pIsolate, "module")));
	poco_assert_dbg (globalModule->IsObject());
	v8::Local<v8::Object> globalImports = v8::Local<v8::Object>::Cast(globalModule->Get(v8::String::NewFromUtf8(pIsolate, "imports")));
	poco_assert_dbg (globalImports->IsObject());

	// Check if we have already imported the module
	v8::Local<v8::String> jsModuleURI = v8::String::NewFromUtf8(pIsolate, moduleURIString.c_str());
	if (globalImports->Has(jsModuleURI))
	{
		args.GetReturnValue().Set(globalImports->Get(jsModuleURI));
	}
	else
	{
		// Create context for import
		v8::Local<v8::ObjectTemplate> moduleTemplate(v8::Local<v8::ObjectTemplate>::New(pIsolate, _globalObject));
		updateGlobals(moduleTemplate, pIsolate);

		v8::Local<v8::Object> moduleObject = v8::Object::New(pIsolate);
		moduleObject->Set(v8::String::NewFromUtf8(pIsolate, "id"), jsModuleURI);
		v8::Local<v8::Object> exportsObject = v8::Object::New(pIsolate);
		moduleObject->Set(v8::String::NewFromUtf8(pIsolate, "exports"), exportsObject);
		moduleTemplate->Set(v8::String::NewFromUtf8(pIsolate, "module"), moduleObject);
		moduleTemplate->Set(v8::String::NewFromUtf8(pIsolate, "exports"), exportsObject);
		globalImports->Set(jsModuleURI, exportsObject);

		v8::Local<v8::Context> moduleContext = v8::Context::New(pIsolate, 0, moduleTemplate);
		v8::Context::Scope moduleContextScope(moduleContext);
	
		std::string source;
		std::auto_ptr<std::istream> istr(Poco::URIStreamOpener::defaultOpener().open(moduleURIString));
		Poco::StreamCopier::copyToString(*istr, source);

		v8::Local<v8::String> sourceObject = v8::String::NewFromUtf8(pIsolate, source.c_str());
		v8::TryCatch tryCatch;
		v8::ScriptOrigin scriptOrigin(jsModuleURI);
		v8::Local<v8::Script> scriptObject = v8::Script::Compile(sourceObject, &scriptOrigin);
		if (scriptObject.IsEmpty() || tryCatch.HasCaught())
		{
			args.GetReturnValue().Set(tryCatch.ReThrow());
		}
		else
		{
			v8::Local<v8::Value> result = scriptObject->Run();
			if (result.IsEmpty() || tryCatch.HasCaught())
			{
				args.GetReturnValue().Set(tryCatch.ReThrow());
			}
			else
			{
				// Note: we cannot use the exports handle from above as the script may 
				// have assigned a new object to module.exports.
				args.GetReturnValue().Set(moduleObject->Get(v8::String::NewFromUtf8(pIsolate, "exports")));
			}
		}
	}
}


//
// RunScriptTask
//


class RunScriptTask: public Poco::Util::TimerTask
{
public:
	RunScriptTask(TimedJSExecutor* pExecutor):
		_pExecutor(pExecutor, true)
	{
	}
	
	void run()
	{
		_pExecutor->runImpl();
	}
	
private:
	TimedJSExecutor::Ptr _pExecutor;
};


//
// CallFunctionTask
//


class CallFunctionTask: public Poco::Util::TimerTask
{
public:
	typedef Poco::AutoPtr<CallFunctionTask> Ptr;

	CallFunctionTask(v8::Isolate* pIsolate, TimedJSExecutor* pExecutor, v8::Handle<v8::Function> function):
		_pExecutor(pExecutor, true),
		_function(pIsolate, function)
	{
		_pExecutor->stopped += Poco::delegate(this, &CallFunctionTask::onExecutorStopped);
	}
	
	~CallFunctionTask()
	{
		try
		{
			if (_pExecutor)
			{
				_pExecutor->stopped -= Poco::delegate(this, &CallFunctionTask::onExecutorStopped);
			}
			_function.Reset();
		}
		catch (...)
		{
			poco_unexpected();
		}
	}
	
	void run()
	{
		TimedJSExecutor::Ptr pExecutor = _pExecutor;
		if (pExecutor)
		{
			pExecutor->call(_function);
		} 
	}
	
	void onExecutorStopped()
	{
		if (_pExecutor)
		{
			_pExecutor->stopped -= Poco::delegate(this, &CallFunctionTask::onExecutorStopped);
		}
		_pExecutor = 0;
	}
	
private:
	TimedJSExecutor::Ptr _pExecutor;
	v8::Persistent<v8::Function> _function;
};


//
// TimedJSExecutor
//


TimedJSExecutor::TimedJSExecutor(const std::string& source, const Poco::URI& sourceURI, Poco::UInt64 memoryLimit):
	JSExecutor(source, sourceURI, memoryLimit)
{
}


TimedJSExecutor::~TimedJSExecutor()
{
	try
	{
		_timer.cancel(true);
		stopped(this);
	}
	catch (...)
	{
		poco_unexpected();
	}
}


void TimedJSExecutor::run()
{
	_timer.schedule(new RunScriptTask(this), Poco::Timestamp());
}


void TimedJSExecutor::registerGlobals(v8::Local<v8::ObjectTemplate>& global, v8::Isolate* pIsolate)
{
	JSExecutor::registerGlobals(global, pIsolate);
	
	global->Set(v8::String::NewFromUtf8(pIsolate, "setTimeout"), v8::FunctionTemplate::New(pIsolate, setTimeout));
	global->Set(v8::String::NewFromUtf8(pIsolate, "setInterval"), v8::FunctionTemplate::New(pIsolate, setInterval));
}


void TimedJSExecutor::stop()
{
	_timer.cancel(true);
	stopped(this);
}


void TimedJSExecutor::setTimeout(const v8::FunctionCallbackInfo<v8::Value>& args)
{
	v8::EscapableHandleScope handleScope(args.GetIsolate());

	if (args.Length() != 2) return;
	if (!args[0]->IsFunction()) return;
	v8::Local<v8::Function> function = args[0].As<v8::Function>();
	if (!args[1]->IsNumber()) return;
	double millisecs = args[1]->NumberValue();
	
	JSExecutor* pCurrentExecutor = _pCurrentExecutor.get();
	if (!pCurrentExecutor) return;
	TimedJSExecutor* pThis = static_cast<TimedJSExecutor*>(pCurrentExecutor);
	
	CallFunctionTask::Ptr pTask = new CallFunctionTask(args.GetIsolate(), pThis, function);
	Poco::Timestamp ts;
	ts += millisecs*1000;
	pThis->_timer.schedule(pTask, ts);
	TimerWrapper wrapper;
	v8::Persistent<v8::Object> timerObject(args.GetIsolate(), wrapper.wrapNativePersistent(args.GetIsolate(), pTask));
	args.GetReturnValue().Set(timerObject);
}


void TimedJSExecutor::setInterval(const v8::FunctionCallbackInfo<v8::Value>& args)
{
	v8::EscapableHandleScope handleScope(args.GetIsolate());

	if (args.Length() != 2) return;
	if (!args[0]->IsFunction()) return;
	v8::Local<v8::Function> function = args[0].As<v8::Function>();
	if (!args[1]->IsNumber()) return;
	double millisecs = args[1]->NumberValue();
	
	JSExecutor* pCurrentExecutor = _pCurrentExecutor.get();
	if (!pCurrentExecutor) return;
	TimedJSExecutor* pThis = static_cast<TimedJSExecutor*>(pCurrentExecutor);
	
	CallFunctionTask::Ptr pTask = new CallFunctionTask(args.GetIsolate(), pThis, function);
	pThis->_timer.scheduleAtFixedRate(pTask, millisecs, millisecs);
	TimerWrapper wrapper;
	v8::Persistent<v8::Object> timerObject(args.GetIsolate(), wrapper.wrapNativePersistent(args.GetIsolate(), pTask));
	args.GetReturnValue().Set(timerObject);
}


} } } // namespace Poco::JS::Core
