//
// Struct1Deserializer.h
//
// Package: Generated
// Module:  TypeDeserializer
//
// This file has been generated.
// Warning: All changes to this will be lost when the file is re-generated.
//


#ifndef TypeDeserializer_Struct1_INCLUDED
#define TypeDeserializer_Struct1_INCLUDED


#include "Poco/RemotingNG/TypeDeserializer.h"
#include "Tester.h"


namespace Poco {
namespace RemotingNG {


template <>
class TypeDeserializer<Struct1>
{
public:
	static bool deserialize(const std::string& name, bool isMandatory, Deserializer& deser, Struct1& value)
	{
		bool ret = deser.deserializeStructBegin(name, isMandatory);
		if (ret)
		{
			deserializeImpl(deser, value);
			deser.deserializeStructEnd(name);
		}
		return ret;
	}

	static void deserializeImpl(Deserializer& deser, Struct1& value)
	{
		remoting__staticInitBegin(REMOTING__NAMES);
		static const std::string REMOTING__NAMES[] = {"aDouble","aString","anEnum","anEnum2","anInt"};
		remoting__staticInitEnd(REMOTING__NAMES);
		bool ret = false;
		TypeDeserializer<double >::deserialize(REMOTING__NAMES[0], true, deser, value.aDouble);
		TypeDeserializer<std::string >::deserialize(REMOTING__NAMES[1], true, deser, value.aString);
		int genanEnum;
		ret = TypeDeserializer<int >::deserialize(REMOTING__NAMES[2], true, deser, genanEnum);
		if (ret) value.anEnum = static_cast<Enum1>(genanEnum);
		int genanEnum2;
		ret = TypeDeserializer<int >::deserialize(REMOTING__NAMES[3], true, deser, genanEnum2);
		if (ret) value.anEnum2 = static_cast<Struct1::Enum2>(genanEnum2);
		TypeDeserializer<int >::deserialize(REMOTING__NAMES[4], true, deser, value.anInt);
	}

};


} // namespace RemotingNG
} // namespace Poco


#endif // TypeDeserializer_Struct1_INCLUDED

