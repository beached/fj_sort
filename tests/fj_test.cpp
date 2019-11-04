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

#include <cstdint>
#include <random>
#include <utility>
#include <vector>

#include "daw/daw_benchmark.h"
#include "daw/fj_sort.h"

inline constexpr size_t NUM_RUNS = 50U;
#if defined( DEBUG )
inline constexpr size_t MAX_RANGE_SZ = 100'000;
#else
inline constexpr size_t MAX_RANGE_SZ = 25'000'000U;
#endif

template<typename Integer, size_t N>
std::vector<Integer> make_data( ) {
	auto rnd_device = std::random_device( );
	auto eng = std::mt19937( rnd_device( ) );
	auto dist =
	  std::uniform_int_distribution( std::numeric_limits<Integer>::min( ),
	                                 std::numeric_limits<Integer>::max( ) );
	auto result = std::vector<Integer>( );
	result.reserve( N );
	for( size_t n = 0; n < N; ++n ) {
		result.push_back( dist( eng ) );
	}
	return result;
}

struct parallel_sorting {
	template<typename Value>
	auto operator( )( std::vector<Value> &c ) const {
		daw::parallel::fj_sort( c.data( ), c.data( ) + c.size( ) );
		return std::make_pair( c.data( ), c.data( ) + c.size( ) );
	}
};

struct sequential_sorting {
	template<typename Value>
	auto operator( )( std::vector<Value> &c ) const {
		std::sort( c.data( ), c.data( ) + c.size( ) );
		return std::make_pair( c.data( ), c.data( ) + c.size( ) );
	}
};

template<typename Value, typename Sorter>
auto test_sort( std::string const &title, std::vector<Value> const &c,
                Sorter s ) {
	auto const sz = c.size( ) * sizeof( c.front( ) );
	return daw::bench_n_test_mbs2<NUM_RUNS>(
	  title, sz,
	  []( auto const &rng ) { return true; /*return std::is_sorted( rng.first, rng.second );*/ },
	  s, c );
}

template<typename Iterator>
void compare_sorts( size_t N, Iterator first ) {
	using value_type = typename std::iterator_traits<Iterator>::value_type;
	std::cout << "Testing " << N << " items of size " << sizeof( value_type )
	          << ": "
	          << daw::utility::to_bytes_per_second( N * sizeof( value_type ) )
	          << '\n';
	auto data =
	  std::vector<value_type>( first, first + static_cast<ptrdiff_t>( N ) );
	auto seq_res = test_sort( "sequential", data, sequential_sorting{} );

	data = std::vector<value_type>( first, first + static_cast<ptrdiff_t>( N ) );
	auto par_res = test_sort( "parallel", data, parallel_sorting{} );

	auto seq_min = *std::min_element( seq_res.begin( ), seq_res.end( ) );
	auto par_min = *std::min_element( par_res.begin( ), par_res.end( ) );
	std::cout << "Speedup: " << std::fixed << std::setprecision( 2 )
	          << ( seq_min / par_min ) << "\n\n";
}

int main( ) {
#if defined( DEBUG )
	std::cout << "Debug build\n";
#else
	std::cout << "Release build\n";
#endif
	std::cout << "Boost hw concurrency: "
	          << boost::thread::hardware_concurrency( ) << '\n';
	auto const data = make_data<intmax_t, MAX_RANGE_SZ>( );
	for( size_t n = MAX_RANGE_SZ; n >= 1024; n /= 2 ) {
		compare_sorts( n, data.begin( ) );
	}
}
