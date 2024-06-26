[Luau](https://github.com/luau-lang/luau)

# Compilation

* On linux `LEAN_CC` environment variable set to a C compiler may help
  with issues related to incompatibility between
  the bundled glibc (used by default when compiling Lean generated C files) and
  the system's one (used when compiling Luau).
