import Pod.Meta
import Pod.Int
import Pod.BytesView
import Luau.Initialization

open Pod (Int32 BytesView)

namespace Luau

open scoped Pod

variable {α β : Type}

/-- Option for passing multiple returns in `pcall` and `call`. -/
define_foreign_constant multret : Int32 := "lean_luau_const_multret"

define_foreign_constant registryIndex : Int32 := "lean_luau_const_registryIndex"
define_foreign_constant environIndex : Int32 := "lean_luau_const_environIndex"
define_foreign_constant globalsIndex : Int32 := "lean_luau_const_globalsIndex"

def upvalueIndex (i : Int32) : Int32 := globalsIndex - i
def isPseudo (i : Int32) : Bool := i <= registryIndex

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

/-- Lua interpreter or a thread. `α`, `β` - userdata and light userdata types. -/
define_foreign_type State (α β : Type)

def CFunction (α β : Type) := State α β → IO Int32
def Continuation (α β : Type) := State α β → CoStatus → IO Int32

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

/-! # State manipulation -/

@[extern "lean_luau_State_new"]
opaque new : BaseIO (State α β)

@[extern "lean_luau_State_close"]
opaque close (state : @& State α β) : IO Unit

@[extern "lean_luau_State_newThread"]
opaque newThread (state : @& State α β) : IO (State α β)

@[extern "lean_luau_State_mainThread"]
opaque mainThread (state : @& State α β) : IO (State α β)

@[extern "lean_luau_State_resetThread"]
opaque resetThread (state : @& State α β) : IO Unit

@[extern "lean_luau_State_isThreadReset"]
opaque isThreadReset (state : @& State α β) : IO Bool

@[extern "lean_luau_State_isValid"]
opaque isValid (state : @& State α β) : BaseIO Bool


/-! # Basic stack manipulation -/

@[extern "lean_luau_State_absIndex"]
opaque absIndex (state : @& State α β) (idx : Int32) : IO Int32

@[extern "lean_luau_State_getTop"]
opaque getTop (state : @& State α β) : IO Int32

@[extern "lean_luau_State_setTop"]
opaque setTop (state : @& State α β) (idx : Int32) : IO Unit

/-- Pops `n` elements from the stack. -/
@[extern "lean_luau_State_pop"]
opaque pop (state : @& State α β) (n : Int32 := 1) : IO Unit

@[extern "lean_luau_State_pushValue"]
opaque pushValue (state : @& State α β) (idx : Int32) : IO Unit

@[extern "lean_luau_State_remove"]
opaque remove (state : @& State α β) (idx : Int32) : IO Unit

@[extern "lean_luau_State_insert"]
opaque insert (state : @& State α β) (idx : Int32) : IO Unit

@[extern "lean_luau_State_replace"]
opaque replace (state : @& State α β) (idx : Int32) : IO Unit

/--
Ensures the stack has room for pushing at least `sz` more values onto it.
Returns `false` if the stack could not be grown.
-/
@[extern "lean_luau_State_checkStack"]
opaque checkStack (state : @& State α β) (sz : Int32) : IO Bool

/-- Like `checkStack` but allows for unlimited stack frames. -/
@[extern "lean_luau_State_rawCheckStack"]
opaque rawCheckStack (state : @& State α β) (sz : Int32) : IO Unit

@[extern "lean_luau_State_xmove"]
opaque xmove («from» to : @& State α β) (n : Int32) : IO Unit

@[extern "lean_luau_State_xpush"]
opaque xpush («from» to : @& State α β) (idx : Int32) : IO Unit


/-! # Access functions -/

@[extern "lean_luau_State_isNumber"]
opaque isNumber (state : @& State α β) (idx : Int32) : IO Bool

@[extern "lean_luau_State_isString"]
opaque isString (state : @& State α β) (idx : Int32) : IO Bool

@[extern "lean_luau_State_isCFunction"]
opaque isCFunction (state : @& State α β) (idx : Int32) : IO Bool

@[extern "lean_luau_State_isLFunction"]
opaque isLFunction (state : @& State α β) (idx : Int32) : IO Bool

@[extern "lean_luau_State_isUserdata"]
opaque isUserdata (state : @& State α β) (idx : Int32) : IO Bool

@[extern "lean_luau_State_type"]
opaque type (state : @& State α β) (idx : Int32) : IO (Option «Type»)

@[extern "lean_luau_State_typeName"]
opaque typeName (state : @& State α β) (tp : «Type») : IO String

def typeName' (state : State α β) (tp : Int32) : IO String :=
  if tp == tNone
    then pure "no value"
    else state.typeName (.ofNat tp.val.toNat)

