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

#define BOOST_THREAD_PROVIDES_FUTURE
#define BOOST_THREAD_PROVIDES_FUTURE_CONTINUATION

#include <boost/thread/future.hpp>
#include <cassert>
#include <optional>
#include <thread>
#include <vector>

#include "daw_mutable_capture.h"

namespace daw::parallel {
	namespace impl {
		template<typename Iterator>
		struct span {
			using value_type = typename std::iterator_traits<Iterator>::value_type;
			using reference = value_type &;
			using pointer = value_type *;
			using iterator = Iterator;

			iterator first;
			iterator last;

			constexpr span( iterator f, iterator l )
			  : first( std::move( f ) )
			  , last( std::move( l ) ) {}

			constexpr iterator begin( ) const {
				return first;
			}

			constexpr iterator end( ) const {
				return last;
			}
		};

		template<typename Iterator>
		span( Iterator )->span<Iterator>;

		template<typename Iterator>
		std::vector<span<Iterator>> partition_range( Iterator first,
		                                             Iterator last ) {
			// Partition
			auto const range_sz = std::distance( first, last );
			auto const part_sz = range_sz / static_cast<ptrdiff_t>(
			                                  std::thread::hardware_concurrency( ) );
			auto result = std::vector<span<Iterator>>( );
			ptrdiff_t start_pos = 0;
			for( ; start_pos < range_sz; start_pos += part_sz ) {
				auto start_it = std::next( first, start_pos );
				result.emplace_back( start_it, std::next( start_it, part_sz ) );
			}
			if( start_pos < range_sz ) {
				auto start_it = std::next( first, start_pos );
				result.emplace_back( start_it,
				                     std::next( start_it, range_sz - start_pos ) );
			}
			return result;
		}

		template<typename Iterator, typename OutputIterator, typename BinaryOp>
		void reduce_futures2( Iterator first, Iterator last, OutputIterator out_it,
		                      BinaryOp const &binary_op ) noexcept {

			ptrdiff_t range_sz = std::distance( first, last );
			if( range_sz == 1 ) {
				*out_it++ = std::move( *first++ );
				(void)out_it;
				return;
			}
			bool const odd_count = range_sz % 2 == 1;
			if( odd_count ) {
				last = std::next( first, range_sz - 1 );
			}
			while( first != last ) {
				auto l_it = first++;
				auto r_it = first++;

				*out_it++ = l_it->then( [r = daw::mutable_capture( std::move( *r_it ) ),
				                         binary_op]( auto &&result ) noexcept {
					using arg_t = decltype( result );
					return binary_op( std::forward<arg_t>( result ).get( ),
					                  std::move( r )->get( ) );
				} );
			}
			if( odd_count ) {
				*out_it++ = std::move( *last );
				(void)out_it;
			}
		}

		template<typename Iterator>
		using fut_red_result_t =
		  typename std::iterator_traits<Iterator>::value_type;

		template<typename Iterator, typename BinaryOp>
		auto reduce_futures( Iterator first, Iterator last,
		                     BinaryOp binary_op ) noexcept {

			using value_type = typename std::iterator_traits<Iterator>::value_type;
			auto results = std::vector<value_type>( );
			results.reserve( static_cast<size_t>( std::distance( first, last ) )/2U );

			impl::reduce_futures2( first, last, std::back_inserter( results ),
			                       binary_op );

			auto tmp = std::vector<value_type>( );
			tmp.reserve( results.size( ) / 2U );
			while( results.size( ) > 1 ) {
				tmp.clear( );

				impl::reduce_futures2( results.begin( ), results.end( ),
				                       std::back_inserter( tmp ), binary_op );
				std::swap( results, tmp );
			}
			return std::move( results.front( ) );
		}

		template<typename Compare>
		struct parallel_sort_merger {
			Compare cmp;

			template<typename Iterator>
			constexpr span<Iterator> operator( )( span<Iterator> l,
			                                      span<Iterator> r ) const noexcept {

				// We must be contiguous
				assert( l.end( ) == r.begin( ) );

				std::inplace_merge( l.begin( ), l.end( ), r.end( ), cmp );

				return span<Iterator>( l.begin( ), r.end( ) );
			}
		};

		template<typename C>
		parallel_sort_merger( C )->parallel_sort_merger<C>;
	} // namespace impl

	template<typename Iterator, typename Compare = std::less<>>
	void fj_sort( Iterator first, Iterator last,
	              Compare comp = Compare{} ) noexcept {

		// Allow for later on swappng with items like stable_sort
		constexpr auto sorter = []( auto f, auto l, auto cmp ) noexcept {
			std::sort( f, l, cmp );
		};

		std::vector<impl::span<Iterator>> ranges =
		  impl::partition_range( first, last );
		auto sorters = std::vector<boost::future<impl::span<Iterator>>>( );

		auto const sort_fn =
		  [sorter,
		   comp]( impl::span<Iterator> rng ) noexcept -> impl::span<Iterator> {
			sorter( rng.begin( ), rng.end( ), comp );
			return rng;
		};

		for( auto rng : ranges ) {
			sorters.push_back(
			  boost::async( boost::launch::async,
			                [rng = daw::mutable_capture( rng ), sort_fn]( ) noexcept
			                -> impl::span<Iterator> { return sort_fn( *rng ); } ) );
		}

		impl::reduce_futures( sorters.begin( ), sorters.end( ),
		                      impl::parallel_sort_merger{comp} )
		  .wait( );
	}
} // namespace daw::parallel
