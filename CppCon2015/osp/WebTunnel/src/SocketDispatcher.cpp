//
// SocketDispatcher.cpp
//
// Library: WebTunnel
// Package: WebTunnel
// Module:  SocketDispatcher
//
// Definition of the SocketDispatcher class.
//
// Copyright (c) 2013, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "Poco/WebTunnel/SocketDispatcher.h"
#include "Poco/Net/NetException.h"


namespace Poco {
namespace WebTunnel {


class TaskNotification: public Poco::Notification
{
public:
	typedef Poco::AutoPtr<TaskNotification> Ptr;
	
	TaskNotification(SocketDispatcher& dispatcher):
		_dispatcher(dispatcher)
	{
	}
	
	~TaskNotification()
	{
	}
	
	virtual void execute() = 0;
	
protected:
	SocketDispatcher& _dispatcher;
};


class ReadableNotification: public TaskNotification
{
public:
	ReadableNotification(SocketDispatcher& dispatcher, const Poco::Net::StreamSocket& socket, const SocketDispatcher::SocketInfo::Ptr& pInfo):
		TaskNotification(dispatcher),
		_socket(socket),
		_pInfo(pInfo)
	{
	}
	
	void execute()
	{
		_dispatcher.readableImpl(_socket, _pInfo);
	}
	
private:
	Poco::Net::StreamSocket _socket;
	SocketDispatcher::SocketInfo::Ptr _pInfo;
};


class ExceptionNotification: public TaskNotification
{
public:
	ExceptionNotification(SocketDispatcher& dispatcher, const Poco::Net::StreamSocket& socket, const SocketDispatcher::SocketInfo::Ptr& pInfo):
		TaskNotification(dispatcher),
		_socket(socket),
		_pInfo(pInfo)
	{
	}
	
	void execute()
	{
		_dispatcher.exceptionImpl(_socket, _pInfo);
	}
	
private:
	Poco::Net::StreamSocket _socket;
	SocketDispatcher::SocketInfo::Ptr _pInfo;
};


class TimeoutNotification: public TaskNotification
{
public:
	TimeoutNotification(SocketDispatcher& dispatcher, const Poco::Net::StreamSocket& socket, const SocketDispatcher::SocketInfo::Ptr& pInfo):
		TaskNotification(dispatcher),
		_socket(socket),
		_pInfo(pInfo)
	{
	}
	
	void execute()
	{
		_dispatcher.timeoutImpl(_socket, _pInfo);
	}
	
private:
	Poco::Net::StreamSocket _socket;
	SocketDispatcher::SocketInfo::Ptr _pInfo;
};


class AddSocketNotification: public TaskNotification
{
public:
	AddSocketNotification(SocketDispatcher& dispatcher, const Poco::Net::StreamSocket& socket, const SocketDispatcher::SocketHandler::Ptr& pHandler, Poco::Timespan timeout):
		TaskNotification(dispatcher),
		_socket(socket),
		_pHandler(pHandler),
		_timeout(timeout)
	{
	}
	
	void execute()
	{
		_dispatcher.addSocketImpl(_socket, _pHandler, _timeout);
	}
	
private:
	Poco::Net::StreamSocket _socket;
	SocketDispatcher::SocketHandler::Ptr _pHandler;
	Poco::Timespan _timeout;
};


class RemoveSocketNotification: public TaskNotification
{
public:
	RemoveSocketNotification(SocketDispatcher& dispatcher, const Poco::Net::StreamSocket& socket):
		TaskNotification(dispatcher),
		_socket(socket)
	{
	}
	
	void execute()
	{
		_dispatcher.removeSocketImpl(_socket);
	}
	
private:
	Poco::Net::StreamSocket _socket;
};


class CloseSocketNotification: public TaskNotification
{
public:
	CloseSocketNotification(SocketDispatcher& dispatcher, const Poco::Net::StreamSocket& socket):
		TaskNotification(dispatcher),
		_socket(socket)
	{
	}
	
