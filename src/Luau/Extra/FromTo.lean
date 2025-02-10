import Luau.Lib

namespace Luau

variable {Uu : Type} {Ut Lt : Tag → Type}

class ToLua (Uu : Type) (Ut Lt : Tag → Type) (α : Type) where
  push : State Uu Ut Lt → α → IO Unit

class FromLua (Uu : Type) (Ut Lt : Tag → Type) (α : Type) where
  is : State Uu Ut Lt → Int32 → IO Bool
  read : State Uu Ut Lt → Int32 → IO (Option α)
  pop : State Uu Ut Lt → IO (Option α) :=
    λ state ↦ do
      let res ← read state (-1)
      state.pop
      pure res

instance : ToLua Uu Ut Lt Unit where
  push state _ := state.pushNil

instance : ToLua Uu Ut Lt Bool where
  push := State.pushBoolean

instance : ToLua Uu Ut Lt Integer where
  push := State.pushInteger

instance : ToLua Uu Ut Lt Unsigned where
  push := State.pushUnsigned

instance : ToLua Uu Ut Lt Number where
  push := State.pushNumber

instance : ToLua Uu Ut Lt String where
  push := State.pushString

instance : ToLua Uu Ut Lt Uu where
  push := State.newUserdata

instance {tag} : ToLua Uu Ut Lt (Ut tag) where
  push state := state.newUserdataTagged tag

instance {tag} : ToLua Uu Ut Lt (Lt tag) where
  push state := state.pushLightUserdataTagged tag

instance : ToLua Uu Ut Lt ByteArray where
  push state buf := state.newBuffer buf.view

instance : FromLua Uu Ut Lt Unit where
  is := State.isNil
  read state idx := do
    pure <| if ← state.isNil idx
      then some ()
      else none

instance : FromLua Uu Ut Lt Bool where
  is := State.isBoolean
  read state idx := state.toBoolean idx

instance : FromLua Uu Ut Lt Integer where
  is state idx := Option.isSome <$> state.toIntegerX idx
  read := State.toIntegerX

instance : FromLua Uu Ut Lt Unsigned where
  is state idx := Option.isSome <$> state.toUnsignedX idx
  read := State.toUnsignedX

instance : FromLua Uu Ut Lt Number where
  is := State.isNumber
  read := State.toNumberX

instance : FromLua Uu Ut Lt String where
  is := State.isString
  read := State.toString

instance : FromLua Uu Ut Lt Uu where
  is := State.isUserdata
  read := State.toUserdata

instance {tag} : FromLua Uu Ut Lt (Ut tag) where
  is state idx := (· == tag.val.toNat.toInt32) <$> state.userdataTag idx
  read state idx := state.toUserdataTagged idx tag

instance {tag} : FromLua Uu Ut Lt (Lt tag) where
  is state idx := do
    let isLt ← state.isLightUserdata idx
    if isLt
      then pure <| (← state.lightUserdataTag idx).toUInt32 == tag.val
      else pure false
  read state idx := state.toLightUserdataTagged idx tag

instance : FromLua Uu Ut Lt ByteArray where
  is := State.isBuffer
  read := State.toBuffer
