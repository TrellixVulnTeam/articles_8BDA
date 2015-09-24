//
// TesterEventDispatcher.h
//
// Package: Generated
// Module:  TesterEventDispatcher
//
// This file has been generated.
// Warning: All changes to this will be lost when the file is re-generated.
//


#ifndef TesterEventDispatcher_INCLUDED
#define TesterEventDispatcher_INCLUDED


#include "Poco/RemotingNG/EventDispatcher.h"
#include "TesterRemoteObject.h"


class TesterEventDispatcher: public Poco::RemotingNG::EventDispatcher
{
public:
	TesterEventDispatcher(TesterRemoteObject* pRemoteObject, const std::string& protocol);
		/// Creates a TesterEventDispatcher.

	virtual ~TesterEventDispatcher();
		/// Destroys the TesterEventDispatcher.

	void event__testEvent(const void* pSender, std::string& data);

	void event__testOneWayEvent(const void* pSender, std::string& data);

	virtual const Poco::RemotingNG::Identifiable::TypeId& remoting__typeId() const;

private:
	void event__testEventImpl(const std::string& subscriberURI, std::string& data);

	void event__testOneWayEventImpl(const std::string& subscriberURI, std::string& data);

	static const std::string DEFAULT_NS;
	TesterRemoteObject* _pRemoteObject;
};


inline const Poco::RemotingNG::Identifiable::TypeId& TesterEventDispatcher::remoting__typeId() const
{
	return ITester::remoting__typeId();
}




#endif // TesterEventDispatcher_INCLUDED

