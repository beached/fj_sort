// The MIT License (MIT)
//
// Copyright (c) 2019 Darrell Wright
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files( the "Software" ), to
// deal in the Software without restriction, including without limitation the
// rights to use, copy, modify, merge, publish, distribute, sublicense, and / or
// sell copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#pragma once
#include <atomic>
#include <memory>

namespace daw {
	template<typename T>
	struct atomic_ptr {
		using pointer = T *;
		using element_type = T;

	private:
		std::atomic<pointer> m_ptr{nullptr};

		static constexpr bool is_nothrow_destructible_v =
		  std::is_nothrow_destructible_v<element_type>;

	public:
		atomic_ptr( ) noexcept = default;
		atomic_ptr( pointer ptr ) noexcept
		  : m_ptr( ptr ) {}

		atomic_ptr( atomic_ptr &&other ) noexcept
		  : m_ptr( other.m_ptr.exchange( nullptr ) ) {}

		atomic_ptr &
		operator=( atomic_ptr &&rhs ) noexcept( is_nothrow_destructible_v ) {
			if( this != &rhs ) {
				pointer tmp_other = rhs.m_ptr.exchange( nullptr );
				try {
					reset( tmp_other );
				} catch( ... ) {
					rhs.m_ptr.store( tmp_other );
					throw;
				}
				m_ptr.store( tmp_other );
			}
			return *this;
		}

		~atomic_ptr( ) noexcept( is_nothrow_destructible_v ) {
			reset( );
		}

		template<typename U>
		void
		reset( U *ptr,
		       std::memory_order order =
		         std::memory_order_seq_cst ) noexcept( is_nothrow_destructible_v ) {
			delete m_ptr.exchange( ptr );
		}

		void
		reset( std::nullptr_t p = nullptr,
		       std::memory_order order =
		         std::memory_order_seq_cst ) noexcept( is_nothrow_destructible_v ) {
			delete m_ptr.exchange( p );
		}
		void
		store( pointer ptr,
		       std::memory_order order =
		         std::memory_order_seq_cst ) noexcept( is_nothrow_destructible_v ) {
			reset( ptr, order );
		}

		pointer get( std::memory_order order = std::memory_order_seq_cst ) const
		  noexcept {
			return m_ptr.load( order );
		}

		pointer release( std::memory_order order = std::memory_order_seq_cst ) {
			return m_ptr.exchange( nullptr, order );
		}
	};

	template<typename T, typename... Args>
	atomic_ptr<T> make_atomic_ptr( Args &&... args ) noexcept(
	  std::is_nothrow_constructible_v<T, Args...> ) {
		if( std::is_aggregate_v<T> ) {
			return {new T{std::forward<Args>( args )...}};
		} else {
			return {new T( std::forward<Args>( args )... )};
		}
	}
} // namespace daw