/--
Returns `true` if the two values in acceptable indices `idx1` and `idx2` are equal,
following the semantics of the Lua `==` operator (that is, may call metamethods).
Otherwise returns `false`. Also returns `false` if any of the indices is non valid.
-/
@[extern "lean_luau_State_equal"]
opaque equal (state : @& State α β) (idx1 idx2 : Int32) : IO Bool

/--
Returns `true` if the two values in acceptable indices `idx1` and `idx2` are primitively equal
(that is, without calling metamethods).
Otherwise returns `false`. Also returns `false` if any of the indices are non valid.
-/
@[extern "lean_luau_State_rawEqual"]
opaque rawEqual (state : @& State α β) (idx1 idx2 : Int32) : IO Bool

@[extern "lean_luau_State_lessThan"]
opaque lessThan (state : @& State α β) (idx1 idx2 : Int32) : IO Bool

@[extern "lean_luau_State_toNumberX"]
opaque toNumberX (state : @& State α β) (idx : Int32) : IO (Option Number)

@[extern "lean_luau_State_toNumber"]
opaque toNumber (state : @& State α β) (idx : Int32) : IO Number

@[extern "lean_luau_State_toIntegerX"]
opaque toIntegerX (state : @& State α β) (idx : Int32) : IO (Option Integer)

@[extern "lean_luau_State_toInteger"]
opaque toInteger (state : @& State α β) (idx : Int32) : IO Integer

@[extern "lean_luau_State_toUnsignedX"]
opaque toUnsignedX (state : @& State α β) (idx : Int32) : IO (Option Unsigned)

@[extern "lean_luau_State_toUnsigned"]
opaque toUnsigned (state : @& State α β) (idx : Int32) : IO Unsigned

-- TODO: toVector

/--
Converts the Lua value at the given index to a `Bool`.
Returns `true` for any Lua value different from `false` and `nil`; otherwise it returns `false`.
-/
@[extern "lean_luau_State_toBoolean"]
opaque toBoolean (state : @& State α β) (idx : Int32) : IO Bool

/--
Converts the Lua value at the given index to a `String`.
The Lua value must be a string or a number; otherwise, the function returns `none`.
If the value is a number, then `toString` also *changes the actual value in the stack to a string*.
The returned string ends with the first `'\0'` in the original body.
-/
@[extern "lean_luau_State_toString"]
opaque toString (state : @& State α β) (idx : Int32) : IO (Option String)

-- TODO: toStringAtom nameCallAtom

@[extern "lean_luau_State_objLen"]
opaque objLen (state : @& State α β) (idx : Int32) : IO Int32

@[extern "lean_luau_State_toCFunction"]
opaque toCFunction (state : @& State α β) (idx : Int32) : IO (Option (CFunction α β))

@[extern "lean_luau_State_toLightUserdata"]
opaque toLightUserdata (state : @& State α β) (idx : Int32) : IO (Option β)

@[extern "lean_luau_State_toLightUserdataTagged"]
opaque toLightUserdataTagged (state : @& State α β) (idx tag : Int32) : IO (Option β)

@[extern "lean_luau_State_toUserdata"]
opaque toUserdata (state : @& State α β) (idx : Int32) : IO (Option α)

@[extern "lean_luau_State_toUserdataTagged"]
opaque toUserdataTagged (state : @& State α β) (idx tag : Int32) : IO (Option α)

@[extern "lean_luau_State_userdataTag"]
opaque userdataTag (state : @& State α β) (idx : Int32) : IO Int32

@[extern "lean_luau_State_lightUserdataTag"]
opaque lightUserdataTag (state : @& State α β) (idx : Int32) : IO Int32

@[extern "lean_luau_State_toThread"]
opaque toThread (state : @& State α β) (idx : Int32) : IO (Option (State α β))

@[extern "lean_luau_State_toBuffer"]
opaque toBuffer (state : @& State α β) (idx : Int32) : IO (Option ByteArray)

-- TODO: toPointer


/-! # Push functions -/

-- TODO


/-! # Get functions -/

-- TODO


/-! # Set functions -/

-- TODO


/-! # Load and call functions -/

@[extern "lean_luau_State_load"]
opaque load (state : @& State α β) (chunkName : @& Option String) {size : @& Nat} (data : @& BytesView size 1) (env : Int32) : IO Int32

@[extern "lean_luau_State_call"]
opaque call (state : @& State α β) (nArgs nResults : Int32) : IO Unit

@[extern "lean_luau_State_pcall"]
opaque pcall (state : @& State α β) (nArgs nResults errFunc : Int32) : IO Int32

-- TODO: Rest
