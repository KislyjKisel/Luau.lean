import Luau.Core

open Pod (Int32)

namespace Luau

namespace State

variable {Uu : Type} {Ut Lt : Tag → Type}

/--
Opens a library.

Creates a new table, sets it as the value of the global variable `libname`,
sets it as the value of `package.loaded[libname]`,
and registers on it all functions in the list `funcs`.
If there is a table in `package.loaded[libname]` or in variable `libname`,
reuses this table instead of creating a new one.

Leaves the table on the top of the stack.
-/
@[extern "lean_luau_State_register"]
opaque register (state : @& State Uu Ut Lt) (libname : @& String) (funcs : @& Array (String × CFunction Uu Ut Lt)) : IO Unit

/--
Registers all functions in `funcs` into the table on the top of the stack.

Leaves the table on the top of the stack.
-/
def register' (state : State Uu Ut Lt) (funcs : Array (String × CFunction Uu Ut Lt)) : IO Unit := do
  for f in funcs do
    state.pushCFunction f.2 f.1
    state.setField (-2) f.1

@[extern "lean_luau_State_getMetaField"]
opaque getMetaField (state : @& State Uu Ut Lt) (obj : Int32) (event : @& String) : IO Bool

/--
Calls a metamethod.

If the object at index `obj` has a metatable and this metatable has a field `event`,
this function calls this field and passes the object as its only argument.
In this case this function returns `true` and pushes onto the stack the value returned by the call.
If there is no metatable or no metamethod, this function returns `false` (without pushing any value on the stack).
-/
@[extern "lean_luau_State_callMeta"]
opaque callMeta (state : @& State Uu Ut Lt) (obj : Int32) (event : @& String) : IO Bool

@[extern "lean_luau_State_typeError"]
opaque typeError (state : @& State Uu Ut Lt) (narg : Int32) (tname : @& String) : IO Empty

@[extern "lean_luau_State_typeError_2"]
opaque typeError' (state : @& State Uu Ut Lt) (narg : Int32) (type : «Type») : IO Empty

@[extern "lean_luau_State_argError"]
opaque argError (state : @& State Uu Ut Lt) (narg : Int32) (extraMsg : @& String) : IO Empty

/--
Checks whether the function argument `narg` is a string and returns this string.

This function uses `toString` (`lua_tolstring`) to get its result,
so all conversions and caveats of that function apply here.
-/
@[extern "lean_luau_State_checkString"]
opaque checkString (state : @& State Uu Ut Lt) (narg : Int32) : IO String

/--
If the function argument `narg` is a string, returns this string.
If this argument is absent or is nil, returns `def`.
Otherwise, raises an error.
-/
@[extern "lean_luau_State_optString"]
opaque optString (state : @& State Uu Ut Lt) (narg : Int32) («def» : @& String) : IO String

/-- Checks whether the function argument `narg` is a number and returns this number.  -/
@[extern "lean_luau_State_checkNumber"]
opaque checkNumber (state : @& State Uu Ut Lt) (narg : Int32) : IO Number

/--
If the function argument `narg` is a number, returns this number.
If this argument is absent or is nil, returns `def`.
Otherwise, raises an error.
-/
@[extern "lean_luau_State_optNumber"]
opaque optNumber (state : @& State Uu Ut Lt) (narg : Int32) («def» : Number) : IO Number

@[extern "lean_luau_State_checkBoolean"]
opaque checkBoolean (state : @& State Uu Ut Lt) (narg : Int32) : IO Bool

@[extern "lean_luau_State_optBoolean"]
opaque optBoolean (state : @& State Uu Ut Lt) (narg : Int32) («def» : Bool) : IO Bool

/-- Checks whether the function argument `narg` is a number and returns this number cast to an `Integer`. -/
@[extern "lean_luau_State_checkInteger"]
opaque checkInteger (state : @& State Uu Ut Lt) (narg : Int32) : IO Integer

@[extern "lean_luau_State_optInteger"]
opaque optInteger (state : @& State Uu Ut Lt) (narg : Int32) («def» : Integer) : IO Integer

@[extern "lean_luau_State_checkUnsigned"]
opaque checkUnsigned (state : @& State Uu Ut Lt) (narg : Int32) : IO Unsigned

@[extern "lean_luau_State_optUnsigned"]
opaque optUnsigned (state : @& State Uu Ut Lt) (narg : Int32) («def» : Unsigned) : IO Unsigned

-- TODO: checkVector optVector

@[extern "lean_luau_State_checkStackL"]
opaque checkStackL (state : @& State Uu Ut Lt) (sz : Int32) (msg : @& String) : IO Unit

