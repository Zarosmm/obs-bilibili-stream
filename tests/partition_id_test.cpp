#include "ui/partition_id.hpp"
#include <cassert>
#include <limits>
int main()
{
	int id = 0;
	assert(UI::parsePartitionId("86", id) && id == 86);
	assert(UI::parsePartitionId(86, id) && id == 86);
	assert(UI::parsePartitionId("2147483647", id) && id == std::numeric_limits<int>::max());
	for (const json11::Json &value :
	     {json11::Json(), json11::Json(true), json11::Json(""), json11::Json("abc"), json11::Json("86abc"),
	      json11::Json("2147483648"), json11::Json("0"), json11::Json("-1"), json11::Json(0), json11::Json(-1),
	      json11::Json(1.5), json11::Json(2147483648.0)}) {
		assert(!UI::parsePartitionId(value, id));
	}
}
