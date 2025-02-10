#include <lua.h>
#include <luau.lean.h>

LEAN_POD_DEFINE_EXTERNAL_CLASS(luau_CompileOptions)
LEAN_POD_DEFINE_EXTERNAL_CLASS(luau_State)

static void lean_luau_CompileOptions_finalize(void* data) {
    lean_luau_CompileOptions_data* data_ = data;
    lean_dec_ref(data_->owner);
    lean_pod_free((void**)data_->options.mutableGlobals);
    lean_pod_free((void**)data_->options.userdataTypes);
    lean_pod_free((void**)data_->options.disabledBuiltins);
    lean_pod_free(data);
}

static void lean_luau_CompileOptions_foreach(void* data, b_lean_obj_arg f) {
    lean_luau_CompileOptions_data* data_ = data;
    lean_inc_ref(f);
    lean_inc_ref(data_->owner);
    lean_apply_1(f, data_->owner);
}

static void lean_luau_State_finalize(void* data) {
    lean_luau_State_data* data_ = data;
    if (data_->state != NULL) {
        if (data_->main == data_) {
            lua_close(data_->state);
            for (size_t i = 0; i < LUA_UTAG_LIMIT; ++i) {
                if (data_->taggedUserdataDtors[i] != NULL) {
                    lean_dec_ref(data_->taggedUserdataDtors[i]);
                }
            }
            free(data_->taggedUserdataDtors);
            for (size_t i = 0; i < data_->referencedCount; ++i) {
                lean_dec(data_->referenced[i]);
            }
            free(data_->referenced);
            if (data_->interruptCallback != NULL) {
                lean_dec_ref(data_->interruptCallback);
            }
            if (data_->panicCallback != NULL) {
                lean_dec_ref(data_->panicCallback);
            }
        }
    }
    lean_pod_free(data_);
}

static void lean_luau_State_foreach(void* data, b_lean_obj_arg f) {
    lean_luau_State_data* data_ = data;
    if (data_->state == NULL) return;
    if (data_->main == data_) {
        lean_inc_ref_n(f, data_->referencedCount);
        for (size_t i = 0; i < data_->referencedCount; ++i) {
            lean_inc(data_->referenced[i]);
            lean_apply_1(f, data_->referenced[i]);
        }
        for (size_t i = 0; i < LUA_UTAG_LIMIT; ++i) {
            if (data_->taggedUserdataDtors[i] != NULL) {
                lean_inc_ref(f);
                lean_inc_ref(data_->taggedUserdataDtors[i]);
                lean_apply_1(f, data_->taggedUserdataDtors[i]);
            }
        }
        if (data_->interruptCallback != NULL) {
            lean_inc_ref(f);
            lean_inc_ref(data_->interruptCallback);
            lean_apply_1(f, data_->interruptCallback);
        }
        if (data_->panicCallback != NULL) {
            lean_inc_ref(f);
            lean_inc_ref(data_->panicCallback);
            lean_apply_1(f, data_->panicCallback);
        }
    }
}

LEAN_EXPORT lean_obj_res lean_luau_initialize(lean_obj_arg io_) {
    LEAN_POD_INITIALIZE_EXTERNAL_CLASS(luau_CompileOptions, lean_luau_CompileOptions_finalize, lean_luau_CompileOptions_foreach);
    LEAN_POD_INITIALIZE_EXTERNAL_CLASS(luau_State, lean_luau_State_finalize, lean_luau_State_foreach);
    return lean_io_result_mk_ok(lean_box(0));
}
