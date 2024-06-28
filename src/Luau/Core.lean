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

@[extern "lean_luau_State_new"]
opaque new : BaseIO (State Uu Ut Lt)

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

@[extern "lean_luau_State_getTop"]
opaque getTop (state : @& State Uu Ut Lt) : IO Int32

@[extern "lean_luau_State_setTop"]
opaque setTop (state : @& State Uu Ut Lt) (idx : Int32) : IO Unit

/-- Pops `n` elements from the stack. -/
@[extern "lean_luau_State_pop"]
opaque pop (state : @& State Uu Ut Lt) (n : Int32 := 1) : IO Unit

@[extern "lean_luau_State_pushValue"]
opaque pushValue (state : @& State Uu Ut Lt) (idx : Int32) : IO Unit

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

@[extern "lean_luau_State_isUserdata"]
opaque isUserdata (state : @& State Uu Ut Lt) (idx : Int32) : IO Bool

@[extern "lean_luau_State_type"]
opaque type (state : @& State Uu Ut Lt) (idx : Int32) : IO (Option «Type»)

@[extern "lean_luau_State_typeName"]
opaque typeName (state : @& State Uu Ut Lt) (tp : «Type») : IO String

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

@[extern "lean_luau_State_lessThan"]
opaque lessThan (state : @& State Uu Ut Lt) (idx1 idx2 : Int32) : IO Bool

@[extern "lean_luau_State_toNumberX"]
opaque toNumberX (state : @& State Uu Ut Lt) (idx : Int32) : IO (Option Number)

@[extern "lean_luau_State_toNumber"]
opaque toNumber (state : @& State Uu Ut Lt) (idx : Int32) : IO Number

@[extern "lean_luau_State_toIntegerX"]
opaque toIntegerX (state : @& State Uu Ut Lt) (idx : Int32) : IO (Option Integer)

@[extern "lean_luau_State_toInteger"]
opaque toInteger (state : @& State Uu Ut Lt) (idx : Int32) : IO Integer

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

@[extern "lean_luau_State_objLen"]
opaque objLen (state : @& State Uu Ut Lt) (idx : Int32) : IO Int32

@[extern "lean_luau_State_toCFunction"]
opaque toCFunction (state : @& State Uu Ut Lt) (idx : Int32) : IO (Option (CFunction Uu Ut Lt))

-- TODO: toLightUserdata (?)

@[extern "lean_luau_State_toLightUserdataTagged"]
opaque toLightUserdataTagged (state : @& State Uu Ut Lt) (idx : Int32) (tag : Tag) : IO (Option (Lt tag))

@[extern "lean_luau_State_toUserdata"]
opaque toUserdata (state : @& State Uu Ut Lt) (idx : Int32) : IO (Option Uu)

@[extern "lean_luau_State_toUserdataTagged"]
opaque toUserdataTagged (state : @& State Uu Ut Lt) (idx : Int32) (tag : Tag) : IO (Option (Ut tag))

/-- Returns `-1` if the value is not userdata, and `utagLimit + 1` if the value is untagged userdata. -/
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


/-! # Get functions -/

-- TODO


/-! # Set functions -/

-- TODO


/-! # Load and call functions -/

@[extern "lean_luau_State_load"]
opaque load (state : @& State Uu Ut Lt) (chunkName : @& Option String) {size : @& Nat} (data : @& BytesView size 1) (env : Int32) : IO Int32

@[extern "lean_luau_State_call"]
opaque call (state : @& State Uu Ut Lt) (nArgs nResults : Int32) : IO Unit

@[extern "lean_luau_State_pcall"]
opaque pcall (state : @& State Uu Ut Lt) (nArgs nResults errFunc : Int32) : IO Int32


/-! # Coroutine functions -/

-- TODO


/-! # GC functions and options -/

-- TODO


/-! # Memory statistics -/

-- TODO


/-! # Miscellaneous functions -/

@[extern "lean_luau_State_setUserdataDtor"]
opaque setUserdataDtor (state : @& State Uu Ut Lt) (tag : Tag) (dtor : State Uu Ut Lt → Ut tag → BaseIO Unit) : IO Unit

@[extern "lean_luau_State_resetUserdataDtor"]
opaque resetUserdataDtor (state : @& State Uu Ut Lt) (tag : Tag) : IO Unit

-- TODO: rest


/-! # Reference system -/

-- TODO


/-! # Debug API -/

-- TODO


/-! # Callbacks -/

-- TODO
