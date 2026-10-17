#pragma once

// Import 当前是静态库，不能依赖 Engine 的导出宏。
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
