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

#include <utility>

namespace daw {
	template<typename T>
	class mutable_capture {
		mutable T m_value;

	public:
		explicit constexpr mutable_capture( T const &value )
		  : m_value( value ) {}
		explicit constexpr mutable_capture( T &value )
		  : m_value( value ) {}
		explicit constexpr mutable_capture( T &&value )
		  : m_value( std::move( value ) ) {}

		constexpr operator T &( ) const &noexcept {
			return m_value;
		}

		constexpr operator T( ) const &&noexcept {
			return std::move( m_value );
		}

		[[nodiscard]] constexpr T &operator*( ) const &noexcept {
			return m_value;
		}

		[[nodiscard]] constexpr T operator*( ) const &&noexcept {
			return std::move( m_value );
		}

		[[nodiscard]] constexpr T *operator->( ) const noexcept {
			return &m_value;
		}
	};

	template<typename T>
	mutable_capture( T )->mutable_capture<T>;
} // namespace daw
