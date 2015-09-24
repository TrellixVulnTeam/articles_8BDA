//
// Struct4Deserializer.h
//
// Package: Generated
// Module:  TypeDeserializer
//
// This file has been generated.
// Warning: All changes to this will be lost when the file is re-generated.
//


#ifndef TypeDeserializer_Struct4_INCLUDED
#define TypeDeserializer_Struct4_INCLUDED


#include "Poco/RemotingNG/TypeDeserializer.h"
#include "Struct4Deserializer.h"
#include "Struct4Serializer.h"
#include "Tester.h"


namespace Poco {
namespace RemotingNG {


template <>
class TypeDeserializer<Struct4>
{
public:
	static bool deserialize(const std::string& name, bool isMandatory, Deserializer& deser, Struct4& value)
	{
		bool ret = deser.deserializeStructBegin(name, isMandatory);
		if (ret)
		{
			deserializeImpl(deser, value);
			deser.deserializeStructEnd(name);
		}
		return ret;
	}

	static void deserializeImpl(Deserializer& deser, Struct4& value)
	{
		remoting__staticInitBegin(REMOTING__NAMES);
		static const std::string REMOTING__NAMES[] = {"ptr","value","vec"};
		remoting__staticInitEnd(REMOTING__NAMES);
		TypeDeserializer<Poco::SharedPtr < Struct4 > >::deserialize(REMOTING__NAMES[0], true, deser, value.ptr);
		TypeDeserializer<std::string >::deserialize(REMOTING__NAMES[1], true, deser, value.value);
		TypeDeserializer<std::vector < Struct4 > >::deserialize(REMOTING__NAMES[2], true, deser, value.vec);
	}

};


} // namespace RemotingNG
} // namespace Poco


#endif // TypeDeserializer_Struct4_INCLUDED

