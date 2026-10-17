#pragma once

// Import is a static library in the current build. Keep its public ABI
// independent from Engine so the importer can later be reused by the editor
// and cooker without pulling the runtime module boundary into the library.
#if defined(IMPORT_DLL_BUILD) && defined(IMPORT_EXPORT)
    #if defined(PITAYA_PLATFORM_WINDOWS)
        #define IMPORT_API __declspec(dllexport)
    #else
        #define IMPORT_API __attribute__((visibility("default")))
    #endif
#elif defined(IMPORT_DLL_BUILD) && defined(PITAYA_PLATFORM_WINDOWS)
    #define IMPORT_API __declspec(dllimport)
#else
    #define IMPORT_API
#endif

#if defined(PITAYA_INTEROP_MODE) && defined(PITAYA_PLATFORM_WINDOWS)
    #define IMPORT_CALL __stdcall
#elif defined(PITAYA_PLATFORM_WINDOWS)
    #define IMPORT_CALL __cdecl
#else
    #define IMPORT_CALL
#endif
