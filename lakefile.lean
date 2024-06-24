import Lake
open Lake DSL

def splitArgStr (s : String) : Array String :=
  Array.mk $ s.splitOn.filter $ not ∘ String.isEmpty

def optionCompilerBindings := get_config? bindings_cc |>.getD "cc"
def optionFlagsCompileBindings := get_config? bindings_cflags |>.getD "" |> splitArgStr
def optionManual := get_config? manual |>.isSome
def optionGit := get_config? git |>.getD "git"
def optionMake := get_config? make |>.getD "make"
def optionLuauFlags := get_config? luau_flags |>.getD "" |> splitArgStr

require pod from git "https://github.com/KislyjKisel/lean-pod" @ "24fe21d"

package luau where
  srcDir := "src"
  leanOptions := #[⟨`autoImplicit, false⟩]

lean_lib Luau where

@[default_target]
lean_exe «luau-test» where
  root := `Main
  moreLinkArgs := cond optionManual #[] #[s!"-L{__dir__}/luau/build/release/", "-lluauvm"]


/-! # Submodule -/

def tryRunProcess {m} [Monad m] [MonadError m] [MonadLiftT IO m] (sa : IO.Process.SpawnArgs) : m String := do
  let output ← IO.Process.output sa
  if output.exitCode ≠ 0 then
    error s!"'{sa.cmd}' returned {output.exitCode}: {output.stderr}"
  else
    return output.stdout

def buildLuauSubmodule' {m} [Monad m] [MonadError m] [MonadLiftT IO m] (printCmdOutput : Bool) : m Unit := do
  if optionGit != "" then
    let gitOutput ← tryRunProcess {
      cmd := optionGit
      args := #["submodule", "update", "--init", "--force", "--recursive", "luau"]
      cwd := __dir__
    }
    if printCmdOutput then IO.println gitOutput
  if optionMake != "" then
    let makeOutput ← tryRunProcess {
      cmd := optionMake
      args := #["config=release", "luau"].append optionLuauFlags
      cwd := __dir__ / "luau"
    }
    if printCmdOutput then IO.println makeOutput

script buildLuauSubmodule do
  buildLuauSubmodule' true
  return 0


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
    "-I", (pkg.dir / "luau" / "Compiler" / "include").toString,
    "-I", (pkg.dir / "ffi" / "include").toString
  ]

  match pkg.deps.find? λ dep ↦ dep.name == `pod with
    | none => error "Missing dependency 'Pod'"
    | some pod => weakArgs := weakArgs ++ #["-I", (pod.dir / "src" / "native" / "include").toString]

  if !optionManual then
    buildLuauSubmodule' false
    pure ()

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
