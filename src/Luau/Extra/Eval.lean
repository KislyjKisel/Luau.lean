import Luau.Lib
import Luau.Compile
import Luau.Extra.FromTo

open Pod (BytesView Int32)

namespace Luau

variable {Uu : Type} {Ut Lt : Tag → Type}

namespace State

def tryLoad (state : State Uu Ut Lt) (chunkName : String) {size} (data : BytesView size 1) (env : Int32 := 0) : IO Unit := do
  if ← state.load chunkName data env then
    throw $ IO.userError $ ToString.toString $ ← state.toString (-1)

set_option compiler.extract_closed false in
def eval (state : State Uu Ut Lt) (chunkSource : String) (nArgs : Int32 := 0) (nResults : Int32 := 1) : IO Unit := do
  let ⟨_, binary⟩ ← Luau.compile chunkSource <| .ofRaw default
  tryLoad state "eval_source" binary.view
  state.call nArgs nResults

end State

def evalStr (chunkSource : String) : IO String := do
  let state : State Uu Ut Lt ← Luau.State.new
  state.eval chunkSource
  state.toStringL (-1)

def evalPop {α} [FromLua Uu Ut Lt α] (chunkSource : String) : IO (Option α) := do
  let state : State Uu Ut Lt ← Luau.State.new
  state.eval chunkSource
  FromLua.pop state
