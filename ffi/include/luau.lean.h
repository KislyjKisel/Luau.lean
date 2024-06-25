#include <lean/lean.h>
#include <lean_pod.h>

// #include <luacode.h>
// Copy pasted and changed because of a syntax error (= nullptr in struct invalid in C)

// "This file is part of the Luau programming language and is licensed under MIT License; see LICENSE.txt for details"

typedef struct lua_CompileOptions lua_CompileOptions;

struct lua_CompileOptions
{
    // 0 - no optimization
    // 1 - baseline optimization level that doesn't prevent debuggability
    // 2 - includes optimizations that harm debuggability such as inlining
    int optimizationLevel; // default=1

    // 0 - no debugging support
    // 1 - line info & function names only; sufficient for backtraces
    // 2 - full debug info with local & upvalue names; necessary for debugger
    int debugLevel; // default=1

    // type information is used to guide native code generation decisions
    // information includes testable types for function arguments, locals, upvalues and some temporaries
    // 0 - generate for native modules
    // 1 - generate for all modules
    int typeInfoLevel; // default=0

    // 0 - no code coverage support
    // 1 - statement coverage
    // 2 - statement and expression coverage (verbose)
    int coverageLevel; // default=0

    // global builtin to construct vectors; disabled by default
    const char* vectorLib;
    const char* vectorCtor;

    // vector type name for type tables; disabled by default
    const char* vectorType;

    // null-terminated array of globals that are mutable; disables the import optimization for fields accessed through these
    const char* const* mutableGlobals;

    // null-terminated array of userdata types that will be included in the type information
    const char* const* userdataTypes;
};

// compile source to bytecode; when source compilation fails, the resulting bytecode contains the encoded error. use free() to destroy
char* luau_compile(const char* source, size_t size, lua_CompileOptions* options, size_t* outsize);

// End of copy pasted segment

typedef struct {
    lua_CompileOptions options;
    lean_object* owner;
} lean_luau_CompileOptions_data;

LEAN_POD_DECLARE_EXTERNAL_CLASS(luau_CompileOptions, lean_luau_CompileOptions_data*)

// Lean-side CompileOptions
#define LEAN_LUAU_LAYOUT_CompileOptions 2, 0, 0, 0, 0
#define LEAN_LUAU_CompileOptions_OPTLVL(v) LEAN_POD_CTOR_GET_U8(LEAN_LUAU_LAYOUT_CompileOptions, v, 0)
#define LEAN_LUAU_CompileOptions_DBGLVL(v) LEAN_POD_CTOR_GET_U8(LEAN_LUAU_LAYOUT_CompileOptions, v, 1)
#define LEAN_LUAU_CompileOptions_TYILVL(v) LEAN_POD_CTOR_GET_U8(LEAN_LUAU_LAYOUT_CompileOptions, v, 2)
#define LEAN_LUAU_CompileOptions_COVLVL(v) LEAN_POD_CTOR_GET_U8(LEAN_LUAU_LAYOUT_CompileOptions, v, 3)
#define LEAN_LUAU_CompileOptions_MUTGLB(v) LEAN_POD_CTOR_GET_BOX(LEAN_LUAU_LAYOUT_CompileOptions, v, 0)
#define LEAN_LUAU_CompileOptions_USRTYS(v) LEAN_POD_CTOR_GET_BOX(LEAN_LUAU_LAYOUT_CompileOptions, v, 1)
