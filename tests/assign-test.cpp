#include <iostream>

#include "rencpp/ren.hpp"

using namespace ren;

#include "catch.hpp"

TEST_CASE("assign test", "[rebol] [assign]")
{
    Integer someInt {10};
    AnyValue someValue;

    someValue = someInt;

    Block someBlock {10, "foo"};

    Block someOtherBlock {20, "bar"};

    someBlock = someOtherBlock;

    CHECK(someBlock.isEqualTo(someOtherBlock));
}
