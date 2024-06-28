import Luau

open Luau

inductive Uu : Type where
| a
| b

inductive Ut : Tag → Type where
| nat (n : Nat) : Ut ⟨0, by decide⟩
| str (s : String) : Ut ⟨1, by decide⟩

inductive Lt : Tag → Type where
| bool (b : Bool) : Lt ⟨0, by decide⟩
| char (c : Char) : Lt ⟨1, by decide⟩

def source := "return 4 * 42.7"

def main : IO Unit := do
  let (.mk _ code) ← Luau.compile source <| .ofRaw {  }
  let state : Luau.State Uu Ut Lt ← Luau.State.new
  state.tryLoad "chunky" code.view 0
  state.call 0 1
  IO.println <| ← state.toStringL (-1)
