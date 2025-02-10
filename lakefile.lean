import Lake
open Lake DSL

def splitArgStr (s : String) : Array String :=
  Array.mk $ s.splitOn.filter $ not ∘ String.isEmpty

def optionBindingsCompiler := get_config? bindings_cc |>.getD "cc"
def optionBindingsCompilerFlags := get_config? bindings_cflags |>.getD "" |> splitArgStr
def optionManual := get_config? manual |>.isSome
def optionGit := get_config? git |>.getD "git"
def optionCMake := get_config? cmake |>.getD "cmake"
def optionLuauFlags := get_config? luau_flags |>.getD "" |> splitArgStr
def optionLuauCCompiler := get_config? luau_cc |>.getD "cc"
def optionLuauCppCompiler := get_config? luau_cxx |>.getD "clang++"

require pod from git "https://github.com/KislyjKisel/lean-pod" @ "adfbcd4"

package luau where
  srcDir := "src"
  leanOptions := #[⟨`autoImplicit, false⟩]
  precompileModules := true

@[default_target]
lean_lib Luau where

@[test_driver]
lean_exe «luau-test» where
  root := `Main
  moreLinkArgs :=
    cond optionManual
      #[]
      #[s!"-L{__dir__}/luau/build/", "-lLuau.VM", "-lLuau.Compiler", "-lLuau.Ast"]


/-! # Submodule -/

def tryRunProcess {m} [Monad m] [MonadError m] [MonadLiftT IO m] (sa : IO.Process.SpawnArgs) : m String := do
  let output ← IO.Process.output sa
  if output.exitCode ≠ 0 then
    error s!"'{sa.cmd}' returned {output.exitCode}: {output.stderr}"
  else
    return output.stdout

def buildSubmodule' {m} [Monad m] [MonadError m] [MonadLiftT IO m] (printCmdOutput : Bool) : m Unit := do
  if optionGit != "" then
    let gitOutput ← tryRunProcess {
      cmd := optionGit
      args := #["submodule", "update", "--init", "--force", "--recursive", "luau"]
      cwd := __dir__
    }
    if printCmdOutput then IO.println gitOutput

  IO.FS.createDirAll <| __dir__ / "luau" / "build"
  if optionCMake != "" then
    let makeOutput ← tryRunProcess {
      cmd := optionCMake
      args := #[
        s!"-DCMAKE_CXX_COMPILER={optionLuauCppCompiler}",
        "-DCMAKE_CXX_FLAGS=-stdlib=libc++",
        s!"-DCMAKE_C_COMPILER={optionLuauCCompiler}",
        "-DCMAKE_POSITION_INDEPENDENT_CODE=ON",
        "-DLUAU_EXTERN_C=ON",
        "-DLUAU_BUILD_CLI=OFF",
        "-DLUAU_BUILD_TESTS=OFF",
        "-DLUAU_BUILD_WEB=OFF",
        "-DCMAKE_BUILD_TYPE=Release",
        ".."
      ].append optionLuauFlags
      cwd := __dir__ / "luau" / "build"
      -- env := #[("LD_LIBRARY_PATH", none)]
    }
    if printCmdOutput then IO.println makeOutput
    let cmakeBuildOutput ← tryRunProcess {
      cmd := optionCMake
      args := #["--build", ".", "--target", "Luau.VM", "Luau.Compiler", "--config", "Release"]
      cwd := __dir__ / "luau" / "build"
    }
    if printCmdOutput then IO.println cmakeBuildOutput

script buildSubmodule do
  buildSubmodule' true
  return 0


/-! # Bindings -/

def bindingsSources := #[
  "initialization",
  "config",
  "compile",
  "core"
]

extern_lib «luau-lean» pkg := do
  let name := nameToStaticLib "luau-lean"
  let mut weakArgs := #["-I", (← getLeanIncludeDir).toString]
  let mut traceArgs := optionBindingsCompilerFlags.append #[
    "-fPIC",
    "-I", (pkg.dir / "ffi" / "include").toString
  ]
  let mut extraTraceFiles : Array System.FilePath := #[
    __dir__ / "ffi" / "include" / "luau.lean.h"
  ]
  if !optionManual then
    traceArgs := traceArgs.append #[
      "-I", (pkg.dir / "luau" / "VM" / "include").toString,
      "-I", (pkg.dir / "luau" / "Compiler" / "include").toString
    ]

  match ← findPackage? `pod with
  | none => error "Missing dependency 'Pod'"
  | some pod => do
    if h: 0 < pod.externLibs.size
      then
        weakArgs := weakArgs ++ #["-I", (pod.dir / "ffi" / "include").toString]
        extraTraceFiles := extraTraceFiles.push <| pod.dir / "ffi" / "include" / "lean_pod.h"
        pure pod.externLibs[0]
      else
        error "Can't find Pod's bindings"

  if !optionManual then
    buildSubmodule' false

  let nativeSrcDir := pkg.dir / "ffi"
  let objectFileDir := pkg.buildDir / "ffi"
  buildStaticLib (pkg.nativeLibDir / name)
    (← bindingsSources.mapM λ suffix ↦ do
        buildO
          (objectFileDir / (suffix ++ ".o"))
          (← inputTextFile $ nativeSrcDir / (suffix ++ ".c"))
          weakArgs traceArgs
          optionBindingsCompiler
          (computeTrace extraTraceFiles))
