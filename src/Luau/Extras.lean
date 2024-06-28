import Luau.Lib

open Pod (BytesView Int32)

namespace Luau.State

variable {Uu : Type} {Ut Lt : Tag → Type}

def tryLoad (state : State Uu Ut Lt) (chunkName : String) {size} (data : BytesView size 1) (env : Int32 := 0) : IO Unit := do
  if ← state.load chunkName data env then
    throw $ IO.userError $ ToString.toString $ ← state.toString (-1)
