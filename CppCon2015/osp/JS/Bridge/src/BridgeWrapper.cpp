//
// BridgeWrapper.cpp
//
// $Id: //poco/1.4/JS/Bridge/src/BridgeWrapper.cpp#9 $
//
// Library: JSBridge
// Package: Bridge
// Module:  BridgeWrapper
//
// Copyright (c) 2013-2014, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// SPDX-License-Identifier: Apache-2.0
//


#include "Poco/JS/Bridge/BridgeWrapper.h"
#include "Poco/JS/Bridge/Listener.h"
#include "Poco/JS/Bridge/Serializer.h"
#include "Poco/JS/Bridge/Deserializer.h"
#include "Poco/JS/Bridge/JSONEventSerializer.h"
#include "Poco/JS/Core/PooledIsolate.h"
#include "Poco/RemotingNG/ServerTransport.h"
#include "Poco/RemotingNG/Transport.h"
#include "Poco/RemotingNG/TransportFactory.h"
#include "Poco/RemotingNG/TransportFactoryManager.h"
#include "Poco/RemotingNG/ORB.h"
#include "Poco/Util/TimerTask.h"
#include "Poco/NumberFormatter.h"
#include "Poco/SharedPtr.h"
#include "Poco/Delegate.h"
#include <sstream>


namespace Poco {
namespace JS {
namespace Bridge {


//
// Transport
//


class Transport: public Poco::RemotingNG::Transport
{
public:
	Transport()
	{
	}
	
	~Transport()
	{
	}
	
	// Transport
	const std::string& endPoint() const
	{
		return _endPoint;
	}
	
	void connect(const std::string& endPoint)
	{
		_endPoint = endPoint;
	}
	
	void disconnect()
	{
		_endPoint.clear();
	}
	
	bool connected() const
	{
		return !_endPoint.empty();
	}
	
	Poco::RemotingNG::Serializer& beginMessage(const Poco::RemotingNG::Identifiable::ObjectId& oid, const Poco::RemotingNG::Identifiable::TypeId& tid, const std::string& messageName, Poco::RemotingNG::SerializerBase::MessageType messageType)
	{
		poco_assert_dbg (messageType == Poco::RemotingNG::SerializerBase::MESSAGE_EVENT);

		_pStream = new std::stringstream;
		_serializer.setup(*_pStream);
		return _serializer;
	}
	
	void sendMessage(const Poco::RemotingNG::Identifiable::ObjectId& oid, const Poco::RemotingNG::Identifiable::TypeId& tid, const std::string& messageName, Poco::RemotingNG::SerializerBase::MessageType messageType)
	{
		poco_assert_dbg (messageType == Poco::RemotingNG::SerializerBase::MESSAGE_EVENT);

		std::string json = _pStream->str();
		BridgeHolder::Ptr pBridgeHolder = BridgeHolder::find(_endPoint);
		if (pBridgeHolder)
		{
			pBridgeHolder->fireEvent(messageName, json);
		}
	}
	
	Poco::RemotingNG::Serializer& beginRequest(const Poco::RemotingNG::Identifiable::ObjectId& oid, const Poco::RemotingNG::Identifiable::TypeId& tid, const std::string& messageName, Poco::RemotingNG::SerializerBase::MessageType messageType)
	{
		throw Poco::NotImplementedException("beginRequest() not implemented for jsbridge Transport");
	}
	
	Poco::RemotingNG::Deserializer& sendRequest(const Poco::RemotingNG::Identifiable::ObjectId& oid, const Poco::RemotingNG::Identifiable::TypeId& tid, const std::string& messageName, Poco::RemotingNG::SerializerBase::MessageType messageType)
	{
		throw Poco::NotImplementedException("sendRequest() not implemented for jsbridge Transport");
	}
	
	void endRequest()
	{
		throw Poco::NotImplementedException("endRequest() not implemented for jsbridge Transport");
	}
	
	static const std::string PROTOCOL;
	
private:
	std::string _endPoint;
	Poco::SharedPtr<std::stringstream> _pStream;
	JSONEventSerializer _serializer;
};


const std::string Transport::PROTOCOL("jsbridge");


//
// TransportFactory
//


class TransportFactory: public Poco::RemotingNG::TransportFactory
{
public:
	TransportFactory()
	{
	}
		
	~TransportFactory()
	{
	}
	
