#pragma once

#include <type_traits>

#define STRING_JOIN2(arg1, arg2) DO_STRING_JOIN2(arg1, arg2)
#define DO_STRING_JOIN2(arg1, arg2) arg1 ## arg2
#define SCOPE_EXIT(BLOCK) auto STRING_JOIN2(scope_exit_, __LINE__) = mu::make_scope_exit([&](){ BLOCK ;});

// scope_exit
// make_scope_exit
namespace mu
{
	template<typename EF>
	class scope_exit;

	template<typename EF>
	scope_exit<EF> make_scope_exit(EF ef) { return scope_exit<EF>(ef); }

	template<typename EF>
	class scope_exit
	{
		EF m_exit_function;
		bool m_call_exit_func{ true };

	public:
		scope_exit(EF ef) noexcept
			: m_exit_function(ef)
		{
		}

		scope_exit(scope_exit&& other) noexcept
			: m_exit_function(std::move(other.m_exit_function))
			, m_call_exit_func(other.m_call_exit_func)
		{
			other.release();
		}

		~scope_exit()
		{
			if (m_call_exit_func) 
			{
				m_exit_function();
			}
		}

		void release() noexcept 
		{
			m_call_exit_func = false;
		}

		scope_exit(const scope_exit&) = delete;
		scope_exit& operator=(const scope_exit&) = delete;
		scope_exit& operator=(scope_exit&&) = delete;
	};
}

// make_unique_resource
// make_unique_resource_checked
// unique_resource_t
namespace mu
{
	namespace details
	{
		template<typename DERIVED, typename R>
		class unique_resource_arrow_support
		{
			template<typename U=R, EnableIf<std::is_pointer<U>::value>...>
			R operator->() const
			{
				return static_cast<const DERIVED*>(this)->get();
			}
		};

		template<typename DERIVED, typename R>
		class unique_resource_star_support
		{
			template<typename U = R, EnableIf<std::is_pointer<U>::value>...>
			std::add_lvalue_reference_t<std::remove_pointer_t<R>> operator*() const noexcept
			{
				return *(static_cast<const DERIVED*>(this)->get());
			}
		};
	}

	template<typename R, typename D>
	class unique_resource_t;

	template<typename R, typename D>
	unique_resource_t<R, D> make_unique_resource(R r, D d) noexcept
	{
		return unique_resource_t(R, d);
	}

	template<typename R, typename D, typename RI = R>
	unique_resource_t<R, D> make_unique_resource_checked(R r, RI ri, D d) noexcept
	{
		bool release = r == ri;
		auto ur = make_unique_resource(r, d);
		if (release) { ur.release(); }
		return std::move(ur);
	}

	template<typename R, typename D>
	class unique_resource_t : public details::unique_resource_arrow_support<unique_resource_t<R, D>, R>,
		details::unique_resource_star_support<unique_resource_t<R, D>, R>
	{
		R m_resource;
		D m_deleter;
		bool m_do_delete;

	public:
		unique_resource_t(R r, D d) noexcept
			: m_resource(r), m_deleter(d), m_do_delete(true)
		{
		}

		unique_resource_t(const unique_resource_t&) = delete;
		unique_resource_t(unique_resource_t&& other)
			: m_resource(std::move(other.m_resource))
			, m_deleter(std::move(other.m_deleter))
			, m_do_delete(other.m_do_delete)
		{
			other.release();
		}

		unique_resource_t& operator=(unique_resource_t&& other)
		{
			reset();

			m_resource = std::move(other.m_resource);
			m_deleter = std::move(other.m_deleter);
			m_do_delete = other.m_do_delete;

			other.release();
		}

		unique_resource_t& operator=(const unique_resource_t&) = delete;

		~unique_resource_t()
		{
			reset();
		}

		void reset()
		{
			if (m_do_delete)
			{
				m_do_delete = false;
				get_deleter()(m_resource);
			}
		}

		void reset(R r)
		{
			reset();
			resource = std::move(r);
			m_do_delete = true;
		}

		R release() noexcept
		{
			m_do_delete = false;
			return m_resource;
		}

		const R& get() const noexcept
		{
			return m_resource;
		}

		operator const R&() const noexcept
		{
			return m_resource;
		}

		const D& get_deleter() const noexcept
		{
			return m_deleter;
		}
	};
}