	void execute()
	{
		_dispatcher.closeSocketImpl(_socket);
	}
	
private:
	Poco::Net::StreamSocket _socket;
};


class ResetNotification: public TaskNotification
{
public:
	ResetNotification(SocketDispatcher& dispatcher):
		TaskNotification(dispatcher)
	{
	}
	
	void execute()
	{
		_dispatcher.resetImpl();
	}
};


SocketDispatcher::SocketDispatcher(int threadCount, Poco::Timespan timeout, int maxReadsPerWorker):
	_timeout(timeout),
	_maxReadsPerWorker(maxReadsPerWorker),
	_mainRunnable(*this, &SocketDispatcher::runMain),
	_workerRunnable(*this, &SocketDispatcher::runWorker),
	_stopped(false),
	_logger(Poco::Logger::get("WebTunnel.SocketDispatcher"))
{
	for (int i = 0; i < threadCount; i++)
	{
		ThreadPtr pThread = new Poco::Thread;
		pThread->start(_workerRunnable);
		_workerThreads.push_back(pThread);
	}
	_mainThread.start(_mainRunnable);
}

	
SocketDispatcher::~SocketDispatcher()
{
	try
	{
		stop();
	}
	catch (...)
	{
		poco_unexpected();
	}
}


void SocketDispatcher::stop()
{
	if (!_stopped) 
	{
		_stopped = true;
		_mainQueue.wakeUpAll();
		_workerQueue.wakeUpAll();
		_mainThread.join();
		for (ThreadVec::iterator it = _workerThreads.begin(); it != _workerThreads.end(); ++it)
		{
			(*it)->join();
		}
		_socketMap.clear();
	}
}


void SocketDispatcher::reset()
{
	_mainQueue.enqueueNotification(new ResetNotification(*this));
}


void SocketDispatcher::addSocket(const Poco::Net::StreamSocket& socket, SocketHandler::Ptr pHandler, Poco::Timespan timeout)
{
	_mainQueue.enqueueNotification(new AddSocketNotification(*this, socket, pHandler, timeout));
}

	
void SocketDispatcher::removeSocket(const Poco::Net::StreamSocket& socket)
{
	_mainQueue.enqueueNotification(new RemoveSocketNotification(*this, socket));
}


void SocketDispatcher::closeSocket(const Poco::Net::StreamSocket& socket)
{
	_mainQueue.enqueueNotification(new CloseSocketNotification(*this, socket));
}


void SocketDispatcher::runMain()
{
	Poco::Net::Socket::SocketList readList;
	Poco::Net::Socket::SocketList writeList;
	Poco::Net::Socket::SocketList exceptList;

	while (!_stopped)
	{
		try
		{
			readList.clear();
			exceptList.clear();
			for (SocketMap::iterator it = _socketMap.begin(); it != _socketMap.end(); ++it)
			{
				if (it->second->wantRead && it->second->timeout != 0 && it->second->timeout < it->second->activity.elapsed())
				{
					it->second->wantRead = false;
					it->second->activity.update();
					timeout(it->first, it->second);
				}
				if (it->second->wantRead)
				{
					readList.push_back(it->first);
					exceptList.push_back(it->first);	
				}
				else
				{
					// reset timeout clock
					it->second->activity.update();
				}
			}
			
			int n = Poco::Net::Socket::select(readList, writeList, exceptList, _timeout);
			if (n > 0)
			{
				for (Poco::Net::Socket::SocketList::iterator it = readList.begin(); it != readList.end(); ++it)
				{
					SocketMap::iterator its = _socketMap.find(*it);
					if (its != _socketMap.end())
					{
						its->second->wantRead = false;
						its->second->activity.update();
						readable(its->first, its->second);
					}
				}
				for (Poco::Net::Socket::SocketList::iterator it = exceptList.begin(); it != exceptList.end(); ++it)
				{
					SocketMap::iterator its = _socketMap.find(*it);
					if (its != _socketMap.end())
					{
						its->second->wantRead = false;
						its->second->activity.update();
						exception(its->first, its->second);
					}
				}
			}
		
			Poco::Notification::Ptr pNf = _socketMap.empty() ? _mainQueue.waitDequeueNotification() : _mainQueue.dequeueNotification();
			while (pNf)
			{
				TaskNotification::Ptr pTaskNf = pNf.cast<TaskNotification>();
				if (pTaskNf)
				{
					pTaskNf->execute();
				}
				pNf = _socketMap.empty() ? _mainQueue.waitDequeueNotification() : _mainQueue.dequeueNotification();
			}
		}
		catch (Poco::Net::NetException& exc)
		{
			if (exc.code() == POCO_ENOTCONN)
			{
				_logger.debug("A socket is no longer connected.");
			}
			else
			{
				_logger.error("Network exception in socket dispatcher: " + exc.displayText());
			}
		}
		catch (Poco::Exception& exc)
		{
			_logger.error("Exception in socket dispatcher: " + exc.displayText());
		}
	}
}


void SocketDispatcher::runWorker()
{
	while (!_stopped)
	{
		try
		{
			Poco::Notification::Ptr pNf = _workerQueue.waitDequeueNotification();
			if (pNf)
			{
				TaskNotification::Ptr pTaskNf = pNf.cast<TaskNotification>();
				if (pTaskNf)
				{
					pTaskNf->execute();
				}
			}
		}
		catch (Poco::Exception& exc)
		{
			_logger.error("Exception in worker thread: " + exc.displayText());
		}
	}
}


void SocketDispatcher::readable(const Poco::Net::StreamSocket& socket, const SocketDispatcher::SocketInfo::Ptr& pInfo)
{
	_workerQueue.enqueueNotification(new ReadableNotification(*this, socket, pInfo));
}


void SocketDispatcher::exception(const Poco::Net::StreamSocket& socket, const SocketDispatcher::SocketInfo::Ptr& pInfo)
{
	_workerQueue.enqueueNotification(new ExceptionNotification(*this, socket, pInfo));
}


void SocketDispatcher::timeout(const Poco::Net::StreamSocket& socket, const SocketDispatcher::SocketInfo::Ptr& pInfo)
{
	_workerQueue.enqueueNotification(new TimeoutNotification(*this, socket, pInfo));
}


void SocketDispatcher::readableImpl(Poco::Net::StreamSocket& socket, SocketDispatcher::SocketInfo::Ptr pInfo)
{
	try
	{
		int reads = 0;
		bool expectMore = false;
		do
		{
			expectMore = pInfo->pHandler->readable(*this, socket);
		}
		while (expectMore && reads++ < _maxReadsPerWorker && socket.poll(_timeout, Poco::Net::Socket::SELECT_READ));
	}
	catch (Poco::Exception& exc)
	{
		_logger.log(exc);
	}
	pInfo->wantRead = socket.impl()->initialized();
}


void SocketDispatcher::exceptionImpl(Poco::Net::StreamSocket& socket, SocketDispatcher::SocketInfo::Ptr pInfo)
{
	try
	{
		pInfo->pHandler->exception(*this, socket);
	}
	catch (Poco::Exception& exc)
	{
		_logger.log(exc);
	}
}


void SocketDispatcher::timeoutImpl(Poco::Net::StreamSocket& socket, SocketDispatcher::SocketInfo::Ptr pInfo)
{
	try
	{
		pInfo->pHandler->timeout(*this, socket);
	}
	catch (Poco::Exception& exc)
	{
		_logger.log(exc);
	}
	pInfo->wantRead = socket.impl()->initialized();
}


void SocketDispatcher::addSocketImpl(const Poco::Net::StreamSocket& socket, SocketHandler::Ptr pHandler, Poco::Timespan timeout)
{
	_socketMap[socket] = new SocketInfo(pHandler, timeout);
}


void SocketDispatcher::removeSocketImpl(const Poco::Net::StreamSocket& socket)
{
	_socketMap.erase(socket);
}


void SocketDispatcher::closeSocketImpl(Poco::Net::StreamSocket& socket)
{
	_socketMap.erase(socket);
	socket.shutdown();
}


void SocketDispatcher::resetImpl()
{
	_socketMap.clear();
}


} } // namespace Poco::WebTunnel
