#pragma once
#include <memory>

#define VA_LIST(...) __VA_ARGS__

#define STRINGIFY(x) STRINGIFY2(x)
#define STRINGIFY2(x) #x
#define __LINESTRING__ STRINGIFY(__LINE__)

#define ADD_FEATURE_CUSTOM(type, name, scope) namespace scope { inline type name; }
#define ADD_FEATURE(type, name) ADD_FEATURE_CUSTOM(type, name, F)

// MAKE_UNIQUE - declares a std::unique_ptr<type> singleton with the given name.
// Ownership is managed by the unique_ptr; use name->Method() to access the instance.
// Prefer this over raw global objects when heap allocation is acceptable.
#define MAKE_UNIQUE(type, name) inline std::unique_ptr<type> name = std::make_unique<type>()

#define Assert(cond) if (!cond) { MessageBox(0, #cond "\n\n" __FUNCSIG__ " Line " __LINESTRING__, "Error", MB_OK | MB_ICONERROR); }
#define AssertFatal(cond) if (!cond) { MessageBox(0, #cond "\n\n" __FUNCSIG__ " Line " __LINESTRING__, "Error", MB_OK | MB_ICONERROR); exit(EXIT_FAILURE); }
#define AssertCustom(cond, message) if (!cond) { MessageBox(0, message, "Error", MB_OK | MB_ICONERROR); }