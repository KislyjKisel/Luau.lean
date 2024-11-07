import Pod.Meta
import Pod.Int
import Pod.BytesView
import Luau.Initialization
import Luau.Config

open Pod (Int32 BytesView)

namespace Luau

open scoped Pod

/-- Option for passing multiple returns in `pcall` and `call`. -/
define_foreign_constant multret : Int32 := "lean_luau_const_multret"

define_foreign_constant registryIndex : Int32 := "lean_luau_const_registryIndex"
define_foreign_constant environIndex : Int32 := "lean_luau_const_environIndex"
define_foreign_constant globalsIndex : Int32 := "lean_luau_const_globalsIndex"

def upvalueIndex (i : Int32) : Int32 := globalsIndex - i
def isPseudo (i : Int32) : Bool := i <= registryIndex

/-- User upvalue index for closures. -/
def upvalueIndex' (i : Int32) : Int32 := globalsIndex - (i + 1)

/-- Thread status -/
inductive Status where
| ok
| yield
| errRun
/-- Legacy error code, preserved for compatibility -/
| errSyntax
| errMem
| errErr
/-- Yielded for a debug breakpoint -/
| break
deriving Repr, Inhabited, DecidableEq

inductive CoStatus where
/-- Running -/
| run
/-- Suspended -/
| sus
/-- 'normal' (it resumed another coroutine) -/
| nor
/-- Finished -/
| fin
/-- Finished with error -/
| err
deriving Repr, Inhabited, DecidableEq