	Poco::RemotingNG::Transport* createTransport()
	{
		return new Transport;
	}

	static void registerFactory()
	{
		Poco::RemotingNG::TransportFactoryManager::instance().registerFactory(Transport::PROTOCOL, new TransportFactory);
	}

	static void unregisterFactory()
	{
		Poco::RemotingNG::TransportFactoryManager::instance().unregisterFactory(Transport::PROTOCOL);
	}
};


//
// ServerTransport
//


class ServerTransport: public Poco::RemotingNG::ServerTransport
{
public:
	ServerTransport(Deserializer& deserializer, Serializer& serializer):
		_deserializer(deserializer),
		_serializer(serializer)
	{
	}

	~ServerTransport()
	{
	}

	// ServerTransport
	Deserializer& beginRequest()
	{
		return _deserializer;
	}
		
	Serializer& sendReply(Poco::RemotingNG::SerializerBase::MessageType /*messageType*/)
	{
		return _serializer;
	}
	
	void endRequest()
	{
	}

private:
	Deserializer& _deserializer;
	Serializer& _serializer;
};


//
// EventTask
//


class EventTask: public Poco::Util::TimerTask
{
public:
	EventTask(Poco::JS::Core::TimedJSExecutor::Ptr pExecutor, v8::Isolate* pIsolate, const v8::Persistent<v8::Object>& jsObject, const std::string& event, const std::string& args):
		_pExecutor(pExecutor),
		_jsObject(pIsolate, jsObject),
		_event(event),
		_args(args)
	{
	}
	
	~EventTask()
	{
		_jsObject.Reset();
	}
	
