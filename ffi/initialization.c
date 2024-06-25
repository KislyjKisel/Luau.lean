#include <luau.lean.h>

LEAN_POD_DEFINE_EXTERNAL_CLASS(luau_CompileOptions)

static void lean_luau_CompileOptions_finalize(void* data) {
    lean_luau_CompileOptions_data* data_ = data;
    lean_dec_ref(data_->owner);
    lean_pod_free((char*)data_->options.mutableGlobals);
    lean_pod_free((char*)data_->options.userdataTypes);
    lean_pod_free(data);
}

static void lean_luau_CompileOptions_foreach(void* data, b_lean_obj_arg f) {
    lean_luau_CompileOptions_data* data_ = data;
    lean_inc_ref(f);
    lean_inc_ref(data_->owner);
    lean_apply_1(f, data_->owner);
}

LEAN_EXPORT lean_obj_res lean_luau_initialize(lean_obj_arg io_) {
    LEAN_POD_INITIALIZE_EXTERNAL_CLASS(luau_CompileOptions, lean_luau_CompileOptions_finalize, lean_luau_CompileOptions_foreach);
    return lean_io_result_mk_ok(lean_box(0));
}
