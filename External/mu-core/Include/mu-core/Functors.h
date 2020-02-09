#pragma once

#include <tuple>
#include <type_traits>

namespace mu {
	namespace functor {
		namespace details {
			using std::decay;
			using std::invoke_result;

			template<template<class...> class FUNCTOR, typename T>
			using FunctorApplication = invoke_result<FUNCTOR<T>, T>;

			template<template<class...> class FUNCTOR, typename T>
			typename FunctorApplication<FUNCTOR, T>::type CallFunctor(T&& t) {
				return FUNCTOR<decay<T>::type>{}(t);
			}

			template<typename TUPLE>
			using TupleSize = std::tuple_size<typename std::decay<TUPLE>::type>;

			template<typename TUPLE>
			using TupleIndices = std::make_index_sequence<TupleSize<TUPLE>::value>;

			template<template<typename...> class FUNCTOR, class R>
			constexpr auto FoldRec(R r) {
				return r;
			}

			template<template<typename...> class FUNCTOR, typename R, typename T, typename... TS>
			constexpr auto FoldRec(R r, T&& t, TS&&... ts) {
				return FoldRec<FUNCTOR>(FUNCTOR<decay<T>::type>{}(r, t), ts...);
			}

			template<template<typename...> class FUNCTOR, typename R, typename... TS>
			constexpr auto FoldRec(R r, TS&&... ts) {
				return FoldRec<FUNCTOR>(r, ts...);
			}

			template<template<typename...> class FUNCTOR, typename R, typename TUPLE, size_t... INDICES>
			constexpr auto FoldHelper(R r, TUPLE&& t, std::index_sequence<INDICES...>) {
				return FoldRec<FUNCTOR>(r, std::get<INDICES>(t)...);
			}

			template<template<typename...> class FUNCTOR>
			void FMapVoidRec() {}

			template<template<typename...> class FUNCTOR, typename T, typename... TS>
			void FMapVoidRec(T&& t, TS&&... ts) {
				CallFunctor<FUNCTOR>(t);
				FMapVoidRec<FUNCTOR>(ts...);
			}

			template<template<typename...> class FUNCTOR, typename TUPLE, size_t... INDICES>
			auto FMapVoidHelper(TUPLE&& t, std::index_sequence<INDICES...>) {
				return details::FMapVoidRec<FUNCTOR>(std::get<INDICES>(t)...);
			}

			template<template<typename...> class FUNCTOR, typename... TS>
			auto FMap(TS&&... ts) {
				return std::tuple<typename FunctorApplication<FUNCTOR, TS>::type...>(CallFunctor<FUNCTOR>(ts)...);
			}

			template<template<typename...> class FUNCTOR, typename TUPLE, size_t... INDICES>
			auto FMapHelper(TUPLE&& t, std::index_sequence<INDICES...>) {
				return details::FMap<FUNCTOR>(std::get<INDICES>(t)...);
			}

			template<typename T>
			struct And {
				constexpr bool operator()(bool a, T b) const { return a && b; }
			};

			template<typename T>
			struct Or {
				constexpr bool operator()(bool a, T b) const { return a || b; }
			};

		}

		template<template<typename...> class FUNCTOR, class R, typename TUPLE>
		auto Fold(R r, TUPLE&& t) {
			return details::FoldHelper<FUNCTOR>(r, t, details::TupleIndices<TUPLE>());
		}

		template<typename TUPLE>
		bool FoldAnd(TUPLE&& t) {
			return Fold<details::And>(true, t);
		}

		template<typename... TS>
		constexpr bool FoldAnd(TS&&... ts) {
			return details::FoldRec<details::And>(true, ts...);
		}

		template<typename TUPLE>
		bool FoldOr(TUPLE&& t) {
			return Fold<details::Or>(false, t);
		}

		template<typename... TS>
		constexpr bool FoldOr(TS&&... ts) {
			return details::FoldRec<details::Or>(false, ts...);
		}

		template<template<typename...> class FUNCTOR, typename... TS>
		void FMapVoid(TS&&... ts) { details::FMapVoidRec<FUNCTOR>(ts...); }

		template<template<typename...> class FUNCTOR, typename TUPLE>
		auto FMapVoid(TUPLE&& t) {
			return details::FMapVoidHelper<FUNCTOR>(t, details::TupleIndices<TUPLE>());
		}

		template<template<typename...> class FUNCTOR, typename TUPLE>
		auto FMap(TUPLE&& t) {
			return details::FMapHelper<FUNCTOR>(t, details::TupleIndices<TUPLE>());
		}

		template<template<typename...> class FUNCTOR, typename... TS>
		auto FMap(TS&&... ts) {
			return details::FMap<FUNCTOR>(ts...);
		}
	}
} // namespace mu::meta
