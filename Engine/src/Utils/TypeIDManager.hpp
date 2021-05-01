#pragma once


template<typename T>
class TypeIDManager
{
public:
	template<typename U>
	static const uint64_t Get()
	{
		static const uint64_t STATIC_TYPE_ID { s_count++ };
		return STATIC_TYPE_ID;
	}
private:
	static uint64_t s_count;
};
template<typename T>
uint64_t TypeIDManager<T>::s_count = 0;
