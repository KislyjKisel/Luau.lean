import Lake
open Lake DSL

package luau where

lean_lib Luau where

@[default_target]
lean_exe «luau-test» where
  root := `Main
