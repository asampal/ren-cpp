#include "rencpp/value.hpp"
#include "rencpp/strings.hpp"

#include "rencpp/red.hpp"

#define UNUSED(x) static_cast<void>(x)

namespace ren {

///
/// TYPE CHECKING AND INITIALIZATION
///

bool AnyValue::isString(RedCell * init) const {
    if (init) {
        init->header = RedRuntime::TYPE_STRING;
        return true;
    }
    return RedRuntime::getDatatypeID(this->cell) == RedRuntime::TYPE_STRING;
}


bool AnyValue::isTag(RenCell *) const {
    throw std::runtime_error("tag not implemented");
}


bool AnyValue::isFilename(RenCell *) const {
    throw std::runtime_error("file not implemented");
}


bool AnyValue::isAnyString() const {
    switch (RedRuntime::getDatatypeID(this->cell)) {
        case RedRuntime::TYPE_STRING:
        case RedRuntime::TYPE_FILE:
        case RedRuntime::TYPE_URL:
            return true;
        default:
            break;
    }
    return false;
}



///
/// CONSTRUCTION
///

AnyString::AnyString(
    char const * cstr,
    internal::CellFunction cellfun,
    Engine * engine
) :
    Series (Dont::Initialize)
{
    throw std::runtime_error("AnyString::AnyString coming soon...");

    UNUSED(cstr);
    UNUSED(cellfun);
    UNUSED(engine);
}


#if REN_CLASSLIB_QT == 1

AnyString::AnyString (
    QString const & spelling,
    internal::CellFunction cellfun,
    Engine * engine
)
    : Series(Dont::Initialize)
{
    throw std::runtime_error("AnyString::AnyString coming soon...");

    UNUSED(spelling);
    UNUSED(cellfun);
    UNUSED(engine);
}

#endif


} // end namespace ren
