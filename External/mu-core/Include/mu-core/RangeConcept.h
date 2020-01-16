#pragma once

#include <type_traits>
#include <utility>

#include "mu-core/Metaprogramming.h"
#include "mu-core/MemberDetection.h"

namespace mu {
	namespace details {	   
		DEFINE_HAS_STATIC_MEMBER(HasSize);
		DEFINE_HAS_STATIC_MEMBER(IsContiguous);
		DEFINE_HAS_STATIC_MEMBER(IsIndexable);

		DEFINE_HAS_INSTANCE_MEMBER(Advance);
		DEFINE_HAS_INSTANCE_MEMBER(Front);
		DEFINE_DETECTOR(Has_Size, size_t{ std::declval<const T>().Size() } );
		DEFINE_DETECTOR(Has_IsEmpty, bool{ std::declval<const T>().IsEmpty() });
		DEFINE_DETECTOR(Has_OperatorIndex, std::declval<T>()[0]);

		template<typename T>
		struct ConstRefToRefImpl {
			using Type = T;
		};

		template<typename T>
		struct ConstRefToRefImpl<const T&> {
			using Type = T&;
		};

		template<typename T> using ConstRefToRef = typename ConstRefToRefImpl<T>::Type;

		static_assert(std::is_same_v<int&, ConstRefToRef<const int&>>, "remove const from const int& failed");
		static_assert(std::is_same_v<int, ConstRefToRef<int>>, "test: remove ConstRefToRef<int>");
		static_assert(std::is_same_v<int&, ConstRefToRef<int&>>, "test: remove ConstRefToRef<int&>");

		template<class T>
		constexpr bool Range_Test_Front = Has_Front<std::add_const_t<std::remove_cv_t<std::remove_reference_t<T>>>>::Value;

		template<class T>
		constexpr bool Range_Test_Advance = Has_Advance<std::remove_cv_t<std::remove_reference_t<T>>>::Value;

		template<class T>
		constexpr bool Range_Test_IsEmpty = Has_IsEmpty<std::add_const_t<std::remove_cv_t<std::remove_reference_t<T>>>>::Value;

		template<class T>
		constexpr bool Range_Test_Size() {
			if constexpr (Has_HasSize<T>::Value) {
				//if constexpr (std::decay_t<T>::HasSize) {
				//	return Has_Size<std::add_const_t<std::remove_cv_t<std::remove_reference_t<T>>>>::Value;
				//}
				//else {
				//	return true;
				//}
				return std::decay_t<T>::HasSize == Has_Size<std::add_const_t<std::remove_cv_t<std::remove_reference_t<T>>>>::Value;
			}
			else {
				return false;
			}
		}

		template<class T>
		constexpr bool Range_Test_OperatorIndex() {
			if constexpr (Has_IsIndexable<T>::Value) {
				return std::decay_t<T>::IsIndexable == Has_OperatorIndex<std::add_const_t<std::remove_cv_t<std::remove_reference_t<T>>>>::Value;
			}
			return false;
		}

		template<class T>
		struct IsRange {
			using RawT = std::decay_t<T>;
			static constexpr bool Value = true
				&& Has_HasSize<T>::Value
				&& Has_IsContiguous<T>::Value
				&& Has_IsIndexable<T>::Value
				&& Range_Test_Advance<T>
				&& Range_Test_Front<T>
				&& Range_Test_IsEmpty<T>
				&& Range_Test_Size<T>()
				&& Range_Test_OperatorIndex<T>()
				;
		};
	}

	namespace tests {
		struct EmptyType {
		};
		struct FakeRange {
			static constexpr bool HasSize = true;
			static constexpr bool IsContiguous = false;
			static constexpr bool IsIndexable = false;

			void Advance();
			bool IsEmpty() const;
			size_t Size() const;
			int Front() const;
		};
		struct Indexable {
			int operator[](size_t) const;
		};
		struct NonConstIndexable {
			int operator[](size_t);
		};
		struct NonConstIsEmpty {
			bool IsEmpty();
		};
		struct NonConstSize{
			size_t Size();
		};
		struct NonConstFront {
			int Front();
		};

		struct Range_BrokenSize {
			static constexpr bool HasSize = true;
			static constexpr bool IsContiguous = false;
			static constexpr bool IsIndexable = false;

			void Advance();
			bool IsEmpty() const;
			int Front() const;
		};
		struct Range_CorrectSize {
			static constexpr bool HasSize = true;
			static constexpr bool IsContiguous = false;
			static constexpr bool IsIndexable = false;

			void Advance();
			bool IsEmpty() const;
			size_t Size() const;
			int Front() const;
		};

		struct Range_BrokenIndexable{
			static constexpr bool HasSize = true;
			static constexpr bool IsContiguous = false;
			static constexpr bool IsIndexable = true;

			void Advance();
			bool IsEmpty() const;
			size_t Size() const;
			int Front() const;
		};
		struct Range_CorrectIndexable{
			static constexpr bool HasSize = true;
			static constexpr bool IsContiguous = false;
			static constexpr bool IsIndexable = true;

			void Advance();
			bool IsEmpty() const;
			size_t Size() const;
			int Front() const;

