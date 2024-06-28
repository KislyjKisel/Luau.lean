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

def main : IO Unit := do
  let (.mk _ code) ← Luau.compile "local function hi(x) return x * 19827 end" <| .ofRaw {  }
  let state : Luau.State Uu Ut Lt ← Luau.State.new
  IO.println s!"CODE: {code.view.toByteArray}"
  IO.println <| ← state.load none code.view 0