/-- Checks whether the function argument `narg` has the specified type. -/
@[extern "lean_luau_State_checkType"]
opaque checkType (state : @& State Uu Ut Lt) (narg : Int32) (type : «Type») : IO Unit

/-- Checks whether the function has an argument of any type (including nil) at position `narg`. -/
@[extern "lean_luau_State_checkAny"]
opaque checkAny (state : @& State Uu Ut Lt) (narg : Int32) : IO Unit

@[extern "lean_luau_State_newMetatable"]
opaque newMetatable (state : @& State Uu Ut Lt) (tname : @& String) : IO Bool

@[extern "lean_luau_State_checkUDataTagged"]
opaque checkUDataTagged (state : @& State Uu Ut Lt) (idx : Int32) (tag : Tag) (tname : @& String) : IO (Ut tag)

@[extern "lean_luau_State_checkUData"]
opaque checkUData (state : @& State Uu Ut Lt) (idx : Int32) (tname : @& String) : IO Uu

@[extern "lean_luau_State_checkBuffer"]
opaque checkBuffer (state : @& State Uu Ut Lt) (narg : Int32) : IO ByteArray

/--
Pushes onto the stack a string identifying the current position
of the control at level `lvl` in the call stack.
Typically this string has the following format:
```text
chunkname:currentline:
```

Level `0` is the running function, level `1` is the function that called the running function, etc.

This function is used to build a prefix for error messages.
-/
@[extern "lean_luau_State_where"]
opaque «where» (state : @& State Uu Ut Lt) (lvl : Int32) : IO Unit

@[extern "lean_luau_State_toStringL"]
opaque toStringL (state : @& State Uu Ut Lt) (idx : Int32) : IO String

@[extern "lean_luau_State_findTable"]
opaque findTable (state : @& State Uu Ut Lt) (idx : Int32) (fname : @& String) (szhint : Int32) : IO (Option String)

@[extern "lean_luau_State_typeNameL"]
opaque typeNameL (state : @& State Uu Ut Lt) (idx : Int32) : IO String

@[inline]
def getMetatableL (state : State Uu Ut Lt) (n : String) : IO «Type» :=
  getField state registryIndex n

@[inline]
def opt {α : Type} (state : State Uu Ut Lt) (idx : Int32) («def» : α) (f : State Uu Ut Lt → Int32 → IO α) : IO α := do
  if ← state.isNoneOrNil idx
    then pure «def»
    else f state idx

-- TODO: Strbuf and fns

def coLibName := "coroutine"
def tabLibName := "table"
def osLibName := "os"
def stringLibName := "string"
def bitLibName := "bit32"
def bufferLibName := "buffer"
def utf8LibName := "utf8"
def mathLibName := "math"
def dbLibName := "debug"

@[extern "lean_luau_State_openBase"]
opaque openBase (state : @& State Uu Ut Lt) : IO Unit

@[extern "lean_luau_State_openCoroutine"]
opaque openCoroutine (state : @& State Uu Ut Lt) : IO Unit

@[extern "lean_luau_State_openTable"]
opaque openTable (state : @& State Uu Ut Lt) : IO Unit

@[extern "lean_luau_State_openOs"]
opaque openOs (state : @& State Uu Ut Lt) : IO Unit

@[extern "lean_luau_State_openString"]
opaque openString (state : @& State Uu Ut Lt) : IO Unit

@[extern "lean_luau_State_openBit32"]
opaque openBit32 (state : @& State Uu Ut Lt) : IO Unit

@[extern "lean_luau_State_openBuffer"]
opaque openBuffer (state : @& State Uu Ut Lt) : IO Unit

@[extern "lean_luau_State_openUtf8"]
opaque openUtf8 (state : @& State Uu Ut Lt) : IO Unit

@[extern "lean_luau_State_openMath"]
opaque openMath (state : @& State Uu Ut Lt) : IO Unit

@[extern "lean_luau_State_openDebug"]
opaque openDebug (state : @& State Uu Ut Lt) : IO Unit

/-- Open all builtin libraries. -/
@[extern "lean_luau_State_openLibs"]
opaque openLibs (state : @& State Uu Ut Lt) : IO Unit

/-- Sandbox libraries and globals. -/
@[extern "lean_luau_State_sandbox"]
opaque sandbox (state : @& State Uu Ut Lt) : IO Unit

/-- Sandbox libraries and globals. -/
@[extern "lean_luau_State_sandboxThread"]
opaque sandboxThread (state : @& State Uu Ut Lt) : IO Unit
