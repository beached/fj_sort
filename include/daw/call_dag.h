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
#include <chrono>
#include <condition_variable>
#include <exception>
#include <functional>
#include <memory>
#include <type_traits>
#include <variant>

#include "daw_mutable_capture.h"

namespace daw {
	template<typename T>
	class promise {
		using variant_t =
		  std::variant<std::monostate, T, std::exception_ptr,
		               std::function<void( std::variant<T, std::exception_ptr> )>>;
		enum var_pos { pos_empty, pos_eptr, pos_value, pos_cont };
		variant_t m_result{};
		std::mutex m_mutex{};
		std::condition_variable m_condition{};

		bool empty( ) const noexcept {
			return m_result.index( ) == pos_empty;
		}

		template<typename U>
		void set( var_pos idx, U &&value, bool notify = true ) noexcept {
			auto ftmp = std::function<void( std::variant<T, std::exception_ptr> )>( );
			{
				auto const lck = std::lock_guard( m_mutex );
				if( empty( ) ) {
					try {
						m_result =
						  variant_t( std::in_place_index<idx>, std::forward<U>( value ) );
					} catch( ... ) {
						m_result = variant_t( std::in_place_index<pos_eptr>,
						                      std::current_exception( ) );
					}
				} else {
					ftmp = std::get<pos_cont>( std::move( m_result ) );
				}
			}
			if( not ftmp ) {
				if( notify ) {
					m_condition.notify_one( );
				}
				return;
			}
			switch( idx ) {
			case pos_value:
				std::move( ftmp )( std::variant<T, std::exception_ptr>(
				  std::in_place_index<0>, std::forward<U>( value ) ) );
			case pos_eptr:
				std::move( ftmp )( std::variant<T, std::exception_ptr>(
				  std::in_place_index<1>, std::forward<U>( value ) ) );
			}
		}

		bool is_ready( ) const {
			return m_result.index( ) == pos_value or m_result.index( ) == pos_eptr;
		}

	public:
		void set_value( T &&value ) noexcept {
			set( pos_value, std::move( value ) );
		}

		void set_exception( std::exception_ptr ptr ) noexcept {
			set( pos_eptr, ptr );
		}

		template<typename Function>
		auto set_continuation( Function &&func ) {
			static_assert( std::is_invocable_v<Function, T> );
			using result_t = std::remove_cv_t<
			  std::remove_reference_t<decltype( func( std::declval<T>( ) ) )>>;

			auto result = std::shared_ptr<promise<result_t>>( );
			auto const lck = std::lock_guard( m_mutex );
			if( not is_ready( ) ) {
				result.set(
				  pos_cont,
				  std::function<void( std::variant<T, std::exception_ptr> )>(
				    [func = daw::mutable_capture( std::forward<Function>( func ) ),
				     wprom = std::weak_ptr( result )]( T &&v ) {
					    if( auto prom = wprom.lock( ); prom ) {
						    try {
							    prom->set_value( ( *std::move( func ) )( std::move( v ) ) );
						    } catch( ... ) {
							    prom->set_exception( std::current_exception( ) );
						    }
					    }
				    } ),
				  false );
				return result;
			}
			try {
				result->set_value( std::forward<Function>( func )( get( ) ) );
			} catch( ... ) { result->set_exception( std::current_exception( ) ); }
			return result;
		}

		void wait( ) const {
			auto lck = std::unique_lock( m_mutex );
			m_condition.wait( lck, [&] { return is_ready( ); } );
		}

		template<class Rep, class Period>
		std::cv_status
		wait_for( std::chrono::duration<Rep, Period> const &rel_time ) const {
			auto lck = std::unique_lock( m_mutex );
			return m_condition.wait_for( lck, rel_time, [&] { return is_ready( ); } );
		}

		template<class Clock, class Duration>
		std::cv_status
		wait_until( std::chrono::time_point<Clock, Duration> const &timeout_time ) {
			auto lck = std::unique_lock( m_mutex );
			return m_condition.wait_until( lck, timeout_time,
			                               [&] { return is_ready( ); } );
		}

		T get( ) {
			wait( );
			if( m_result.index( ) == 1 ) {
				return std::get<1>( std::move( m_result ) );
			}
			std::rethrow_exception( std::get<2>( m_result ) );
		}

		std::exception_ptr get_exception( ) noexcept {
			wait( );
			try {
				return std::get<2>( m_result );
			} catch( ... ) { return std::current_exception( ); }
		}

		bool has_value( ) const noexcept {
			wait( );
			return m_result.index( ) == 1;
		}

		bool has_exception( ) const noexcept {
			wait( );
			return m_result.index( ) == 2;
		}
	}; // namespace daw

	template<typename Result>
	struct packaged_task {
		static inline constexpr bool is_packaged_task = true;
		using result_type = Result;

	private:
		std::shared_ptr<promise<Result>> m_promise =
		  std::make_shared<promise<Result>>( );
		std::function<Result( )> m_func;

	public:
		packaged_task( std::function<Result( )> f )
		  : m_func( std::move( f ) ) {}

		template<typename Executor>
		std::shared_ptr<promise<Result>> operator( )( Executor &ex ) noexcept(
		  std::is_nothrow_invocable_v<Executor, std::function<void( )>> ) {
			post( ex, [wprom = std::weak_ptr( m_promise ),
			           m_func = daw::mutable_capture( std::move( m_func ) )] {
				if( auto prom = wprom.lock( ); prom ) {
					try {
						prom->set_value( ( *std::move( m_func )( ) ) );
					} catch( ... ) { prom->set_exception( std::current_exception( ) ); }
				}
			} );
			return std::move( m_promise );
		}
	};

	template<typename PackagedTask, typename Function>
	constexpr auto then( PackagedTask &&pt, Function f ) {
		static_assert( PackagedTask::is_packaged_task );
		static_assert( std::is_invocable_v<Function, PackagedTask::result_type> );
	}
} // namespace daw
