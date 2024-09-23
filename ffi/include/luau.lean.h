#include <lean/lean.h>
#include <lean_pod.h>
#include <luacode.h>

typedef struct {
    lua_CompileOptions options;
    lean_object* owner;
} lean_luau_CompileOptions_data;

LEAN_POD_DECLARE_EXTERNAL_CLASS(luau_CompileOptions, lean_luau_CompileOptions_data*)

// Lean-side CompileOptions
#define LEAN_LUAU_CompileOptions_LAYOUT 0, 2, 0, 0, 0, 0, 4
#define LEAN_LUAU_CompileOptions_optimizationLevel U8, 0, LEAN_LUAU_CompileOptions_LAYOUT
#define LEAN_LUAU_CompileOptions_debugLevel U8, 1, LEAN_LUAU_CompileOptions_LAYOUT
#define LEAN_LUAU_CompileOptions_typeInfoLevel U8, 2, LEAN_LUAU_CompileOptions_LAYOUT
#define LEAN_LUAU_CompileOptions_coverageLevel U8, 3, LEAN_LUAU_CompileOptions_LAYOUT
#define LEAN_LUAU_CompileOptions_mutableGlobals BOX, 0, LEAN_LUAU_CompileOptions_LAYOUT
#define LEAN_LUAU_CompileOptions_userdataTypes BOX, 1, LEAN_LUAU_CompileOptions_LAYOUT

typedef struct lua_State lua_State;

typedef struct lean_luau_State_data lean_luau_State_data;

struct lean_luau_State_data {
    lua_State* state; // NULL = closed
    lean_luau_State_data* main;
    lean_object** taggedUserdataDtors; // undefined for non-main data
    lean_object** referenced; // undefined for non-main data
    size_t referencedCount; // undefined for non-main data
    size_t referencedCapacity; // undefined for non-main data
    lean_object* interruptCallback; // undefined for non-main data, may be NULL
    lean_object* panicCallback; // undefined for non-main data, may be NULL
};

LEAN_POD_DECLARE_EXTERNAL_CLASS(luau_State, lean_luau_State_data*)
