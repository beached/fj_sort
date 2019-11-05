// The MIT License (MIT)
//
// Copyright (c) 2014-2019 Darrell Wright
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

#include <chrono>
#include <cstdint>
#include <exception>
#include <functional>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

namespace daw {
	namespace utility {
		template<typename T>
		std::string format_seconds( T t, size_t prec = 0 ) {
			std::stringstream ss;
			ss << std::setprecision( static_cast<int>( prec ) ) << std::fixed;
			auto val = static_cast<double>( t ) * 1000000000000000.0;
			if( val < 1000 ) {
				ss << val << "fs";
				return ss.str( );
			}
			val /= 1000.0;
			if( val < 1000 ) {
				ss << val << "ps";
				return ss.str( );
			}
			val /= 1000.0;
			if( val < 1000 ) {
				ss << val << "ns";
				return ss.str( );
			}
			val /= 1000.0;
			if( val < 1000 ) {
				ss << val << "us";
				return ss.str( );
			}
			val /= 1000.0;
			if( val < 1000 ) {
				ss << val << "ms";
				return ss.str( );
			}
			val /= 1000.0;
			ss << val << "s";
			return ss.str( );
		}

		template<typename Bytes, typename Time = double>
		std::string to_bytes_per_second( Bytes bytes, Time t = 1.0,
		                                 size_t prec = 1 ) {
			std::stringstream ss;
			ss << std::setprecision( static_cast<int>( prec ) ) << std::fixed;
			auto val = static_cast<double>( bytes ) / static_cast<double>( t );
			if( val < 1024.0 ) {
				ss << ( static_cast<double>( val * 100.0 ) / 100 ) << "bytes";
				return ss.str( );
			}
			val /= 1024.0;
			if( val < 1024.0 ) {
				ss << ( static_cast<double>( val * 100.0 ) / 100 ) << "KB";
				return ss.str( );
			}
			val /= 1024.0;
			if( val < 1024.0 ) {
				ss << ( static_cast<double>( val * 100.0 ) / 100 ) << "MB";
				return ss.str( );
			}
			val /= 1024.0;
			if( val < 1024.0 ) {
				ss << ( static_cast<double>( val * 100.0 ) / 100 ) << "GB";
				return ss.str( );
			}
			val /= 1024.0;
			if( val < 1024.0 ) {
				ss << ( static_cast<double>( val * 100.0 ) / 100 ) << "TB";
				return ss.str( );
			}
			val /= 1024.0;
			ss << ( static_cast<double>( val * 100.0 ) / 100 ) << "PB";
			return ss.str( );
		}
	} // namespace utility

	template<typename Func>
	void show_benchmark( size_t data_size_bytes, std::string const &title,
	                     Func &&func, size_t data_prec = 1, size_t time_prec = 0,
	                     size_t item_count = 1 ) noexcept {
		double const t = benchmark( std::forward<Func>( func ) );
		double const t_per_item = t / static_cast<double>( item_count );
		std::cout << title << ": took " << utility::format_seconds( t, time_prec )
		          << ' ';
		if( item_count > 1 ) {
			std::cout << "or " << utility::format_seconds( t_per_item, time_prec )
			          << " per item to process ";
		}
		std::cout << utility::to_bytes_per_second( data_size_bytes, 1.0, data_prec )
		          << " at "
		          << utility::to_bytes_per_second( data_size_bytes, t, data_prec )
		          << "/s\n";
	}

	/*
	  // Borrowed from https://www.youtube.com/watch?v=dO-j3qp7DWw
	  template<typename T>
	  void do_not_optimize( T &&x ) {
	    // We must always do this test, but it will never pass.
	    //
	    if( std::chrono::system_clock::now( ) ==
	        std::chrono::time_point<std::chrono::system_clock>( ) ) {
	      // This forces the value to never be optimized away
	      // by taking a reference then using it.
	      const auto *p = &x;
	      putchar( *reinterpret_cast<const char *>( p ) );

	      // If we do get here, kick out because something has gone wrong.
	      std::abort( );
	    }
	  }
	*/
#ifndef _MSC_VER
	template<typename Tp>
	inline void do_not_optimize( Tp const &value ) {
		asm volatile( "" : : "r,m"( value ) : "memory" );
	}

