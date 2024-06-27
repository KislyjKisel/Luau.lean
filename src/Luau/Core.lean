import Pod.Meta
import Pod.Int
import Pod.BytesView
import Luau.Initialization

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

/-- Lua interpreter or a thread -/
define_foreign_type State

def Function := State → IO Int32
def Continuation := State → CoStatus → IO Int32

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
opaque new : BaseIO State

@[extern "lean_luau_State_close"]
opaque close (state : @& State) : IO Unit

@[extern "lean_luau_State_newThread"]
opaque newThread (state : @& State) : IO State

@[extern "lean_luau_State_mainThread"]
opaque mainThread (state : @& State) : IO State

@[extern "lean_luau_State_resetThread"]
opaque resetThread (state : @& State) : IO Unit

@[extern "lean_luau_State_isThreadReset"]
opaque isThreadReset (state : @& State) : IO Bool

@[extern "lean_luau_State_isValid"]
opaque isValid (state : @& State) : BaseIO Bool


/-! # Basic stack manipulation -/

@[extern "lean_luau_State_absIndex"]
opaque absIndex (state : @& State) (idx : Int32) : IO Int32

@[extern "lean_luau_State_getTop"]
opaque getTop (state : @& State) : IO Int32

@[extern "lean_luau_State_setTop"]
opaque setTop (state : @& State) (idx : Int32) : IO Unit

@[extern "lean_luau_State_pushValue"]
opaque pushValue (state : @& State) (idx : Int32) : IO Unit

@[extern "lean_luau_State_remove"]
opaque remove (state : @& State) (idx : Int32) : IO Unit

@[extern "lean_luau_State_insert"]
opaque insert (state : @& State) (idx : Int32) : IO Unit

@[extern "lean_luau_State_replace"]
opaque replace (state : @& State) (idx : Int32) : IO Unit

/--
Ensures the stack has room for pushing at least `sz` more values onto it.
Returns `false` if the stack could not be grown.
-/
@[extern "lean_luau_State_checkStack"]
opaque checkStack (state : @& State) (sz : Int32) : IO Bool

/-- Like `checkStack` but allows for unlimited stack frames. -/
@[extern "lean_luau_State_rawCheckStack"]
opaque rawCheckStack (state : @& State) (sz : Int32) : IO Unit

@[extern "lean_luau_State_xmove"]
opaque xmove («from» to : @& State) (n : Int32) : IO Unit

@[extern "lean_luau_State_xpush"]
opaque xpush («from» to : @& State) (idx : Int32) : IO Unit


/-! # Access functions -/

@[extern "lean_luau_State_isNumber"]
opaque isNumber (state : @& State) (idx : Int32) : IO Bool

@[extern "lean_luau_State_isString"]
opaque isString (state : @& State) (idx : Int32) : IO Bool

@[extern "lean_luau_State_isCFunction"]
opaque isCFunction (state : @& State) (idx : Int32) : IO Bool

@[extern "lean_luau_State_isLFunction"]
opaque isLFunction (state : @& State) (idx : Int32) : IO Bool

@[extern "lean_luau_State_isUserdata"]
opaque isUserdata (state : @& State) (idx : Int32) : IO Bool

@[extern "lean_luau_State_type"]
opaque type (state : @& State) (idx : Int32) : IO (Option «Type»)

@[extern "lean_luau_State_typeName"]
opaque typeName (state : @& State) (tp : «Type») : IO String

def typeName' (state : State) (tp : Int32) : IO String :=
  if tp == tNone
    then pure "no value"
    else state.typeName (.ofNat tp.val.toNat)

/--
Returns `true` if the two values in acceptable indices `idx1` and `idx2` are equal,
following the semantics of the Lua `==` operator (that is, may call metamethods).
Otherwise returns `false`. Also returns `false` if any of the indices is non valid.
-/
@[extern "lean_luau_State_equal"]
opaque equal (state : @& State) (idx1 idx2 : Int32) : IO Bool

/--
Returns `true` if the two values in acceptable indices `idx1` and `idx2` are primitively equal
(that is, without calling metamethods).
Otherwise returns `false`. Also returns `false` if any of the indices are non valid.
-/
@[extern "lean_luau_State_rawEqual"]
opaque rawEqual (state : @& State) (idx1 idx2 : Int32) : IO Bool

@[extern "lean_luau_State_lessThan"]
opaque lessThan (state : @& State) (idx1 idx2 : Int32) : IO Bool


/-! # Push functions -/

-- TODO


/-! # Get functions -/

-- TODO


/-! # Set functions -/

-- TODO


/-! # Load and call functions -/

@[extern "lean_luau_State_load"]
opaque load (state : @& State) (chunkName : @& Option String) {size : @& Nat} (data : @& BytesView size 1) (env : Int32) : IO Int32

@[extern "lean_luau_State_call"]
opaque call (state : @& State) (nArgs nResults : Int32) : IO Unit

@[extern "lean_luau_State_pcall"]
opaque pcall (state : @& State) (nArgs nResults errFunc : Int32) : IO Int32

-- TODO: Rest
