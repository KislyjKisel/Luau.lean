#include <lean/lean.h>
#include <luaconf.h>

LEAN_EXPORT uint32_t lean_luau_config_idSize(lean_obj_arg unit_) {
    return LUA_IDSIZE;
}

LEAN_EXPORT uint32_t lean_luau_config_maxCStack(lean_obj_arg unit_) {
    return LUAI_MAXCSTACK;
}

LEAN_EXPORT uint32_t lean_luau_config_maxCalls(lean_obj_arg unit_) {
    return LUAI_MAXCALLS;
}

LEAN_EXPORT uint32_t lean_luau_config_maxCCalls(lean_obj_arg unit_) {
    return LUAI_MAXCCALLS;
}

LEAN_EXPORT uint32_t lean_luau_config_bufferSize(lean_obj_arg unit_) {
    return LUA_BUFFERSIZE;
}

LEAN_EXPORT uint32_t lean_luau_config_utagLimit(lean_obj_arg unit_) {
    return LUA_UTAG_LIMIT;
}

LEAN_EXPORT uint32_t lean_luau_config_sizeClasses(lean_obj_arg unit_) {
    return LUA_SIZECLASSES;
}

LEAN_EXPORT uint32_t lean_luau_config_memoryCategories(lean_obj_arg unit_) {
    return LUA_MEMORY_CATEGORIES;
}

LEAN_EXPORT uint32_t lean_luau_config_minStrTabSize(lean_obj_arg unit_) {
    return LUA_MINSTRTABSIZE;
}

LEAN_EXPORT uint32_t lean_luau_config_maxCaptures(lean_obj_arg unit_) {
    return LUA_MAXCAPTURES;
}

LEAN_EXPORT uint32_t lean_luau_config_vectorSize(lean_obj_arg unit_) {
    return LUA_VECTOR_SIZE;
}

LEAN_EXPORT uint32_t lean_luau_config_extraSize(lean_obj_arg unit_) {
    return LUA_EXTRA_SIZE;
}
