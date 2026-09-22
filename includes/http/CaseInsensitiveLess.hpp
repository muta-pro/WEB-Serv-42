#pragma once

# include <string>
# include <cctype>
# include <string_view>

struct CaseInsensitiveLess { // a struct for normalizing strings in the map
	bool operator()(std::string_view a, std::string_view b) const // it's ok to keep string_view here for parameter types - Liza
	{
		size_t n = a.size() < b.size() ? a.size() : b.size();
		for (size_t i = 0; i < n; ++i) {
			unsigned char x = tolower(static_cast<unsigned char>(a[i]));
			unsigned char y = tolower(static_cast<unsigned char>(b[i]));
			if (x != y)
				return x < y;
		}
		return a.size() < b.size();
	}
};
