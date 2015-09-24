//
// Struct3Deserializer.h
//
// Package: Generated
// Module:  TypeDeserializer
//
// This file has been generated.
// Warning: All changes to this will be lost when the file is re-generated.
//


#ifndef TypeDeserializer_Struct3_INCLUDED
#define TypeDeserializer_Struct3_INCLUDED


#include "Poco/RemotingNG/TypeDeserializer.h"
#include "Struct2Deserializer.h"
#include "Struct2Serializer.h"
#include "Tester.h"


namespace Poco {
namespace RemotingNG {


template <>
class TypeDeserializer<Struct3>
{
public:
	static bool deserialize(const std::string& name, bool isMandatory, Deserializer& deser, Struct3& value)
	{
		remoting__staticInitBegin(REMOTING__NAMESPACE);
		static const std::string REMOTING__NAMESPACE("http://www.appinf.com/types");
		remoting__staticInitEnd(REMOTING__NAMESPACE);
		bool ret = deser.deserializeStructBegin(name, isMandatory);
		deser.pushProperty(SerializerBase::PROP_NAMESPACE, REMOTING__NAMESPACE);
		if (ret)
		{
			deserializeImpl(deser, value);
			deser.popProperty(SerializerBase::PROP_NAMESPACE);
			deser.deserializeStructEnd(name);
		}
		else deser.popProperty(SerializerBase::PROP_NAMESPACE);
		return ret;
	}

	static void deserializeImpl(Deserializer& deser, Struct3& value)
	{
		remoting__staticInitBegin(REMOTING__NAMES);
		static const std::string REMOTING__NAMES[] = {"aCharVector","aComplexVector","aList","aMultiSet","aNullable","aSet"};
		remoting__staticInitEnd(REMOTING__NAMES);
		TypeDeserializer<std::vector < char > >::deserialize(REMOTING__NAMES[0], true, deser, value.aCharVector);
		TypeDeserializer<std::vector < Struct2 > >::deserialize(REMOTING__NAMES[1], true, deser, value.aComplexVector);
		TypeDeserializer<std::list < Struct2 > >::deserialize(REMOTING__NAMES[2], true, deser, value.aList);
		TypeDeserializer<std::multiset < int > >::deserialize(REMOTING__NAMES[3], true, deser, value.aMultiSet);
		TypeDeserializer<Poco::Nullable < std::string > >::deserialize(REMOTING__NAMES[4], true, deser, value.aNullable);
		TypeDeserializer<std::set < int > >::deserialize(REMOTING__NAMES[5], true, deser, value.aSet);
	}

};


} // namespace RemotingNG
} // namespace Poco


#endif // TypeDeserializer_Struct3_INCLUDED

