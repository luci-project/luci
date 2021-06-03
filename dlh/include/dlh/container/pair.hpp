#pragma once

template<class F, class S>
struct Pair {
	F first;
	S second;

	Pair() : first(), second() { }

	Pair(const F& first, const S& second) : first(first), second(second) { }

	template<class OF, class OS>
	Pair(const Pair<OF, OS>& o) : first(o.first), second(o.second) {}

	template<class OF, class OS>
	Pair& operator=(const Pair<OF,OS>& o) {
		first = o.first;
		second = o.second;
		return *this;
	}

	template<class OF, class OS>
	constexpr bool operator==(const Pair<OF,OS>& other) const {
		return first == other.first && second == other.second;
	}

	template<class OF, class OS>
	constexpr bool operator!=(const Pair<OF,OS>& other) const {
		return first != other.first || second != other.second;
	}

	template<class OF, class OS>
	constexpr bool operator<=(const Pair<OF,OS>& other) const {
		return first <= other.first && second <= other.second;
	}

	template<class OF, class OS>
	constexpr bool operator<(const Pair<OF,OS>& other) const {
		return first < other.first && second < other.second;
	}

	template<class OF, class OS>
	constexpr bool operator>=(const Pair<OF,OS>& other) const {
		return first >= other.first && second >= other.second;
	}

	template<class OF, class OS>
	constexpr bool operator>(const Pair<OF,OS>& other) const {
		return first > other.first && second > other.second;
	}
};
