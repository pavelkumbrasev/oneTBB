/*
    Copyright (c) 2005-2020 Intel Corporation

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

        http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
*/

#include "common/test.h"
#include "common/utils.h"
#include "common/utils_assert.h"
#include "common/utils_concurrency_limit.h"

#include "tbb/blocked_range.h"
#include "tbb/parallel_for.h"
#include "tbb/global_control.h"

#include <vector>

//! \file conformance_blocked_range.cpp
//! \brief Test for [algorithms.blocked_range] specification

class AbstractValueType {
    AbstractValueType() {}
    int value;
public:
    friend AbstractValueType MakeAbstractValueType( int i );
    friend int GetValueOf( const AbstractValueType& v ) {return v.value;}
};

AbstractValueType MakeAbstractValueType( int i ) {
    AbstractValueType x;
    x.value = i;
    return x;
}

std::size_t operator-( const AbstractValueType& u, const AbstractValueType& v ) {
    return GetValueOf(u) - GetValueOf(v);
}

bool operator<( const AbstractValueType& u, const AbstractValueType& v ) {
    return GetValueOf(u) < GetValueOf(v);
}

AbstractValueType operator+( const AbstractValueType& u, std::size_t offset ) {
    return MakeAbstractValueType(GetValueOf(u) + int(offset));
}

static void SerialTest() {
    for( int x=-10; x<10; ++x ) {
        for( int y=-10; y<10; ++y ) {
            AbstractValueType i = MakeAbstractValueType(x);
            AbstractValueType j = MakeAbstractValueType(y);
            for( std::size_t k=1; k<10; ++k ) {
                typedef tbb::blocked_range<AbstractValueType> range_type;
                range_type r( i, j, k );
                utils::AssertSameType( r.empty(), true );
                utils::AssertSameType( range_type::size_type(), std::size_t() );
                utils::AssertSameType( static_cast<range_type::const_iterator*>(0), static_cast<AbstractValueType*>(0) );
                utils::AssertSameType( r.begin(), MakeAbstractValueType(0) );
                utils::AssertSameType( r.end(), MakeAbstractValueType(0) );
                CHECK( r.empty()==(y<=x));
                CHECK( r.grainsize()==k);
                if( x<=y ) {
                    utils::AssertSameType( r.is_divisible(), true );
                    CHECK( r.is_divisible()==(std::size_t(y-x)>k) );
                    CHECK( r.size()==std::size_t(y-x) );
                    if( r.is_divisible() ) {
                        tbb::blocked_range<AbstractValueType> r2(r,tbb::split());
                        CHECK( GetValueOf(r.begin())==x );
                        CHECK( GetValueOf(r.end())==GetValueOf(r2.begin()) );
                        CHECK( GetValueOf(r2.end())==y );
                        CHECK( r.grainsize()==k );
                        CHECK( r2.grainsize()==k );
                    }
                }
            }
        }
    }
}

const int N = 1<<22;
unsigned char Array[N];

struct Striker {
    void operator()( const tbb::blocked_range<int>& r ) const {
        for( tbb::blocked_range<int>::const_iterator i=r.begin(); i!=r.end(); ++i )
            ++Array[i];
    }
};

void ParallelTest() {
    for (int i=0; i<N; i=i<3 ? i+1 : i*3) {
        const tbb::blocked_range<int> r( 0, i, 10 );
        tbb::parallel_for( r, Striker() );
        for (int k=0; k<N; ++k) {
            if (Array[k] != (k<i)) CHECK(false);
            Array[k] = 0;
        }
    }
}

//! Testing blocked_range interface
//! \brief \ref interface \ref requirement
TEST_CASE("Basic serial") {
    SerialTest();
}

//! Testing blocked_range interface with parallel_for
//! \brief \ref requirement
TEST_CASE("Basic parallel") {
    for ( auto concurrency_level : utils::concurrency_range() ) {
        tbb::global_control control(tbb::global_control::max_allowed_parallelism, concurrency_level);
        ParallelTest();
    }
}

//! Testing blocked_range with proportional splitting
//! \brief \ref interface \ref requirement
TEST_CASE("blocked_range proportional splitting") {
    tbb::blocked_range<int> original(0, 100);
    tbb::blocked_range<int> first(original);
    tbb::proportional_split ps(3, 1);
    tbb::blocked_range<int> second(first, ps);

    // Test proportional_split -> split conversion
    tbb::blocked_range<int> copy(original);
    tbb::split s = tbb::split(ps);
    tbb::blocked_range<int> splitted_copy(copy, s);
    CHECK(copy.size() == original.size() / 2);
    CHECK(splitted_copy.size() == copy.size());


    int expected_first_end = original.begin() + ps.left() * (original.end() - original.begin()) / (ps.left() + ps.right());
    utils::check_range_bounds_after_splitting(original, first, second, expected_first_end);
}

#if __TBB_CPP17_DEDUCTION_GUIDES_PRESENT
//! Testing blocked_range deduction guides
//! \brief \ref interface
TEST_CASE("Deduction guides") {
    std::vector<const int *> v;

    // check blocked_range(Value, Value, size_t)
    tbb::blocked_range r1(v.begin(), v.end());
    static_assert(std::is_same<decltype(r1), tbb::blocked_range<decltype(v)::iterator>>::value);

    // check blocked_range(blocked_range &)
    tbb::blocked_range r2(r1);
    static_assert(std::is_same<decltype(r2), decltype(r1)>::value);

    // check blocked_range(blocked_range &&)
    tbb::blocked_range r3(std::move(r1));
    static_assert(std::is_same<decltype(r3), decltype(r1)>::value);
}
#endif

