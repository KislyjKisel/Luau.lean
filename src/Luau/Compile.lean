import Pod.Meta
import Pod.Buffer
import Luau.Initialization

namespace Luau

open Pod (Buffer)
open scoped Pod

inductive OptimizationLevel where
/-- No optimization -/
| no
/-- Baseline optimization level that doesn't prevent debuggability (default) -/
| baseline
/-- Includes optimizations that harm debuggability such as inlining -/
| performance
deriving Repr

instance : Inhabited OptimizationLevel where
  default := .baseline

inductive DebugLevel where
/-- No debugging support -/
| no
/-- Line info & function names only; sufficient for backtraces (default) -/
| backtrace
/-- Full debug info with local & upvalue names; necessary for debugger -/
| full
deriving Repr

instance : Inhabited DebugLevel where
  default := .backtrace

/--
Type information is used to guide native code generation decisions.
Information includes testable types for function arguments, locals, upvalues and some temporaries.
-/
inductive TypeInfoLevel where
/-- Generate for native modules (default) -/
| native
/-- Generate for all modules -/
| all
deriving Repr

instance : Inhabited TypeInfoLevel where
  default := .native

inductive CoverageLevel where
/-- No code coverage support (default) -/
| no
/-- Statement coverage -/
| statement
/-- Statement and expression coverage (verbose) -/
| verbose
deriving Repr

instance : Inhabited CoverageLevel where
  default := .no

structure CompileOptions where -- Layout synchronized with FFI
  optimizationLevel : OptimizationLevel := default
  debugLevel : DebugLevel := default
  typeInfoLevel : TypeInfoLevel := default
  coverageLevel : CoverageLevel := default
  /-- Array of globals that are mutable; disables the import optimization for fields accessed through these -/
  mutableGlobals : Array String := #[]
  /-- array of userdata types that will be included in the type information -/
  userdataTypes : Array String := #[]
  -- TODO vectorLib vectorCtor vectorType (what is it?)
deriving Repr, Inhabited

/-- Foreign memory blob created from `CompileOptions`. -/
define_foreign_type CompileOptions'

/--
Create a compile options blob.
The blob must be created before being passed to the compilation function anyway.
Exposing it to the Lean code allows its reuse.
Keeps a reference to the original object.
-/
@[extern "lean_luau_CompileOptions_bake"]
opaque CompileOptions.bake (raw : CompileOptions) : CompileOptions'

/--
Compile source to bytecode.
When source compilation fails, the resulting bytecode contains the encoded error.
-/
@[extern "lean_luau_compile"]
opaque compile (source : @& String) (options : @& CompileOptions') : BaseIO (Σ size : Nat, Buffer size 1) :=
  pure (.mk 0 Classical.ofNonempty)
