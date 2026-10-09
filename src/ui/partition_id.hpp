#pragma once
#include "json11/json11.hpp"
#include <charconv>
#include <cmath>
#include <limits>

namespace UI {
inline bool parsePartitionId(const json11::Json &value, int &id)
{
	if (value.is_string()) {
		const auto &text = value.string_value();
		auto result = std::from_chars(text.data(), text.data() + text.size(), id);
		return result.ec == std::errc{} && result.ptr == text.data() + text.size() && id > 0;
	}
	if (value.is_number()) {
		double number = value.number_value();
		if (std::isfinite(number) && number > 0 && number <= std::numeric_limits<int>::max() &&
		    std::floor(number) == number) {
			id = static_cast<int>(number);
			return true;
		}
	}
	return false;
}
} // namespace UI
