import Lake
open Lake DSL

def splitArgStr (s : String) : Array String :=
  Array.mk $ s.splitOn.filter $ not ∘ String.isEmpty

def optionCompilerBindings := (get_config? bindings_cc).getD "cc"
def optionFlagsCompileBindings := splitArgStr $ (get_config? bindings_cflags).getD ""

require pod from git "https://github.com/KislyjKisel/lean-pod" @ "24fe21d"

package luau where
  srcDir := "src"
  leanOptions := #[⟨`autoImplicit, false⟩]

lean_lib Luau where

@[default_target]
lean_exe «luau-test» where
  root := `Main


/-! # Bindings -/

def bindingsSources := #[
  "core"
]

def bindingsExtras : Array String := #[
  "ffi/include/luau.lean.h"
]

extern_lib «luau-lean» pkg := do
  let name := nameToStaticLib "luau-lean"
  let mut weakArgs := #["-I", (← getLeanIncludeDir).toString]
  let mut traceArgs := optionFlagsCompileBindings.append #[
    "-fPIC",
    "-I", (pkg.dir / "luau" / "VM" / "include").toString,
    "-I", (pkg.dir / "ffi" / "include").toString
  ]

  match pkg.deps.find? λ dep ↦ dep.name == `pod with
    | none => error "Missing dependency 'Pod'"
    | some pod => weakArgs := weakArgs ++ #["-I", (pod.dir / "src" / "native" / "include").toString]

  let nativeSrcDir := pkg.dir / "ffi"
  let objectFileDir := pkg.irDir / "ffi"
  let extraTrace ← mixTraceArray <$> (bindingsExtras.mapM $ λ h ↦ computeTrace (pkg.dir / ⟨h⟩))
  buildStaticLib (pkg.nativeLibDir / name)
    (#[].append
      (← bindingsSources.mapM λ suffix ↦ do
        buildO
          (objectFileDir / (suffix ++ ".o"))
          (← inputFile $ nativeSrcDir / (suffix ++ ".c"))
          weakArgs traceArgs
          optionCompilerBindings
          (pure extraTrace)
      ))
