#ifndef RENCPP_BLOCKS_HPP
#define RENCPP_BLOCKS_HPP

//
// arrays.hpp
// This file is part of RenCpp
// Copyright (C) 2015 HostileFork.com
//
// Licensed under the Boost License, Version 1.0 (the "License")
//
//      http://www.boost.org/LICENSE_1_0.txt
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
// implied.  See the License for the specific language governing
// permissions and limitations under the License.
//
// See http://rencpp.hostilefork.com for more information on this project
//

#include "value.hpp"
#include "series.hpp"

namespace ren {

class AnyArray : public Series {
protected:
    friend class AnyValue;
    AnyArray (Dont) noexcept : Series (Dont::Initialize) {}
    inline bool isValid() const { return isAnyArray(); }

protected:
    //
    // Provide a helper to the derived classes to construct AnyArray
    // instances through the binding.  But you can't construct an "AnyArray"
    // instance without a block type... it's abstract!  No such thing.
    //
    // For the curious: empty constructor has to result in an empty block.
    // No way to distinguish from the default constructor:
    //
    //     http://stackoverflow.com/a/9020606/211160
    //

    AnyArray (
        internal::Loadable const loadables[],
        size_t numLoadables,
        internal::CellFunction cellfun,
        Context const * contextPtr,
        Engine * engine
    );

    AnyArray (
        AnyValue const values[],
        size_t numValues,
        internal::CellFunction cellfun,
        Context const * contextPtr,
        Engine * engine
    );
};



namespace internal {

//
// ANYARRAY_ SUBTYPE HELPER
//

//
// BracesT corresponds to the kind of type which is constructed
// when curly braces are encountered in the initialization of a
// block type. Let's consider this example:
//
// Foo foo { 1, { true, false }, {} };
//
// If BracesT is void, you will get a compile time error. But if
// BracesT is Foo, the example will be equivalent to:
//
// Foo foo { 1, Foo { true, false }, Foo {} };
//
// On the other hand, if BracesT is Block, then the example will
// be equivalent to:
//
// Foo foo { 1, Block { true, false }, Block {} };
//
// When untyped braces are encountered, they follow the rules of
// the closest block type. Let's consider another example:
//
// Foo foo { {}, Block { {}, {} }, {} };
//           #1          #2  #3    #4
//
// In this example, #1 and #4 will follow the bracing rules of
// Foo while #2 and #3 will follow the bracing rules of Block.
//

template <class C, CellFunction F, typename BracesT=void>
class AnyArray_ : public AnyArray {
protected:
    friend class AnyValue;
    AnyArray_ (Dont) : AnyArray (Dont::Initialize) {}
    inline bool isValid() const { return (this->*F)(nullptr); }

public:
    AnyArray_ (
        AnyValue const values[],
        size_t numValues,
        internal::ContextWrapper const & wrapper
    ) :
        AnyArray (values, numValues, F, &wrapper.context, nullptr)
    {
    }

    AnyArray_ (
        AnyValue const values[],
        size_t numValues,
        Engine * engine
    ) :
        AnyArray (values, numValues, F, nullptr, engine)
    {
    }

    AnyArray_ (
        std::initializer_list<BlockLoadable<BracesT>> const & loadables,
        internal::ContextWrapper const & wrapper
    ) :
        AnyArray (
            loadables.begin(),
            loadables.size(),
            F,
            &wrapper.context,
            nullptr
        )
    {
    }

    AnyArray_ (Context const & context) :
        AnyArray (static_cast<Loadable *>(nullptr), 0, F, &context, nullptr)
    {
    }

    AnyArray_ (
        std::initializer_list<BlockLoadable<BracesT>> const & loadables,
        Engine * engine = nullptr
    ) :
        AnyArray (loadables.begin(), loadables.size(), F, nullptr, engine)
    {
    }

    AnyArray_ (Engine * engine = nullptr) :
        AnyArray (static_cast<Loadable *>(nullptr), 0, F, nullptr, engine)
    {
    }

    // A block can be invoked something like a function via DO, so it makes
    // sense for it to have a way of applying it...but it doesn't take
    // any "parameters"
#ifdef REN_RUNTIME
public:
    inline AnyValue operator()() const {
        return apply();
    }
#endif
};

} // end namespace internal



//
// CONCRETE BLOCK TYPES
//

//
// For why these are classes and not typedefs:
//
//     https://github.com/hostilefork/rencpp/issues/49
//


class Block : public internal::AnyArray_<Block, &AnyValue::isBlock, Block>
{
public:
    friend class AnyValue;
    using internal::AnyArray_<Block, &AnyValue::isBlock, Block>::AnyArray_;
};


class Group : public internal::AnyArray_<Group, &AnyValue::isGroup>
{
public:
    friend class AnyValue;
	using internal::AnyArray_<Group, &AnyValue::isGroup>::AnyArray_;
};


class Path : public internal::AnyArray_<Path, &AnyValue::isPath>
{
public:
    friend class AnyValue;
    using internal::AnyArray_<Path, &AnyValue::isPath>::AnyArray_;

public:
    template <typename... Ts>
    inline optional<AnyValue> operator()(Ts &&... args) const {
        return apply(std::forward<Ts>(args)...);
    }
};


class SetPath : public internal::AnyArray_<SetPath, &AnyValue::isSetPath>
{
public:
    friend class AnyValue;
    using internal::AnyArray_<SetPath, &AnyValue::isSetPath>::AnyArray_;

public:
    template <typename... Ts>
    inline AnyValue operator()(Ts &&... args) const {
        // An expression like `x/y/z: (...)` cannot give back a non set
        // result, it would generate an error first.
        return *apply(std::forward<Ts>(args)...);
    }
};


class GetPath : public internal::AnyArray_<GetPath, &AnyValue::isGetPath>
{
public:
    friend class AnyValue;
    using internal::AnyArray_<GetPath, &AnyValue::isGetPath>::AnyArray_;

    // As with GetWord, it can be convenient to think of "using" a GetPath
    // as calling a function with no arguments.
#ifdef REN_RUNTIME
public:
    template <typename... Ts>
    inline optional<AnyValue> operator()() const {
        return apply();
    }
#endif
};


class LitPath : public internal::AnyArray_<LitPath, &AnyValue::isLitPath>
{
public:
    friend class AnyValue;
    using internal::AnyArray_<LitPath, &AnyValue::isLitPath>::AnyArray_;
};


} // end namespace ren

#endif
