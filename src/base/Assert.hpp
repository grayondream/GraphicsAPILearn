#pragma once
#include <cassert>

#define ASSERT(cond, fmt, ...)\
do{\
	if (!(cond)) {\
        LOGE(fmt, ##__VA_ARGS__);\
		assert(0);\
	}\
}while(0)