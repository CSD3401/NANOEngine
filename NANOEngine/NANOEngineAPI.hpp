#ifndef NANOENGINE_API_HPP
#define NANOENGINE_API_HPP

#if defined(_WIN32) || defined(_WIN64)
#ifdef NANOENGINE_EXPORTS
// When building the DLL, define ENGINECORE_EXPORTS in your project settings.
#define NANOENGINE_API __declspec(dllexport)
#else
#define NANOENGINE_API __declspec(dllimport)
#endif
#else
// On non-Windows platforms, this can be left empty.
#define NANOENGINE_API
#endif

#endif // NANOENGINE_API_HPP