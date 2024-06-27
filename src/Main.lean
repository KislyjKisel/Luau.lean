import Luau

def main : IO Unit := do
  let (.mk _ code) ← Luau.compile "local function hi(x) return x * 19827 end" <| .ofRaw {  }
  let state : Luau.State Unit Unit ← Luau.State.new
  IO.println s!"CODE: {code.view.toByteArray}"
  IO.println <| ← state.load none code.view 0