	template<typename Tp>
	inline void do_not_optimize( Tp &value ) {
#if defined( __clang__ )
		asm volatile( "" : "+r,m"( value ) : : "memory" );
#else
		asm volatile( "" : "+m,r"( value ) : : "memory" );
#endif
	}
#else
	namespace internal {
		inline void UseCharPointer( char const volatile * ) {}
	} // namespace internal

	template<class T>
	inline void do_not_optimize( T const &value ) {
		internal::UseCharPointer(
		  &reinterpret_cast<char const volatile &>( value ) );
		_ReadWriteBarrier( );
	}

#endif

	// Test N runs
	template<size_t Runs, char delem = '\n', typename Test, typename... Args>
	auto bench_n_test( std::string const &title, Test &&test_callable,
	                   Args &&... args ) noexcept {
		static_assert( Runs > 0 );
		using result_t = std::invoke_result_t<Test, Args...>;

		result_t result{};

		double base_time = std::numeric_limits<double>::max( );
		{
			for( size_t n = 0; n < 1000; ++n ) {
				(void)( ( do_not_optimize( args ), 1 ) + ... );

				int a = 0;
				daw::do_not_optimize( a );
				auto const start = std::chrono::high_resolution_clock::now( );
				auto r = [a]( ) { return a * a; };
				auto const finish = std::chrono::high_resolution_clock::now( );
				daw::do_not_optimize( r );
				auto const duration =
				  std::chrono::duration<double>( finish - start ).count( );
				if( duration < base_time ) {
					base_time = duration;
				}
			}
		}
		double min_time = std::numeric_limits<double>::max( );
		double max_time = 0.0;

		auto const total_start = std::chrono::high_resolution_clock::now( );
		for( size_t n = 0; n < Runs; ++n ) {
			(void)( ( do_not_optimize( args ), 1 ) + ... );
			auto const start = std::chrono::high_resolution_clock::now( );

			result = test_callable( args... );

			auto const finish = std::chrono::high_resolution_clock::now( );
			daw::do_not_optimize( result );
			auto const duration =
			  std::chrono::duration<double>( finish - start ).count( );
			if( duration < min_time ) {
				min_time = duration;
			}
			if( duration > max_time ) {
				max_time = duration;
			}
		}
		auto const total_finish = std::chrono::high_resolution_clock::now( );
		min_time -= base_time;
		max_time -= base_time;
		auto total_time =
		  std::chrono::duration<double>( total_finish - total_start ).count( ) -
		  static_cast<double>( Runs ) * base_time;

		auto avg_time =
		  Runs >= 10 ? ( total_time - max_time ) / static_cast<double>( Runs - 1 )
		             : total_time / static_cast<double>( Runs );
		avg_time -= base_time;
		std::cout << title << delem << "	runs: " << Runs << delem
		          << "	total: " << utility::format_seconds( total_time, 2 )
		          << delem << "	avg: " << utility::format_seconds( avg_time, 2 )
		          << delem << "	min: " << utility::format_seconds( min_time, 2 )
		          << delem << "	max: " << utility::format_seconds( max_time, 2 )
		          << '\n';
		return result;
	}