			int operator[](size_t) const;
		};
		
		static_assert(details::Has_HasSize<EmptyType>::Value == false);
		static_assert(details::Has_HasSize<FakeRange>::Value == true);
		static_assert(details::Has_HasSize<FakeRange&>::Value == true);
		static_assert(details::Has_HasSize<const FakeRange&>::Value == true);
		static_assert(details::Has_HasSize<Range_BrokenSize>::Value == true);
		static_assert(details::Has_HasSize<Range_CorrectSize>::Value == true);
		static_assert(details::Has_HasSize<Range_BrokenIndexable>::Value == true);
		static_assert(details::Has_HasSize<Range_CorrectIndexable>::Value == true);

		static_assert(details::Has_IsContiguous<EmptyType>::Value == false);
		static_assert(details::Has_IsContiguous<FakeRange>::Value == true);
		static_assert(details::Has_IsContiguous<FakeRange&>::Value == true);
		static_assert(details::Has_IsContiguous<const FakeRange&>::Value == true);
		static_assert(details::Has_IsContiguous<Range_BrokenSize>::Value == true);
		static_assert(details::Has_IsContiguous<Range_CorrectSize>::Value == true);
		static_assert(details::Has_IsContiguous<Range_BrokenIndexable>::Value == true);
		static_assert(details::Has_IsContiguous<Range_CorrectIndexable>::Value == true);

		static_assert(details::Has_IsIndexable<EmptyType>::Value == false);
		static_assert(details::Has_IsIndexable<FakeRange>::Value == true);
		static_assert(details::Has_IsIndexable<FakeRange&>::Value == true);
		static_assert(details::Has_IsIndexable<const FakeRange&>::Value == true);
		static_assert(details::Has_IsIndexable<Range_BrokenIndexable>::Value == true);
		static_assert(details::Has_IsIndexable<Range_CorrectIndexable>::Value == true);

		static_assert(details::Range_Test_OperatorIndex<FakeRange>() == true);
		static_assert(details::Range_Test_OperatorIndex<FakeRange&>() == true);
		static_assert(details::Range_Test_OperatorIndex<const FakeRange&>() == true);
		static_assert(details::Range_Test_OperatorIndex<Range_BrokenIndexable>() == false);
		static_assert(details::Range_Test_OperatorIndex<Range_CorrectIndexable>() == true);

		static_assert(details::Range_Test_Advance<EmptyType> == false);
		static_assert(details::Range_Test_Advance<FakeRange> == true);
		static_assert(details::Range_Test_Advance<FakeRange&> == true);
		static_assert(details::Range_Test_Advance<const FakeRange&> == true);
		static_assert(details::Range_Test_Advance<Range_BrokenSize> == true);
		static_assert(details::Range_Test_Advance<Range_CorrectSize> == true);
		static_assert(details::Range_Test_Advance<Range_BrokenIndexable> == true);
		static_assert(details::Range_Test_Advance<Range_CorrectIndexable> == true);

		static_assert(details::Range_Test_Front<EmptyType> == false);
		static_assert(details::Range_Test_Front<FakeRange> == true);
		static_assert(details::Range_Test_Front<FakeRange&> == true);
		static_assert(details::Range_Test_Front<const FakeRange&> == true);
		static_assert(details::Range_Test_Front<Range_BrokenSize> == true);
		static_assert(details::Range_Test_Front<Range_CorrectSize> == true);
		static_assert(details::Range_Test_Front<Range_BrokenIndexable> == true);
		static_assert(details::Range_Test_Front<Range_CorrectIndexable> == true);
		static_assert(details::Range_Test_Front<NonConstFront> == false);

		static_assert(details::Range_Test_IsEmpty<EmptyType> == false);
		static_assert(details::Range_Test_IsEmpty<FakeRange> == true);
		static_assert(details::Range_Test_IsEmpty<FakeRange&> == true);
		static_assert(details::Range_Test_IsEmpty<const FakeRange&> == true);
		static_assert(details::Range_Test_IsEmpty<NonConstIsEmpty> == false);

		static_assert(details::Range_Test_Size<EmptyType>() == false);
		static_assert(details::Range_Test_Size<FakeRange>() == true);
		static_assert(details::Range_Test_Size<FakeRange&>() == true);
		static_assert(details::Range_Test_Size<const FakeRange&>() == true);
		static_assert(details::Range_Test_Size<NonConstSize>() == false);
		static_assert(details::Range_Test_Size<Range_BrokenSize>() == false);
		static_assert(details::Range_Test_Size<Range_CorrectSize>() == true);

		static_assert(details::IsRange<FakeRange>::Value);
		static_assert(details::IsRange<FakeRange&>::Value);
		static_assert(details::IsRange<const FakeRange&>::Value);
		static_assert(details::IsRange<Range_BrokenSize>::Value == false);
		static_assert(details::IsRange<Range_CorrectSize>::Value);
		static_assert(details::IsRange<Range_BrokenIndexable>::Value == false);
		static_assert(details::IsRange<Range_CorrectIndexable>::Value);
	}

	template<class T>
	constexpr bool IsRange = details::IsRange<T>::Value;
}