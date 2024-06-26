import Pod.Meta

namespace Luau.Config

open scoped Pod

def useLongjmp := true -- implied by the usage of C API

define_foreign_constant idSize : UInt32 := "lean_luau_config_idSize"

define_foreign_constant maxCStack : UInt32 := "lean_luau_config_maxCStack"

define_foreign_constant maxCalls : UInt32 := "lean_luau_config_maxCalls"

define_foreign_constant maxCCalls : UInt32 := "lean_luau_config_maxCCalls"

define_foreign_constant bufferSize : UInt32 := "lean_luau_config_bufferSize"

define_foreign_constant utagLimit : UInt32 := "lean_luau_config_utagLimit"

define_foreign_constant sizeClasses : UInt32 := "lean_luau_config_sizeClasses"

define_foreign_constant memoryCategories : UInt32 := "lean_luau_config_memoryCategories"

define_foreign_constant minStrTabSize : UInt32 := "lean_luau_config_minStrTabSize"

define_foreign_constant maxCaptures : UInt32 := "lean_luau_config_maxCaptures"

define_foreign_constant vectorSize : UInt32 := "lean_luau_config_vectorSize"

define_foreign_constant extraSize : UInt32 := "lean_luau_config_extraSize"
