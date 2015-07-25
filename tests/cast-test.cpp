#include <iostream>
#include <cassert>

#include "rencpp/ren.hpp"

using namespace ren;

#include "catch.hpp"

TEST_CASE("cast test", "[rebol] [cast]")
{

    SECTION("integer cast")
    {
        Value someIntAsValue = 10;
        CHECK(someIntAsValue.isInteger());

        Integer someInt = static_cast<Integer>(someIntAsValue);

        CHECK(someIntAsValue.isEqualTo(someInt));
        CHECK(static_cast<int>(someInt) == 10);

        Value someIntConstValue = someIntAsValue;
        Integer anotherInt = static_cast<Integer>(someIntConstValue);
    }


    SECTION("integer to float cast")
    {
        Value value = Integer {20};

        // You can't treat the bits of an Integer as Float in the Rebol or
        // Red world...C++ *could* have a cast operator for it.  See:
        //
        //     https://github.com/hostilefork/rencpp/issues/7
        CHECK_THROWS_AS(
            static_cast<Float>(value),
            std::bad_cast
        );
    }
}