abbrev Tag := { tag : UInt32 // tag < Config.utagLimit }

/--
Lua interpreter or a thread.
`Uu` - untagged userdata type.
`Ut` - tagged userdata type.
`Lt` - tagged light userdata type.
-/
define_foreign_type State (Uu : Type) (Ut Lt : Tag → Type)

/-- NOTE: User upvalues start at index 2. -/
def CFunction (Uu : Type) (Ut Lt : Tag → Type) := State Uu Ut Lt → IO Int32

/-- NOTE: User upvalues start at index 2. -/
def Continuation (Uu : Type) (Ut Lt : Tag → Type) := State Uu Ut Lt → CoStatus → IO Int32

-- Alloc skipped

def tNone : Int32 := -1

inductive «Type» where
| nil
| boolean
| lightUserdata
| number
| vector
| string
| table
| function
| userdata
| thread
| buffer
deriving Repr, Inhabited, DecidableEq

def «Type».toInt32 (ty : «Type») : Int32 :=
  Pod.Int32.ofUInt32 $ UInt32.ofNat ty.toCtorIdx

-- TODO: GCObject(?) Type with extra values

abbrev Number := Float
abbrev Integer := Int32
abbrev Unsigned := UInt32


namespace State

variable {Uu : Type} {Ut Lt : Tag → Type}


/-! # State manipulation -/

/--
Creates a new, independent state.
Throws an error if cannot create the state (due to lack of memory).
-/
@[extern "lean_luau_State_new"]
opaque new : IO (State Uu Ut Lt)

/--
Destroys all objects in the given Lua state
(calling the corresponding garbage-collection metamethods, if any) and
frees all dynamic memory used by this state.
-/
@[extern "lean_luau_State_close"]
opaque close (state : @& State Uu Ut Lt) : IO Unit

@[extern "lean_luau_State_newThread"]
opaque newThread (state : @& State Uu Ut Lt) : IO (State Uu Ut Lt)

@[extern "lean_luau_State_mainThread"]
opaque mainThread (state : @& State Uu Ut Lt) : IO (State Uu Ut Lt)

@[extern "lean_luau_State_resetThread"]
opaque resetThread (state : @& State Uu Ut Lt) : IO Unit

@[extern "lean_luau_State_isThreadReset"]
opaque isThreadReset (state : @& State Uu Ut Lt) : IO Bool

@[extern "lean_luau_State_isValid"]
opaque isValid (state : @& State Uu Ut Lt) : BaseIO Bool


/-! # Basic stack manipulation -/

@[extern "lean_luau_State_absIndex"]
opaque absIndex (state : @& State Uu Ut Lt) (idx : Int32) : IO Int32

/--
Returns the index of the top element in the stack.
Because indices start at 1, this result is equal to the number of elements in the stack
(and so 0 means an empty stack).
-/
@[extern "lean_luau_State_getTop"]
opaque getTop (state : @& State Uu Ut Lt) : IO Int32

/--
Accepts any acceptable index, or 0, and sets the stack top to this index.
If the new top is larger than the old one, then the new elements are filled with nil.
If index is 0, then all stack elements are removed.
-/
@[extern "lean_luau_State_setTop"]
opaque setTop (state : @& State Uu Ut Lt) (idx : Int32) : IO Unit

/-- Pops `n` elements from the stack. -/
@[extern "lean_luau_State_pop"]
opaque pop (state : @& State Uu Ut Lt) (n : Int32 := 1) : IO Unit

/-- Pushes a copy of the element at the given valid index onto the stack. -/
@[extern "lean_luau_State_pushValue"]
opaque pushValue (state : @& State Uu Ut Lt) (idx : Int32) : IO Unit

/--
Removes the element at the given valid index,
shifting down the elements above this index to fill the gap.
Cannot be called with a pseudo-index, because a pseudo-index is not an actual stack position.
-/
@[extern "lean_luau_State_remove"]
opaque remove (state : @& State Uu Ut Lt) (idx : Int32) : IO Unit

/--
Moves the top element into the given valid index, shifting up the elements above this index to open space.
This function cannot be called with a pseudo-index, because a pseudo-index is not an actual stack position.
-/
@[extern "lean_luau_State_insert"]
opaque insert (state : @& State Uu Ut Lt) (idx : Int32) : IO Unit

/--
Moves the top element into the given valid index without shifting any element
(therefore replacing the value at that given index), and then pops the top element.
-/
@[extern "lean_luau_State_replace"]
opaque replace (state : @& State Uu Ut Lt) (idx : Int32) : IO Unit

/--
Ensures the stack has room for pushing at least `sz` more values onto it.
Returns `false` if the stack could not be grown.
-/
@[extern "lean_luau_State_checkStack"]
opaque checkStack (state : @& State Uu Ut Lt) (sz : Int32) : IO Bool

/-- Like `checkStack` but allows for unlimited stack frames. -/
@[extern "lean_luau_State_rawCheckStack"]
opaque rawCheckStack (state : @& State Uu Ut Lt) (sz : Int32) : IO Unit

/--
Exchange values between different threads of the same global state.

This function pops `n` values from the stack from, and pushes them onto the stack to.
-/
@[extern "lean_luau_State_xmove"]
opaque xmove («from» to : @& State Uu Ut Lt) (n : Int32) : IO Unit

@[extern "lean_luau_State_xpush"]
opaque xpush («from» to : @& State Uu Ut Lt) (idx : Int32) : IO Unit


/-! # Access functions -/

@[extern "lean_luau_State_isNumber"]
opaque isNumber (state : @& State Uu Ut Lt) (idx : Int32) : IO Bool

@[extern "lean_luau_State_isString"]
opaque isString (state : @& State Uu Ut Lt) (idx : Int32) : IO Bool

@[extern "lean_luau_State_isCFunction"]
opaque isCFunction (state : @& State Uu Ut Lt) (idx : Int32) : IO Bool

@[extern "lean_luau_State_isLFunction"]
opaque isLFunction (state : @& State Uu Ut Lt) (idx : Int32) : IO Bool

/-- Is userdata, not light and untagged. -/
@[extern "lean_luau_State_isUserdata"]
opaque isUserdata (state : @& State Uu Ut Lt) (idx : Int32) : IO Bool

@[extern "lean_luau_State_isFunction"]
opaque isFunction (state : @& State Uu Ut Lt) (idx : Int32) : IO Bool

@[extern "lean_luau_State_isTable"]
opaque isTable (state : @& State Uu Ut Lt) (idx : Int32) : IO Bool

@[extern "lean_luau_State_isLightUserdata"]
opaque isLightUserdata (state : @& State Uu Ut Lt) (idx : Int32) : IO Bool

@[extern "lean_luau_State_isNil"]
opaque isNil (state : @& State Uu Ut Lt) (idx : Int32) : IO Bool

@[extern "lean_luau_State_isBoolean"]
opaque isBoolean (state : @& State Uu Ut Lt) (idx : Int32) : IO Bool

@[extern "lean_luau_State_isVector"]
opaque isVector (state : @& State Uu Ut Lt) (idx : Int32) : IO Bool

@[extern "lean_luau_State_isThread"]
opaque isThread (state : @& State Uu Ut Lt) (idx : Int32) : IO Bool

@[extern "lean_luau_State_isBuffer"]
opaque isBuffer (state : @& State Uu Ut Lt) (idx : Int32) : IO Bool

/--
Returns `true` if the given acceptable index is not valid
(that is, it refers to an element outside the current stack), and `false` otherwise.
-/
@[extern "lean_luau_State_isNone"]
opaque isNone (state : @& State Uu Ut Lt) (idx : Int32) : IO Bool

/--
Returns `true` if the given acceptable index is not valid
(that is, it refers to an element outside the current stack) or
if the value at this index is nil, and `false` otherwise.
-/
@[extern "lean_luau_State_isNoneOrNil"]
opaque isNoneOrNil (state : @& State Uu Ut Lt) (idx : Int32) : IO Bool

/--
Returns the type of the value in the given acceptable index,
or `tNone` for a non-valid index (that is, an index to an "empty" stack position).
-/
@[extern "lean_luau_State_type"]
opaque type (state : @& State Uu Ut Lt) (idx : Int32) : IO (Option «Type»)

/--
Returns the name of the type encoded by the value `tp`,
which must be one the values returned by `type`.
-/
@[extern "lean_luau_State_typeName"]
opaque typeName (state : @& State Uu Ut Lt) (tp : «Type») : IO String

@[inherit_doc typeName]
def typeName' (state : State Uu Ut Lt) (tp : Int32) : IO String :=
  if tp == tNone
    then pure "no value"
    else state.typeName (.ofNat tp.val.toNat)

/--
Returns `true` if the two values in acceptable indices `idx1` and `idx2` are equal,
following the semantics of the Lua `==` operator (that is, may call metamethods).
Otherwise returns `false`. Also returns `false` if any of the indices is non valid.
-/
@[extern "lean_luau_State_equal"]
opaque equal (state : @& State Uu Ut Lt) (idx1 idx2 : Int32) : IO Bool

/--
Returns `true` if the two values in acceptable indices `idx1` and `idx2` are primitively equal
(that is, without calling metamethods).
Otherwise returns `false`. Also returns `false` if any of the indices are non valid.
-/
@[extern "lean_luau_State_rawEqual"]
opaque rawEqual (state : @& State Uu Ut Lt) (idx1 idx2 : Int32) : IO Bool

/--
Returns `true` if the value at acceptable index `idx1` is smaller than the value at
acceptable index `idx2`, following the semantics of the Lua `<` operator (that is, may call metamethods).
Otherwise returns `false`.
Also returns `false` if any of the indices is non valid.
-/
@[extern "lean_luau_State_lessThan"]
opaque lessThan (state : @& State Uu Ut Lt) (idx1 idx2 : Int32) : IO Bool

/-- Same as `toNumber` but returns `none` if the value is not convertible to a number. -/
@[extern "lean_luau_State_toNumberX"]
opaque toNumberX (state : @& State Uu Ut Lt) (idx : Int32) : IO (Option Number)

/--
Converts the Lua value at the given acceptable index to the type `Number`.
The Lua value must be a number or a string convertible to a number
(see Lua Reference Manual §2.2.1); otherwise, returns `none`.
-/
@[extern "lean_luau_State_toNumber"]
opaque toNumber (state : @& State Uu Ut Lt) (idx : Int32) : IO Number

/-- Same as `toInteger` but returns `none` if the value is not convertible to an integer. -/
@[extern "lean_luau_State_toIntegerX"]
opaque toIntegerX (state : @& State Uu Ut Lt) (idx : Int32) : IO (Option Integer)

@[extern "lean_luau_State_toInteger"]
opaque toInteger (state : @& State Uu Ut Lt) (idx : Int32) : IO Integer

/-- Same as `toUnsigned` but returns `none` if the value is not convertible to an unsigned integer. -/
@[extern "lean_luau_State_toUnsignedX"]
opaque toUnsignedX (state : @& State Uu Ut Lt) (idx : Int32) : IO (Option Unsigned)

@[extern "lean_luau_State_toUnsigned"]
opaque toUnsigned (state : @& State Uu Ut Lt) (idx : Int32) : IO Unsigned

-- TODO: toVector

/--
Converts the Lua value at the given index to a `Bool`.
Returns `true` for any Lua value different from `false` and `nil`; otherwise it returns `false`.
-/
@[extern "lean_luau_State_toBoolean"]
opaque toBoolean (state : @& State Uu Ut Lt) (idx : Int32) : IO Bool

/--
Converts the Lua value at the given index to a `String`.
The Lua value must be a string or a number; otherwise, the function returns `none`.
If the value is a number, then `toString` also *changes the actual value in the stack to a string*.
The returned string ends with the first `'\0'` in the original body.
-/
@[extern "lean_luau_State_toString"]
opaque toString (state : @& State Uu Ut Lt) (idx : Int32) : IO (Option String)

-- TODO: toStringAtom nameCallAtom

/--
Returns the "length" of the value at the given acceptable index:
for strings, this is the string length;
for tables, this is the result of the length operator (`#`);
for userdata, this is the size of the block of memory allocated for the userdata;
for other values, it is `0`.
-/
@[extern "lean_luau_State_objLen"]
opaque objLen (state : @& State Uu Ut Lt) (idx : Int32) : IO Int32

@[inline]
def strLen (state : State Uu Ut Lt) (idx : Int32) : IO Int32 :=
  objLen state idx

@[extern "lean_luau_State_toCFunction"]
opaque toCFunction (state : @& State Uu Ut Lt) (idx : Int32) : IO (Option (CFunction Uu Ut Lt))

-- TODO: toLightUserdata (?)

@[extern "lean_luau_State_toLightUserdataTagged"]
opaque toLightUserdataTagged (state : @& State Uu Ut Lt) (idx : Int32) (tag : Tag) : IO (Option (Lt tag))

@[extern "lean_luau_State_toUserdata"]
opaque toUserdata (state : @& State Uu Ut Lt) (idx : Int32) : IO (Option Uu)

@[extern "lean_luau_State_toUserdataTagged"]
opaque toUserdataTagged (state : @& State Uu Ut Lt) (idx : Int32) (tag : Tag) : IO (Option (Ut tag))

/-- Returns `-1` if the value is not userdata, and `utagLimit` if the value is untagged userdata. -/
@[extern "lean_luau_State_userdataTag"]
opaque userdataTag (state : @& State Uu Ut Lt) (idx : Int32) : IO Int32

@[extern "lean_luau_State_lightUserdataTag"]
opaque lightUserdataTag (state : @& State Uu Ut Lt) (idx : Int32) : IO Int32

@[extern "lean_luau_State_toThread"]
opaque toThread (state : @& State Uu Ut Lt) (idx : Int32) : IO (Option (State Uu Ut Lt))

@[extern "lean_luau_State_toBuffer"]
opaque toBuffer (state : @& State Uu Ut Lt) (idx : Int32) : IO (Option ByteArray)

-- TODO: toPointer


/-! # Push functions -/

@[extern "lean_luau_State_pushNil"]
opaque pushNil (state : @& State Uu Ut Lt) : IO Unit

@[extern "lean_luau_State_pushNumber"]
opaque pushNumber (state : @& State Uu Ut Lt) (n : Number) : IO Unit

@[extern "lean_luau_State_pushInteger"]
opaque pushInteger (state : @& State Uu Ut Lt) (n : Integer) : IO Unit

@[extern "lean_luau_State_pushUnsigned"]
opaque pushUnsigned (state : @& State Uu Ut Lt) (n : Unsigned) : IO Unit

-- TODO: pushVector

@[extern "lean_luau_State_pushString"]
opaque pushString (state : @& State Uu Ut Lt) (s : @& String) : IO Unit

-- TODO: pushLiteral

/--
NOTE: User upvalues start at index 2. `nup` is the number of *user* upvalues. Same with the continuation.
-/
@[extern "lean_luau_State_pushCClosureK"]
opaque pushCClosureK (state : @& State Uu Ut Lt) (fn : CFunction Uu Ut Lt) (debugName : @& Option String) (nup : UInt32) (cont : Continuation Uu Ut Lt) : IO Unit

/--
NOTE: User upvalues start at index 2. `nup` is the number of *user* upvalues.
-/
@[extern "lean_luau_State_pushCClosure"]
opaque pushCClosure (state : @& State Uu Ut Lt) (fn : CFunction Uu Ut Lt) (debugName : @& Option String) (nup : UInt32) : IO Unit

/--
NOTE: User upvalues start at index 2. `nup` is the number of *user* upvalues.
-/
@[inline]
def pushCFunction (state : State Uu Ut Lt) (fn : CFunction Uu Ut Lt) (debugName : Option String) : IO Unit :=
  pushCClosure state fn debugName 0

@[extern "lean_luau_State_pushBoolean"]
opaque pushBoolean (state : @& State Uu Ut Lt) (b : Bool) : IO Unit

@[extern "lean_luau_State_pushThread"]
opaque pushThread (state : @& State Uu Ut Lt) : IO Bool

@[extern "lean_luau_State_pushLightUserdataTagged"]
opaque pushLightUserdataTagged (state : @& State Uu Ut Lt) (tag : Tag) (p : Lt tag) : IO Unit

-- TODO; pushLightUserdata (?)

@[extern "lean_luau_State_newUserdataTagged"]
opaque newUserdataTagged (state : @& State Uu Ut Lt) (tag : Tag) (userdata : Ut tag) : IO Unit

@[extern "lean_luau_State_newUserdata"]
opaque newUserdata (state : @& State Uu Ut Lt) (userdata : Uu) : IO Unit

@[extern "lean_luau_State_newUserdataDtor"]
opaque newUserdataDtor (state : @& State Uu Ut Lt) (userdata : Uu) (dtor : Uu → BaseIO Unit) : IO Unit

@[extern "lean_luau_State_newBuffer"]
opaque newBuffer (state : @& State Uu Ut Lt) {sz : @& Nat} (data : @& BytesView sz 1) : IO Unit


/-! # Get functions (Lua -> stack) -/

/--
Pushes onto stack the value `t[k]`, where `t` is the value at the given index and `k` is the value on top of the stack.
This function pops the key from the stack, pushing the resulting value in its place.
As in Lua, the function may trigger a metamethod for the "index" event.
Returns the type of the pushed value.
-/
@[extern "lean_luau_State_getTable"]
opaque getTable (state : @& State Uu Ut Lt) (idx : Int32) : IO «Type»

/--
Pushes onto stack the value `t[k]`, where `t` is the value at the given valid index.
As in Lua, this function may trigger a metamethod for the "index" event (see Lua Reference Manual §2.8).
-/
@[extern "lean_luau_State_getField"]
opaque getField (state : @& State Uu Ut Lt) (idx : Int32) (k : @& String) : IO «Type»

/-- Same as `getField` but does not trigger metamethods. -/
@[extern "lean_luau_State_rawGetField"]
opaque rawGetField (state : @& State Uu Ut Lt) (idx : Int32) (k : @& String) : IO «Type»

/--
Similar to `getTable`, but does a raw access (i.e., without metamethods).
The value at index must be a table.
-/
@[extern "lean_luau_State_rawGet"]
opaque rawGet (state : @& State Uu Ut Lt) (idx : Int32) : IO «Type»

@[extern "lean_luau_State_rawGetI"]
opaque rawGetI (state : @& State Uu Ut Lt) (idx n : Int32) : IO «Type»

/--
Creates a new empty table and pushes it onto the stack.
The new table has space pre-allocated for `narr` array elements and `nrec` non-array elements.
This pre-allocation is useful when you know exactly how many elements the table will have.
Otherwise you can use the function `newTable`.
-/
@[extern "lean_luau_State_createTable"]
opaque createTable (state : @& State Uu Ut Lt) (narr nrec : Int32) : IO Unit

/-- Creates a new empty table and pushes it onto the stack. -/
@[inline]
def newTable (state : State Uu Ut Lt) : IO Unit :=
  createTable state 0 0

@[extern "lean_luau_State_setReadonly"]
opaque setReadonly (state : @& State Uu Ut Lt) (idx : Int32) (enabled : Bool) : IO Unit

@[extern "lean_luau_State_getReadonly"]
opaque getReadonly (state : @& State Uu Ut Lt) (idx : Int32) : IO Bool

@[extern "lean_luau_State_setSafeEnv"]
opaque setSafeEnv (state : @& State Uu Ut Lt) (idx : Int32) (enabled : Bool) : IO Unit

/--
Pushes onto the stack the metatable of the value at the given acceptable index.
If the index is not valid, or if the value does not have a metatable,
the function returns `false` and pushes nothing on the stack.
-/
@[extern "lean_luau_State_getMetatable"]
opaque getMetatable (state : @& State Uu Ut Lt) (objindex : Int32) : IO Bool

@[extern "lean_luau_State_getFEnv"]
opaque getFEnv (state : @& State Uu Ut Lt) (idx : Int32) : IO Unit

@[inline]
def getGlobal (state : @& State Uu Ut Lt) (k : @& String) : IO «Type» :=
  getField state globalsIndex k


/-! # Set functions (stack -> Lua) -/

@[extern "lean_luau_State_setTable"]
opaque setTable (state : @& State Uu Ut Lt) (idx : Int32) : IO Unit

/--
Does the equivalent to `t[k] = v`,
where `t` is the value at the given valid index and `v` is the value at the top of the stack.

This function pops the value from the stack.
As in Lua, this function may trigger a metamethod for the "newindex" event (see Lua Reference Manual §2.8).
-/
@[extern "lean_luau_State_setField"]
opaque setField (state : @& State Uu Ut Lt) (idx : Int32) (k : @& String) : IO Unit

@[extern "lean_luau_State_rawSetField"]
opaque rawSetField (state : @& State Uu Ut Lt) (idx : Int32) (k : @& String) : IO Unit

@[extern "lean_luau_State_rawSet"]
opaque rawSet (state : @& State Uu Ut Lt) (idx : Int32) : IO Unit

@[extern "lean_luau_State_rawSetI"]
opaque rawSetI (state : @& State Uu Ut Lt) (idx n : Int32) : IO Unit

/--
Pops a table or `nil` from the stack and sets that value as the new metatable
for the value at the given index.
(`nil` means no metatable)
-/
@[extern "lean_luau_State_setMetatable"]
opaque setMetatable (state : @& State Uu Ut Lt) (objindex : Int32) : IO Unit

@[extern "lean_luau_State_setFEnv"]
opaque setFEnv (state : @& State Uu Ut Lt) (idx : Int32) : IO Bool

@[inline]
def setGlobal (state : State Uu Ut Lt) (k : String) : IO Unit :=
  setField state globalsIndex k


/-! # Load and call functions -/

@[extern "lean_luau_State_load"]
opaque load (state : @& State Uu Ut Lt) (chunkName : @& String) {size : @& Nat} (data : @& BytesView size 1) (env : Int32 := 0) : IO Bool

/--

Calls a function.

To call a function you must use the following protocol:
first, the function to be called is pushed onto the stack;
then, the arguments to the function are pushed in direct order;
that is, the first argument is pushed first.
Finally you call `call`; `nArgs` is the number of arguments that you pushed onto the stack.
All arguments and the function value are popped from the stack when the function is called.
The function results are pushed onto the stack when the function returns.
The number of results is adjusted to `nResults`, unless nresults is `multret`.
In this case, all results from the function are pushed.
Lua takes care that the returned values fit into the stack space.
The function results are pushed onto the stack in direct order (the first result is pushed first),
so that after the call the last result is on the top of the stack.

Any error inside the called function is propagated upwards (with a `longjmp`).

The following example shows how the host program can do the equivalent to this Lua code:

```lua
a = f("how", t.x, 14)
```

Here it is in Lean:
```lean4
let _ ← state.getField globalsIndex "f" -- function to be called
state.pushString "how"                  -- 1st argument
let _ ← state.getField globalsIndex "t" -- table to be indexed
let _ ← state.getField -1 "x"           -- push result of t.x (2nd arg)
state.remove (-2)                       -- remove 't' from the stack
state.pushInteger 14                    -- 3rd argument
state.call 3 1                          -- call 'f' with 3 arguments and 1 result
state.setField globalsIndex "a"         -- set global 'a'
```

Note that the code above is "balanced": at its end, the stack is back to its original configuration.
This is considered good programming practice.
-/
@[extern "lean_luau_State_call"]
opaque call (state : @& State Uu Ut Lt) (nArgs nResults : Int32) : IO Unit

/--
Calls a function in protected mode.
If there are no errors during the call, `pcall` behaves exactly like `call`.
However, if there is any error, `pcall` catches it,
pushes a single value on the stack (the error message), and returns an error code.
Like `call`, `pcall` always removes the function and its arguments from the stack.

If `errfunc` is `0`, then the error message returned on the stack is exactly the original error message.
Otherwise, `errfunc` is the stack index of an error handler function.
In case of runtime errors, this function will be called with the error message and
its return value will be the message returned on the stack by `pcall`.
-/
@[extern "lean_luau_State_pcall"]
opaque pcall (state : @& State Uu Ut Lt) (nArgs nResults errFunc : Int32) : IO Int32


/-! # Coroutine functions -/

@[extern "lean_luau_State_yield"]
opaque yield (state : @& State Uu Ut Lt) (nargs : Int32) : IO Int32

@[extern "lean_luau_State_break"]
opaque «break» (state : @& State Uu Ut Lt) : IO Int32

@[extern "lean_luau_State_resume"]
opaque resume (state : @& State Uu Ut Lt) («from» : @& Option (State Uu Ut Lt)) (narg : Int32) : IO Status

@[extern "lean_luau_State_resumeError"]
opaque resumeError (state : @& State Uu Ut Lt) («from» : @& Option (State Uu Ut Lt)) : IO Status

@[extern "lean_luau_State_status"]
opaque status (state : @& State Uu Ut Lt) (idx : Int32) : IO Status

@[extern "lean_luau_State_isYieldable"]
opaque isYieldable (state : @& State Uu Ut Lt) : IO Bool

-- TODO: getThreadUserdata setThreadUserdata

@[extern "lean_luau_State_coStatus"]
opaque coStatus (state co : @& State Uu Ut Lt) : IO CoStatus



/-! # GC functions and options -/

inductive GCOp where
/-- Stop incremental garbage collection -/
| stop
/-- Resume incremental garbage collection -/
| restart
/-- Run a full GC cycle; not recommended for latency sensitive applications -/
| collect
/-- Return the heap size in KB -/
| count
/-- Return the heap size KB remainder in bytes -/
| countB
/-- Return `1` if the GC is active (not stopped); note that GC may not be actively collecting even if it's running -/
| isRunning
/-- Perform an explicit GC step, with the step size specified in KB -/
| step
/-- Tune GC parameters: G (goal) -/
| setGoal
/-- Tune GC parameters: S (step multiplier) -/
| setStepMul
/-- Tune GC parameters: step size (usually best left ignored) -/
| setStepSize
deriving Repr, Inhabited, DecidableEq

/-- Controls the garbage collector. See `GCOp` constructors. -/
@[extern "lean_luau_State_gc"]
opaque gc (state : @& State Uu Ut Lt) (what : GCOp) (data : Int32) : IO Int32

@[inline, inherit_doc GCOp.stop]
def gcStop (state : State Uu Ut Lt) : IO Unit := gc state .stop 0 *> pure ()

@[inline, inherit_doc GCOp.restart]
def gcRestart (state : State Uu Ut Lt) : IO Unit := gc state .restart 0 *> pure ()

@[inline, inherit_doc GCOp.collect]
def gcCollect (state : State Uu Ut Lt) : IO Unit := gc state .collect 0 *> pure ()


/-! # Memory statistics -/

@[extern "lean_luau_State_setMemCat"]
opaque setMemCat (state : @& State Uu Ut Lt) (category : UInt32) : IO Unit

@[extern "lean_luau_State_totalBytes"]
opaque totalBytes (state : @& State Uu Ut Lt) (category : UInt32) : IO USize


/-! # Miscellaneous functions -/

/--
Generates a Lua error.
The error message (which can actually be a Lua value of any type) must be on the stack top.
This function does a long jump, and therefore never returns.
-/
@[extern "lean_luau_State_error"]
opaque error (state : @& State Uu Ut Lt) : IO Empty

/--
Pops a key from the stack, and pushes a key-value pair from the table at the given index (the "next" pair after the given key).
If there are no more elements in the table, then `next` returns `false` (and pushes nothing).
-/
@[extern "lean_luau_State_next"]
opaque next (state : @& State Uu Ut Lt) : IO Bool

@[extern "lean_luau_State_rawIter"]
opaque rawIter (state : @& State Uu Ut Lt) (idx iter : Int32) : IO Int32

/--
Concatenates the `n` values at the top of the stack, pops them, and leaves the result at the top.
If n is `1`, the result is the single value on the stack (that is, the function does nothing);
if n is `0`, the result is the empty string.
Concatenation is performed following the usual semantics of Lua (see Lua Reference Manual §2.5.4).
-/
@[extern "lean_luau_State_concat"]
opaque concat (state : @& State Uu Ut Lt) (n : Int32) : IO Unit

@[extern "lean_luau_State_clock"]
opaque clock (state : @& State Uu Ut Lt) : IO Float

-- Skipped setuserdatatag: tag is in the type of the userdata value

@[extern "lean_luau_State_setUserdataDtor"]
opaque setUserdataDtor (state : @& State Uu Ut Lt) (tag : Tag) (dtor : State Uu Ut Lt → Ut tag → BaseIO Unit) : IO Unit

@[extern "lean_luau_State_resetUserdataDtor"]
opaque resetUserdataDtor (state : @& State Uu Ut Lt) (tag : Tag) : IO Unit

@[extern "lean_luau_State_setUserdataMetatable"]
opaque setUserdataMetatable (state : @& State Uu Ut Lt) (tag : Tag) (idx : Int32) : IO Unit

@[extern "lean_luau_State_getUserdataMetatable"]
opaque getUserdataMetatable (state : @& State Uu Ut Lt) (tag : Tag) : IO Unit

@[extern "lean_luau_State_setLightUserdataName"]
opaque setLightUserdataName (state : @& State Uu Ut Lt) (tag : Tag) (name : @& String) : IO Unit

@[extern "lean_luau_State_getLightUserdataName"]
opaque getLightUserdataName (state : @& State Uu Ut Lt) (tag : Tag) : IO String

@[extern "lean_luau_State_cloneFunction"]
opaque cloneFunction (state : @& State Uu Ut Lt) (idx : Int32) : IO Unit

@[extern "lean_luau_State_clearTable"]
opaque clearTable (state : @& State Uu Ut Lt) (idx : Int32) : IO Unit


/-! # Reference system -/

abbrev Ref := Int32

def noRef : Ref := -1
def refNil : Ref := 0

@[extern "lean_luau_State_ref"]
opaque ref (state : @& State Uu Ut Lt) (idx : Int32) : IO Ref

@[extern "lean_luau_State_unref"]
opaque unref (state : @& State Uu Ut Lt) (ref : Ref) : IO Unit


/-! # Debug API -/

-- TODO


/-! # Callbacks -/

@[extern "lean_luau_State_setInterruptCallback"]
opaque setInterruptCallback (state : @& State Uu Ut Lt) (f : State Uu Ut Lt → Int32 → BaseIO Unit) : IO Unit

@[extern "lean_luau_State_resetInterruptCallback"]
opaque resetInterruptCallback (state : @& State Uu Ut Lt) : IO Unit

@[extern "lean_luau_State_setPanicCallback"]
opaque setPanicCallback (state : @& State Uu Ut Lt) (f : State Uu Ut Lt → Int32 → BaseIO Unit) : IO Unit

@[extern "lean_luau_State_resetPanicCallback"]
opaque resetPanicCallback (state : @& State Uu Ut Lt) : IO Unit

-- TODO: userthread, useratom, debug callbacks
