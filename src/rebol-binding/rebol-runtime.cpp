#include <iostream>
#include <stdexcept>

#include <csignal>
#include <unistd.h>

#include "rencpp/engine.hpp"
#include "rencpp/rebol.hpp"
#include "rencpp/blocks.hpp"

extern "C" {
#include "rebol/src/include/reb-ext.h"
#include "rebol/src/include/reb-lib.h"
    extern REBOL_HOST_LIB Host_Lib_Init_RenCpp;

#ifdef TO_WIN32
    #include <windows.h>
    // The objects file from Rebol linked into RenCpp need a
    // variable named App_Instance for the linkage to work when
    // built for Windows. Therefore, we provided this variable
    // here. It does not serve any other purpose.
    HINSTANCE App_Instance = 0;
#endif
}

#ifndef MAX_PATH
#define MAX_PATH 4096  // from host-lib.c, generally lacking in Posix
#endif

namespace ren {


RebolRuntime runtime {true};



bool Runtime::needsRefcount(REBVAL const & cell) {
    // actually need to include ANY_OBJECT as well, but since the hooks
    // from Rebol to actually call and ask us what we know about garbage
    // collection aren't in yet... leaving it at series for the moment

    return ANY_SERIES(&cell);
}



///
/// REBOL INITIALIZATION
///

//
// The code in the initialization is an unwinding of the RL_init code from
// a-lib.c (which we do not include in the binding)
//
// We don't call Init_Core until a lazy opportunity on the first creation
// of an environment.  This is because we want client applications to have
// a simple interface by default, yet be able to pass in parameters to an
// initializer if needed.  (For instance, passing in the working directory,
// if they want the bound program to know about that kind of thing.)
//


RebolRuntime::RebolRuntime (bool) :
    Runtime (),
    initialized (false)
{
    Host_Lib = &Host_Lib_Init_RenCpp; // OS host library (dispatch table)

    // We don't want to rewrite the entire host lib here, but we can
    // hook functions in.  It's good to have a debug hook point here
    // for when things crash, rather than sending to the default
    // host lib implementation.

    Host_Lib->os_crash =
        [](REBYTE const * title, REBYTE const * content) {
            // This is our only error hook for certain types of "crashes"
            // (evaluation_error is caught elsewhere).  If we want to
            // break crashes down more specifically (without touching Rebol
            // source) we'd have to parse the error strings to translate them
            // into exception classes.  Left as an exercise for the reader

            throw std::runtime_error(
                std::string(reinterpret_cast<char const *>(title))
                + " : " + reinterpret_cast<char const *>(content)
            );
        };

    // = {0} on construct should zero fill, but gives warnings in older gcc
    // http://stackoverflow.com/questions/13238635/#comment37115390_13238635

    rebargs.options = 0;
    rebargs.script = nullptr;
    rebargs.args = nullptr;
    rebargs.do_arg = nullptr;
    rebargs.version = nullptr;
    rebargs.debug = nullptr;
    rebargs.import = nullptr;
    rebargs.secure = nullptr;
    rebargs.boot = nullptr;
    rebargs.exe_path = nullptr;
    rebargs.home_dir = nullptr;

    // Rebytes, version numbers
    // REBOL_VER
    // REBOL_REV
    // REBOL_UPD
    // REBOL_SYS
    // REBOL_VAR
}



REBVAL RebolRuntime::loadAndBindWord(
    REBSER * context,
    const char * cstrUtf8,
    REBOL_Types kind,
    size_t len
) {
    REBVAL result;

    // Set_Word sets the fields of an ANY_WORD cell, but doesn't set
    // the header bits.

    Set_Word(
        &result,
        // Make_Word has a misleading name; it just finds the symbol
        // number (a REBINT) and the rest of this is needed in order
        // to get to a well formed word.
        Make_Word(
            reinterpret_cast<REBYTE*>(const_cast<char*>(cstrUtf8)),
            len == 0 ? strlen(cstrUtf8) : len
        ),
        // Initialize FRAME to null
        nullptr,
        0
    );

    // Set the "cell type" and clear the other header flags
    // (SET_TYPE would leave the other header flags alone, but they
    // are still uninitialized data at this point)

    VAL_SET(&result, kind);

    // The word is now well formed, but unbound.  If you supplied a
    // context we will bind it here.

    if (context)
        Bind_Word(context, &result);

    // Note that with C++11 move semantics, this is constructed in place;
    // in other words, the caller's REBVAL that they process in the return
    // was always the memory "result" used.  Quick evangelism note is that
    // this means objects can *contain* pointers and manage the lifetime
    // while not costing more to pass around.  If you want value semantics
    // you just use them.

    return result;
}

bool RebolRuntime::lazyInitializeIfNecessary() {
    if (initialized)
        return false;

    #ifdef OS_STACK_GROWS_UP
        Stack_Limit = static_cast<void*>(-1);
    #else
        Stack_Limit = 0;
    #endif


    // Parse_Args has a memory leak; it uses OS_Get_Current_Dir which
    // mallocs a string which is never freed.  We hold onto the REBARGS
    // we use and control the initialization so we don't leak strings

    // There are (R)ebol (O)ption flags telling it various things,
    // in rebargs.options, we just don't want it to display the banner

    rebargs.options = RO_QUIET;

    // Theoretically we could offer hooks for this deferred initialization
    // to pass something here, or even change it while running.  Right
    // now for exe_path we offer an informative fake path that is (hopefully)
    // harmless.

    rebargs.home_dir = new REBCHR[MAX_PATH];

#ifdef TO_WIN32
    rebargs.exe_path = reinterpret_cast<REBCHR *>(const_cast<wchar_t *>(
        L"/dev/null/rencpp-binding/look-at/rebol-hooks.cpp"
    ));

    GetCurrentDirectory(MAX_PATH, reinterpret_cast<TCHAR *>(rebargs.home_dir));
#else
    rebargs.exe_path = reinterpret_cast<REBCHR *>(const_cast<char*>(
        "/dev/null/rencpp-binding/look-at/rebol-hooks.cpp"
    ));

    getcwd(reinterpret_cast<char *>(rebargs.home_dir), MAX_PATH);
#endif

    Init_Core(&rebargs);

    GC_Active = TRUE; // Turn on GC

    if (rebargs.options & RO_TRACE) {
        Trace_Level = 9999;
        Trace_Flags = 1;
    }


    // Set up the interrupt handler for things like Ctrl-C (which had
    // previously been stuck in with stdio, because that's where the signals
    // were coming from...like Ctrl-C at the keyboard).  Rencpp is more
    // general, and if you are performing an evaluation in a GUI and want
    // to handle a Ctrl-C on the gui thread during an infinite loop you need
    // to be doing the evaluation on a worker thread and signal it from GUI

    auto signalHandler = [](int) {
        Engine::runFinder().getOutputStream() << "[escape]";
        SET_SIGNAL(SIG_ESCAPE);
    };

    signal(SIGINT, signalHandler);
#ifdef SIGHUP
    signal(SIGHUP, signalHandler);
#endif
    signal(SIGTERM, signalHandler);

    // Initialize the REBOL library (reb-lib):
    if (not CHECK_STRUCT_ALIGN)
        throw std::runtime_error(
            "RebolHooks: Incompatible struct alignment..."
            " Did you build mainline Rebol on 64-bit instead of with -m32?"
            " (for 64-bit builds, use http://github.com/rebolsource/r3)"
        );


    // bin is optional startup code (compressed).  If it is provided, it
    // will be stored in system/options/boot-host, loaded, and evaluated.

    REBYTE * startupBin = nullptr;
    REBINT len = 0; // length of above bin

    if (startupBin) {
        REBSER spec;
        spec.data = startupBin;
        spec.tail = len;
        spec.rest = 0;
        spec.info = 0;
        spec.size = 0;

        REBSER * startup = Decompress(&spec, 0, -1, 10000000, 0);

        if (not startup)
            throw std::runtime_error("RebolHooks: Bad startup code");;

        Set_Binary(BLK_SKIP(Sys_Context, SYS_CTX_BOOT_HOST), startup);
    }

    if (Init_Mezz(0) != 0)
        throw std::runtime_error("RebolHooks: Mezzanine startup failure");

    // There is an unfortunate property of QUIT which is that it calls
    // Halt_Code, which uses a jump buffer Halt_State which is static to
    // c-do.c - hence we cannot catch QUIT.  Unless...we rewrite the value
    // it is set to, to use a "fake quit" that shares the jump buffer we
    // use for execution...

    REBVAL quitWord = loadAndBindWord(Lib_Context, "quit");

    REBVAL quitNative = *Get_Var(&quitWord);

    if (not IS_NATIVE(&quitNative)) {

        // If you look at sys-value.h and check out the definition of
        // REBHDR, you will see that the order of fields depends on the
        // endianness of the platform.  So if the C build and the C++
        // build do not have the same #define for ENDIAN_LITTLE then they
        // will wind up disagreeing.  You might have successfully loaded
        // a function but have the bits scrambled.  So before telling you
        // we can't find "load" in the Mezzanine, we look for the
        // scrambling in question and tell you that's what the problem is

        if (quitNative.flags.flags.resv == REB_NATIVE) {
            throw std::runtime_error(
                "Bit field order swap detected..."
                " Did you compile Rebol with a different setting for"
                " ENDIAN_LITTLE?  Check reb-config.h"
            );
        }

        // If that wasn't the problem, it was something else.

        throw std::runtime_error(
            "Couldn't get QUIT native for unknown reason"
        );
    }

    // Tweak the bits to put our fake quit function in and save it back...
    // This worked for QUIT but unfortunately couldn't work for Escape/CtrlC
    // Had to extract c-do.c => c-do.cpp

/*    quitNative.data.func.func.code = &internal::Fake_Quit;
    Set_Var(&quitWord, &quitNative); */

    initialized = true;
    return true;
}



void RebolRuntime::doMagicOnlyRebolCanDo() {
   std::cout << "REBOL MAGIC!\n";
}


void RebolRuntime::cancel() {
    SET_SIGNAL(SIG_ESCAPE);
}


RebolRuntime::~RebolRuntime () {
    OS_QUIT_DEVICES(0);

    delete [] rebargs.home_dir; // needs to last during Rebol run
}

} // end namespace ren


