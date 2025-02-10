[Luau](https://github.com/luau-lang/luau) 0.660

# Options

* `manual`: if provided disables submodule build and flags.
* `git`, `cmake`: used to build the Luau submodule.
  Pass `""` to one of these to skip its execution.
* `bindings_cc`, `bindings_cflags`: compiler and flags used when building bindings.
* `luau_cc`, `luau_cxx`, `luau_flags`: c compiler, c++ compiler and cmake flags used when building Luau submodule.


# Compilation

* On linux `LEAN_CC` environment variable set to a C compiler may help
  with issues related to incompatibility between
  the bundled glibc (used by default when compiling Lean generated C files) and
  the system's one (used when compiling Luau).
  Static variants of `libc++` and `libc++abi` must be installed.