	void run()
	{
		_pExecutor->call(_jsObject, _event, _args);
	}
	
private:
	Poco::JS::Core::TimedJSExecutor::Ptr _pExecutor;
	v8::Persistent<v8::Object> _jsObject;
	std::string _event;
	std::string _args;
};


//
// BridgeHolder
//


Poco::AtomicCounter BridgeHolder::_counter;
BridgeHolder::HolderMap BridgeHolder::_holderMap;
Poco::FastMutex BridgeHolder::_holderMapMutex;


BridgeHolder::BridgeHolder(v8::Isolate* pIsolate, const std::string& uri):
	_pIsolate(pIsolate),
	_pExecutor(Poco::JS::Core::JSExecutor::current()),
	_uri(uri)
{
	_subscriberURI += "jsbridge://local/jsbridge/Bridge/";
	int id = ++_counter;
	Poco::NumberFormatter::append(_subscriberURI, id);
	registerHolder();
	
	_pExecutor->stopped += Poco::delegate(this, &BridgeHolder::onExecutorStopped);
}


BridgeHolder::~BridgeHolder()
{
	try
	{
		if (_pExecutor)
		{
			_pExecutor->stopped -= Poco::delegate(this, &BridgeHolder::onExecutorStopped);
		}

		unregisterHolder();
		clear();
	}
	catch (...)
	{
		poco_unexpected();
	}
}


void BridgeHolder::setPersistent(const v8::Persistent<v8::Object>& jsObject)
{
	_persistent.Reset(_pIsolate, jsObject);
	_persistent.SetWeak(this, BridgeHolder::destruct);
	_persistent.MarkIndependent();
}


void BridgeHolder::clear()
{
	_persistent.ClearWeak();
	_persistent.Reset();
	try
	{
		disableEvents();
	}
	catch (...)
	{
		poco_unexpected();
	}
}


void BridgeHolder::fireEvent(const std::string& event, const std::string& args)
{
	if (_pExecutor)
	{
		Poco::JS::Core::TimedJSExecutor::Ptr pTimedExecutor = _pExecutor.cast<Poco::JS::Core::TimedJSExecutor>();
		if (pTimedExecutor)
		{
			EventTask::Ptr pEventTask = new EventTask(pTimedExecutor, _pIsolate, _persistent, event, args);
			pTimedExecutor->timer().schedule(pEventTask, Poco::Clock());
		}
	}
}


BridgeHolder::Ptr BridgeHolder::find(const std::string& subscriberURI)
{
	Poco::FastMutex::ScopedLock lock(_holderMapMutex);

	HolderMap::iterator it = _holderMap.find(subscriberURI);
	if (it != _holderMap.end())
		return Ptr(it->second, true);
	else
		return Ptr();
}


void BridgeHolder::destruct(const v8::WeakCallbackData<v8::Object, BridgeHolder>& data)
{
	data.GetValue().Clear();
	data.GetParameter()->clear();
}


void BridgeHolder::enableEvents()
{
	if (!_pEventDispatcher)
	{
		Poco::RemotingNG::Identifiable::Ptr pIdentifiable = Poco::RemotingNG::ORB::instance().findObject(_uri);
		Poco::RemotingNG::RemoteObject::Ptr pRemoteObject = pIdentifiable.cast<Poco::RemotingNG::RemoteObject>();
		if (pRemoteObject && pRemoteObject->remoting__hasEvents())
		{
			_pEventDispatcher = Poco::RemotingNG::ORB::instance().findEventDispatcher(_uri, "jsbridge");
			if (_pEventDispatcher)
			{
				_pEventDispatcher->subscribe(_subscriberURI, _subscriberURI);
			}
		}
	}
}


void BridgeHolder::disableEvents()
{
	if (_pEventDispatcher)
	{
		_pEventDispatcher->unsubscribe(_subscriberURI);
		_pEventDispatcher = 0;
	}
}


void BridgeHolder::registerHolder()
{
	Poco::FastMutex::ScopedLock lock(_holderMapMutex);
	
	_holderMap[_subscriberURI] = this;
}


void BridgeHolder::unregisterHolder()
{
	Poco::FastMutex::ScopedLock lock(_holderMapMutex);
	
	_holderMap.erase(_subscriberURI);
}


void BridgeHolder::onExecutorStopped()
{
	disableEvents();
	if (_pExecutor)
	{
		_pExecutor->stopped -= Poco::delegate(this, &BridgeHolder::onExecutorStopped);
	}
	_pExecutor = 0;
}


//
// BridgeWrapper
//


BridgeWrapper::BridgeWrapper()
{
}


BridgeWrapper::~BridgeWrapper()
{
}


v8::Handle<v8::FunctionTemplate> BridgeWrapper::constructor(v8::Isolate* pIsolate)
{
	return v8::FunctionTemplate::New(pIsolate, construct);
}


v8::Handle<v8::ObjectTemplate> BridgeWrapper::objectTemplate(v8::Isolate* pIsolate)
{
	v8::EscapableHandleScope handleScope(pIsolate);
	Poco::JS::Core::PooledIsolate* pPooledIso = Poco::JS::Core::PooledIsolate::fromIsolate(pIsolate);
	poco_check_ptr (pPooledIso);
	v8::Persistent<v8::ObjectTemplate>& pooledObjectTemplate(pPooledIso->objectTemplate("Bridge.Bridge"));
	if (pooledObjectTemplate.IsEmpty())
	{
		v8::Handle<v8::ObjectTemplate> objectTemplate = v8::ObjectTemplate::New();
		objectTemplate->SetInternalFieldCount(1);
		
		objectTemplate->SetNamedPropertyHandler(getProperty, setProperty);
	
		pooledObjectTemplate.Reset(pIsolate, objectTemplate);
	}
	v8::Local<v8::ObjectTemplate> dateTimeTemplate = v8::Local<v8::ObjectTemplate>::New(pIsolate, pooledObjectTemplate);
	return handleScope.Escape(dateTimeTemplate);
}


void BridgeWrapper::construct(const v8::FunctionCallbackInfo<v8::Value>& args)
{
	BridgeHolder::Ptr pHolder;
	try
	{
		if (args.Length() == 1)
		{
			pHolder = new BridgeHolder(args.GetIsolate(), toString(args[0]));
		}
		else
		{
			returnException(args, "invalid or missing arguments; object URI required");
			return;
		}
		
		BridgeWrapper wrapper;
		v8::Persistent<v8::Object>& bridgeObject(wrapper.wrapNativePersistent(args.GetIsolate(), pHolder));
		pHolder->setPersistent(bridgeObject);
		args.GetReturnValue().Set(bridgeObject);
	}
	catch (Poco::Exception& exc)
	{
		returnException(args, exc);
	}
}


void BridgeWrapper::getProperty(v8::Local<v8::String> property, const v8::PropertyCallbackInfo<v8::Value>& info)
{
	v8::Local<v8::Object> object = info.Holder();
	if (object->HasRealNamedProperty(property))
	{
		info.GetReturnValue().Set(object->GetRealNamedProperty(property));
	}
	else if (toString(property) == "on")
	{
		// For some reason trying to set this function in the object template leads
		// to a crash at runtime. Therefore this workaround.
		v8::Local<v8::Function> function = v8::Function::New(info.GetIsolate(), on);
		function->SetName(property);
		info.GetReturnValue().Set(function);
	}
	else
	{
		v8::Local<v8::Function> function = v8::Function::New(info.GetIsolate(), bridgeFunction);
		function->SetName(property);
		info.GetReturnValue().Set(function);
	}
}


void BridgeWrapper::setProperty(v8::Local<v8::String> name, v8::Local<v8::Value> value, const v8::PropertyCallbackInfo<v8::Value>& info)
{
	v8::Local<v8::Object> object = info.Holder();
	object->ForceSet(name, value);
	if (value->IsFunction())
	{
		BridgeHolder* pHolder = Wrapper::unwrapNative<BridgeHolder>(info);
		try
		{
			poco_check_ptr (pHolder);
			pHolder->enableEvents();
		}
		catch (Poco::Exception& exc)
		{
			returnException(info, exc);
		}
	}
}


void BridgeWrapper::bridgeFunction(const v8::FunctionCallbackInfo<v8::Value>& args)
{
	v8::HandleScope scope(args.GetIsolate());
	BridgeHolder* pHolder = Wrapper::unwrapNative<BridgeHolder>(args);
	if (pHolder)
	{
		try
		{
			std::string method(toString(args.Callee()->GetName()));
			v8::Local<v8::Object> argsArray = v8::Array::New(args.GetIsolate(), args.Length());
			for (int i = 0; i < args.Length(); i++)
			{
				argsArray->Set(i, args[i]);
			}
			Deserializer deserializer(method, Poco::RemotingNG::SerializerBase::MESSAGE_REQUEST, args.GetIsolate(), argsArray);
			Serializer serializer(args.GetIsolate());
			ServerTransport transport(deserializer, serializer);
			Listener listener;
			if (!Poco::RemotingNG::ORB::instance().invoke(listener, pHolder->uri(), transport))
			{
				returnException(args, std::string("object not found: " + pHolder->uri()));
			}
			else if (serializer.exception())
			{
				returnException(args, *serializer.exception());
			}
			else
			{
				v8::Local<v8::Object> returnObject = serializer.jsValue();
				v8::Local<v8::String> returnParam = v8::String::NewFromUtf8(args.GetIsolate(), Poco::RemotingNG::SerializerBase::RETURN_PARAM.c_str(), v8::String::kNormalString, Poco::RemotingNG::SerializerBase::RETURN_PARAM.size());
				if (serializer.totalSerialized() == 1 && returnObject->Has(returnParam))
				{
					args.GetReturnValue().Set(returnObject->Get(returnParam));
				}
				else
				{
					args.GetReturnValue().Set(returnObject);
				}
			}
		}
		catch (Poco::Exception& exc)
		{
			returnException(args, exc);
		}
	}
	else returnException(args, std::string("no object - bridge function cannot be called as standalone function"));
}


void BridgeWrapper::on(const v8::FunctionCallbackInfo<v8::Value>& args)
{
	v8::Local<v8::Object> object = args.Holder();
	if (args.Length() >= 1)
	{
		if (args[0]->IsString())
		{
			v8::Local<v8::String> name = v8::Local<v8::String>::Cast(args[0]);
			if (args.Length() >= 2 && args[1]->IsFunction())
			{
				object->ForceSet(name, args[1]);
				BridgeHolder* pHolder = Wrapper::unwrapNative<BridgeHolder>(args);
				try
				{
					poco_check_ptr (pHolder);
					pHolder->enableEvents();
				}
				catch (Poco::Exception& exc)
				{
					returnException(args, exc);
				}
			}
			else if (args.Length() >= 2)
			{
				returnException(args, "Invalid argument: Second argument to on() must be a function");
			}
			else
			{
				args.GetReturnValue().Set(object->GetRealNamedProperty(name));
			}
		}
		else
		{
			returnException(args, "Invalid argument: First argument to on() must be property name");
		}
	}
}


void BridgeWrapper::registerTransportFactory()
{
	TransportFactory::registerFactory();
}


void BridgeWrapper::unregisterTransportFactory()
{
	TransportFactory::unregisterFactory();
}


} } } // namespace Poco::JS::Bridge
