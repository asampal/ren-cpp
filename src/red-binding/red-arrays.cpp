#include "rencpp/value.hpp"
#include "rencpp/arrays.hpp"

#include "rencpp/red.hpp"


#define UNUSED(x) static_cast<void>(x)


namespace ren {

///
/// TYPE CHECKING AND INITIALIZATION
///

bool AnyValue::isBlock(RedCell * init) const {
    if (init) {
        init->header = RedRuntime::TYPE_BLOCK;
        return true;
    }
    return RedRuntime::getDatatypeID(this->cell) == RedRuntime::TYPE_BLOCK;
}


bool AnyValue::isGroup(RedCell * init) const {
    if (init) {
        init->header = RedRuntime::TYPE_PAREN;
        return true;
    }
    return RedRuntime::getDatatypeID(this->cell) == RedRuntime::TYPE_PAREN;
}


bool AnyValue::isPath(RedCell * init) const {
    if (init) {
        init->header = RedRuntime::TYPE_PATH;
        return true;
    }
    return RedRuntime::getDatatypeID(this->cell) == RedRuntime::TYPE_PATH;
}


bool AnyValue::isGetPath(RedCell * init) const {
    if (init) {
        init->header = RedRuntime::TYPE_GET_PATH;
        return true;
    }
    return RedRuntime::getDatatypeID(this->cell) == RedRuntime::TYPE_GET_PATH;
}


bool AnyValue::isSetPath(RedCell * init) const {
    if (init) {
        init->header = RedRuntime::TYPE_SET_PATH;
        return true;
    }
    return RedRuntime::getDatatypeID(this->cell) == RedRuntime::TYPE_SET_PATH;
}


bool AnyValue::isLitPath(RedCell * init) const {
    if (init) {
        init->header = RedRuntime::TYPE_LIT_PATH;
        return true;
    }
    return RedRuntime::getDatatypeID(this->cell) == RedRuntime::TYPE_LIT_PATH;
}


bool AnyValue::isAnyArray() const {
    switch (RedRuntime::getDatatypeID(this->cell)) {
        case RedRuntime::TYPE_BLOCK:
        case RedRuntime::TYPE_PAREN:
        case RedRuntime::TYPE_PATH:
        case RedRuntime::TYPE_SET_PATH:
        case RedRuntime::TYPE_GET_PATH:
        case RedRuntime::TYPE_LIT_PATH:
            return true;
        default:
            break;
    }
    return false;
}



///
/// CONSTRUCTION
///

AnyArray::AnyArray (
    internal::Loadable const loadables[],
    size_t numLoadables,
    internal::CellFunction cellfun,
    Context const * contextPtr,
    Engine * engine
) :
    Series (Dont::Initialize)
{
    throw std::runtime_error("AnyArray::AnyArray coming soon...");

    UNUSED(loadables);
    UNUSED(numLoadables);
    UNUSED(cellfun);
    UNUSED(contextPtr);
    UNUSED(engine);
}


} // end namespace ren