	template<size_t Runs, char delem = '\n', typename Validator,
	         typename Function, typename... Args>
	std::array<double, Runs>
	bench_n_test_mbs2( std::string const &title, size_t bytes,
	                   Validator &&validator, Function &&func, Args &&... args ) {
		static_assert( Runs > 0 );
		auto results = std::array<double, Runs>{};

		double base_time = std::numeric_limits<double>::max( );
		{
			for( size_t n = 0; n < 1000; ++n ) {
				(void)( ( do_not_optimize( args ), 1 ) + ... );

				int a = 0;
				daw::do_not_optimize( a );
				auto const start = std::chrono::high_resolution_clock::now( );
				auto r = [a]( ) { return a * a; };
				daw::do_not_optimize( r );
				auto const finish = std::chrono::high_resolution_clock::now( );
				auto const duration =
				  std::chrono::duration<double>( finish - start ).count( );
				if( duration < base_time ) {
					base_time = duration;
				}
			}
		}
		double min_time = std::numeric_limits<double>::max( );
		double max_time = std::numeric_limits<double>::min( );

		auto const total_start = std::chrono::high_resolution_clock::now( );
		std::chrono::duration<double> valid_time = std::chrono::seconds( 0 );
		for( size_t n = 0; n < Runs; ++n ) {
			std::chrono::time_point<std::chrono::high_resolution_clock> start;
			auto result = [&] {
				if constexpr( sizeof...( args ) == 0 ) {
					start = std::chrono::high_resolution_clock::now( );
					return func( );
				} else if constexpr( sizeof...( args ) == 1 ) {
					auto tp_args = std::tuple<
					  std::remove_cv_t<std::remove_reference_t<decltype( args )>>...>{
					  args...};
					daw::do_not_optimize( tp_args );
					start = std::chrono::high_resolution_clock::now( );
					return func( std::get<0>( tp_args ) );
				} else {
					auto tp_args = std::tuple<
					  std::remove_cv_t<std::remove_reference_t<decltype( args )>>...>{
					  args...};
					daw::do_not_optimize( tp_args );
					start = std::chrono::high_resolution_clock::now( );
					return std::apply( func, tp_args );
				}
			}( );
			auto const finish = std::chrono::high_resolution_clock::now( );
			daw::do_not_optimize( result );

			auto const valid_start = std::chrono::high_resolution_clock::now( );
			if( not validator( result ) ) {
				std::cerr << "Error validating result\n" << std::flush;
				throw std::runtime_error( "Error in validating result" );
			}
			valid_time += std::chrono::duration<double>(
			  std::chrono::high_resolution_clock::now( ) - valid_start );

			auto const duration =
			  std::chrono::duration<double>( finish - start ).count( );
			results[n] = duration;
			if( duration < min_time ) {
				min_time = duration;
			}
			if( duration > max_time ) {
				max_time = duration;
			}
		}
		auto const total_finish = std::chrono::high_resolution_clock::now( );
		min_time -= base_time;
		max_time -= base_time;
		auto total_time = std::chrono::duration<double>(
		                    ( total_finish - total_start ) - valid_time )
		                    .count( ) -
		                  static_cast<double>( Runs ) * base_time;
		auto const avg_time = [&]( ) {
			if( Runs >= 10 ) {
				auto result =
				  ( total_time - max_time ) / static_cast<double>( Runs - 1 );
				result -= base_time;
				return result;
			} else {
				auto result = total_time / static_cast<double>( Runs );
				result -= base_time;
				return result;
			}
		}( );

		std::cout << title << delem << "	runs: " << Runs << delem
		          << "	total: " << utility::format_seconds( total_time, 2 )
		          << delem << "	avg: " << utility::format_seconds( avg_time, 2 )
		          << " -> " << utility::to_bytes_per_second( bytes, avg_time, 2 )
		          << "/s" << delem
		          << "	min: " << utility::format_seconds( min_time, 2 ) << " -> "
		          << utility::to_bytes_per_second( bytes, min_time, 2 ) << "/s"
		          << delem << "	max: " << utility::format_seconds( max_time, 2 )
		          << " -> " << utility::to_bytes_per_second( bytes, max_time, 2 )
		          << "/s" << '\n';
		return results;
	}
} // namespace daw
