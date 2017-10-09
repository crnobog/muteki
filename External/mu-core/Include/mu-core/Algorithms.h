#pragma once

#include "Ranges.h"

// TODO: Consider argument types. Should I be passing by value mostly here?

// Common algorithms operating on ranges
namespace mu {
	// Move assign elements from the source to the destination
	// Returns the remaining range in the destination after consuming the source.
	template<typename DEST_RANGE, typename SOURCE_RANGE>
	auto Move(DEST_RANGE&& in_dest, SOURCE_RANGE&& in_source) {
		auto dest = Range(std::forward<DEST_RANGE>(in_dest));
		auto source = Range(std::forward<SOURCE_RANGE>(in_source));
		for (; !dest.IsEmpty() && !source.IsEmpty(); dest.Advance(), source.Advance()) {
			dest.Front() = std::move(source.Front());
		}
		return dest;
	}

	// Move CONSTRUCT elements from the source into the destination.
	// Assumes the destination is uninitialized or otherwise does not 
	//	require destructors/assignment operators to be called.
	// Returns the remaining range in the destination after consuming the source.
	template<typename DEST_RANGE, typename SOURCE_RANGE>
	auto MoveConstruct(DEST_RANGE&& in_dest, SOURCE_RANGE&& in_source) {
		auto dest = Range(std::forward<DEST_RANGE>(in_dest));
		auto source = Range(std::forward<SOURCE_RANGE>(in_source));
		typedef std::remove_reference<decltype(dest.Front())>::type ELEMENT_TYPE;
		for (; !dest.IsEmpty() && !source.IsEmpty(); dest.Advance(), source.Advance()) {
			new(&dest.Front()) ELEMENT_TYPE(std::move(source.Front()));
		}
		return dest;
	}

	// Copy assign elements from the source into the destination
	// Returns the remaining range in the destination after consuming the source.
	template<typename DEST_RANGE, typename SOURCE_RANGE>
	auto Copy(DEST_RANGE&& in_dest, SOURCE_RANGE&& in_source) {
		auto dest = Range(std::forward<DEST_RANGE>(in_dest));
		auto source = Range(std::forward<SOURCE_RANGE>(in_source));
		for (; !dest.IsEmpty() && !source.IsEmpty(); dest.Advance(), source.Advance()) {
			dest.Front() = source.Front(); // TODO: Should I forward here?
		}
		return dest;
	}

	// Copy CONSTRUCT elements from the source into the destination.
	// Assumes the destination is uninitialized or otherwise does not 
	//	require destructors/assignment operators to be called.
	// Returns the remaining range in the destination after consuming the source.
	template<typename DEST_RANGE, typename SOURCE_RANGE>
	auto CopyConstruct(DEST_RANGE&& in_dest, SOURCE_RANGE&& in_source) {
		auto dest = Range(std::forward<DEST_RANGE>(in_dest));
		auto source = Range(std::forward<DEST_RANGE>(in_source));
		typedef std::remove_reference<decltype(dest.Front())>::type ELEMENT_TYPE;
		for (; !dest.IsEmpty() && !source.IsEmpty(); dest.Advance(), source.Advance()) {
			new(&dest.Front()) ELEMENT_TYPE(source.Front()); // TODO: Should I forward here?
		}
		return dest;
	}

	// Executes f for every element in the input range
	// If the range returns values by reference they can be modified.
	template<typename RANGE, typename FUNC>
	auto Map(RANGE&& in_r, FUNC&& f) {
		for (auto&& r = Range(std::forward<RANGE>(in_r));
			!r.IsEmpty(); r.Advance()) {
			f(r.Front());
		}
	}

	// Finds the first element in the range which satisfies the predicate f
	// Returns a copy of the range advanced to the matching element or an empty range
	template<typename RANGE, typename FUNC>
	auto Find(RANGE&& in_r, FUNC&& f) {
		auto&& r = Range(std::forward<RANGE>(in_r));
		for (; !r.IsEmpty(); r.Advance()) {
			if (f(r.Front())) {
				return r;
			}
		}
		return r;
	}

	// Finds the next element in the range which satisfies the predicate f 
	//  - ignores the first element of the range to allow for iterated search
	// Returns a copy of the range advanced to the matching element or an empty range
	template<typename RANGE, typename FUNC>
	auto FindNext(RANGE&& in_r, FUNC&& f) {
		if (in_r.IsEmpty()) { return in_r; }

		auto&& r = Range(std::forward<RANGE>(in_r));
		r.Advance(); // Skip the last found value
		for (; !r.IsEmpty(); r.Advance()) {
			if (f(r.Front())) {
				return r;
			}
		}
		return r;
	}

	// Finds the LAST element in the range which satisfies the predicate f
	// Returns a copy of the range advanced to the matching element or an empty range
	template<typename RANGE, typename FUNC>
	auto FindLast(RANGE&& in_r, FUNC&& f) {
		auto r = Find(in_r, f);
		if (r.IsEmpty()) {
			return r;
		}
		while (true) {
			auto next = FindNext(r, f);
			if (next.IsEmpty()) {
				return r;
			}
			r = next;
		}
	}

	// Returns true of the range contains an element satisfying the predicate
	template<typename RANGE, typename FUNC>
	bool Contains(RANGE&& in_r, FUNC&& f) {
		return !Find(std::forward<RANGE>(in_r), std::forward<FUNC>(f) ).IsEmpty();
	}

	// Fill the range with objects constructs with the given arguments
	template<typename RANGE, typename... ARGS>
	void FillConstruct(RANGE&& in_r, ARGS... args) {
		auto&& r = Range(std::forward<RANGE>(in_r));

		typedef std::decay<decltype(r.Front())>::type ITEM_TYPE;

		for (auto& item : r) {
			new(&item) ITEM_TYPE(std::forward<ARGS>(args)...);
		}
	}
	// Fill the range with objects constructs with the given arguments
	template<typename RANGE, typename... ARGS>
	void Fill(RANGE&& in_r, ARGS... args) {
		auto&& r = Range(std::forward<RANGE>(in_r));

		typedef std::decay<decltype(r.Front())>::type ITEM_TYPE;

		for (auto& item : r) {
			item = ITEM_TYPE(std::forward<ARGS>(args)...);
		}
	}


	template<typename RANGE>
	void Destroy(RANGE r) {
		typedef std::decay<decltype(r.Front())>::type ITEM_TYPE;
		for (auto& item : r) {
			item.~ITEM_TYPE();
		}
	}
}

#ifdef DOCTEST_LIBRARY_INCLUDED
#include "Tests/Algorithms_Tests.inl"
#endif
