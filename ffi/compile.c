#include <lean/lean.h>
#include <lean_pod.h>
#include <luau.lean.h>

LEAN_EXPORT lean_luau_CompileOptions lean_luau_CompileOptions_bake(lean_obj_arg raw) {
    lean_luau_CompileOptions_data* data = lean_pod_alloc(sizeof(lean_luau_CompileOptions_data));
    data->owner = raw;
    data->options.optimizationLevel = LEAN_POD_CTOR_GET(raw, LEAN_LUAU_CompileOptions_optimizationLevel);
    data->options.debugLevel = LEAN_POD_CTOR_GET(raw, LEAN_LUAU_CompileOptions_debugLevel);
    data->options.typeInfoLevel = LEAN_POD_CTOR_GET(raw, LEAN_LUAU_CompileOptions_typeInfoLevel);
    data->options.coverageLevel = LEAN_POD_CTOR_GET(raw, LEAN_LUAU_CompileOptions_coverageLevel);

    lean_object* mutableGlobals = LEAN_POD_CTOR_GET(raw, LEAN_LUAU_CompileOptions_mutableGlobals);
    size_t mutableGlobalsCount = lean_array_size(mutableGlobals);
    const char** mutableGlobals_c = lean_pod_alloc((1 + mutableGlobalsCount) * sizeof(char*));
    for (size_t i = 0; i < mutableGlobalsCount; ++i) {
        mutableGlobals_c[i] = lean_string_cstr(lean_array_get_core(mutableGlobals, i));
    }
    mutableGlobals_c[mutableGlobalsCount] = NULL;
    data->options.mutableGlobals = (const char* const*)mutableGlobals_c;

    lean_object* userdataTypes = LEAN_POD_CTOR_GET(raw, LEAN_LUAU_CompileOptions_userdataTypes);
    size_t userdataTypesCount = lean_array_size(userdataTypes);
    const char** userdataTypes_c = lean_pod_alloc((1 + userdataTypesCount) * sizeof(char*));
    for (size_t i = 0; i < userdataTypesCount; ++i) {
        userdataTypes_c[i] = lean_string_cstr(lean_array_get_core(userdataTypes, i));
    }
    userdataTypes_c[userdataTypesCount] = NULL;
    data->options.userdataTypes = (const char* const*)userdataTypes_c;

    data->options.vectorCtor = NULL;
    data->options.vectorLib = NULL;
    data->options.vectorType = NULL;
    return lean_alloc_external(lean_luau_CompileOptions_class, data);
}

LEAN_EXPORT lean_obj_res lean_luau_compile(b_lean_obj_arg source, lean_luau_CompileOptions options) {
    size_t size;
    char* data = luau_compile(
        lean_string_cstr(source),
        lean_string_byte_size(source),
        &lean_luau_CompileOptions_fromRepr(options)->options,
        &size
    );
    return lean_io_result_mk_ok(lean_mk_tuple2(
        lean_usize_to_nat(size),
        lean_pod_Buffer_box(data, free)
    ));
}
