#include <lean/lean.h>
#include <lean_pod.h>
#include <lua.h>
#include <lualib.h>
#include <luau.lean.h>

static lean_object* lean_luau_State_box(lean_luau_State_data* data) {
    return lean_alloc_external(lean_luau_State_class, (void*)data);
}

static lean_object* lean_luau_ioerr(const char* errMsg) {
    return lean_io_result_mk_error(lean_mk_io_user_error(lean_mk_string(errMsg)));
}

#define lean_luau_guard_valid(data)\
    if ((data)->state == NULL) {\
        return lean_luau_ioerr("State is invalid (was closed).");\
    }

#define lean_luau_guard_interpreter(data)\
    if (!(data)->isInterpreter) {\
        return lean_luau_ioerr("Expected interpreter state.");\
    }

#define lean_luau_guard_thread(data)\
    if ((data)->isInterpreter) {\
        return lean_luau_ioerr("Expected thread state.");\
    }

// Constants

LEAN_EXPORT uint32_t lean_luau_const_multret(lean_obj_arg unit_) {
    return (int32_t)LUA_MULTRET;
}

LEAN_EXPORT uint32_t lean_luau_const_registryIndex(lean_obj_arg unit_) {
    return (int32_t)LUA_REGISTRYINDEX;
}

LEAN_EXPORT uint32_t lean_luau_const_environIndex(lean_obj_arg unit_) {
    return (int32_t)LUA_ENVIRONINDEX;
}

LEAN_EXPORT uint32_t lean_luau_const_globalsIndex(lean_obj_arg unit_) {
    return (int32_t)LUA_GLOBALSINDEX;
}


// State manipulation

static void* lean_luau_alloc(void* ud, void* ptr, size_t osize, size_t nsize) {
    // TODO: make configurable and/or use lean's allocator
    if (nsize == 0) {
        free(ptr);
        return NULL;
    }
    return realloc(ptr, nsize);
}

LEAN_EXPORT lean_obj_res lean_luau_State_new(lean_obj_arg io_) {
    lean_luau_State_data* data = lean_pod_alloc(sizeof(lean_luau_State_data));
    data->state = lua_newstate(lean_luau_alloc, NULL);
    data->isInterpreter = true;
    return lean_io_result_mk_ok(lean_luau_State_box(data));
}

LEAN_EXPORT lean_obj_res lean_luau_State_close(b_lean_obj_arg state, lean_obj_arg io_) {
    lean_luau_State_data* data = lean_luau_State_fromRepr(state);
    lean_luau_guard_valid(data);
    lean_luau_guard_interpreter(data);
    lua_close(data->state);
    return lean_io_result_mk_ok(lean_box(0));
}

// @[extern "lean_luau_State_close"]
// opaque close (state : @& State) : BaseIO Unit

LEAN_EXPORT lean_obj_res lean_luau_State_newThread(lean_luau_State state, lean_obj_arg io_) {
    lean_luau_State_data* data = lean_luau_State_fromRepr(state);
    lean_luau_guard_valid(data);
    lean_luau_State_data* newThread = lean_pod_alloc(sizeof(lean_luau_State_data));
    newThread->state = lua_newthread(data->state);
    newThread->isInterpreter = false;
    return lean_io_result_mk_ok(lean_luau_State_box(newThread));
}

LEAN_EXPORT lean_obj_res lean_luau_State_mainThread(lean_luau_State state, lean_obj_arg io_) {
    lean_luau_State_data* data = lean_luau_State_fromRepr(state);
    lean_luau_guard_valid(data);
    lean_luau_State_data* newThread = lean_pod_alloc(sizeof(lean_luau_State_data));
    newThread->state = lua_mainthread(data->state);
    newThread->isInterpreter = false;
    return lean_io_result_mk_ok(lean_luau_State_box(newThread));
}

LEAN_EXPORT lean_obj_res lean_luau_State_resetThread(lean_luau_State state, lean_obj_arg io_) {
    lean_luau_State_data* data = lean_luau_State_fromRepr(state);
    lean_luau_guard_valid(data);
    lean_luau_guard_thread(data);
    lua_resetthread(data->state);
    return lean_io_result_mk_ok(lean_box(0));
}

LEAN_EXPORT lean_obj_res lean_luau_State_isThreadReset(lean_luau_State state, lean_obj_arg io_) {
    lean_luau_State_data* data = lean_luau_State_fromRepr(state);
    lean_luau_guard_valid(data);
    return lean_io_result_mk_ok(lean_box(lua_isthreadreset(data->state)));
}

LEAN_EXPORT lean_obj_res lean_luau_State_isValid(lean_luau_State state, lean_obj_arg io_) {
    lean_luau_State_data* data = lean_luau_State_fromRepr(state);
    return lean_io_result_mk_ok(lean_box(data->state != NULL));
}


// Basic stack manipulation

LEAN_EXPORT lean_obj_res lean_luau_State_absIndex(lean_luau_State state, uint32_t idx, lean_obj_arg io_) {
    lean_luau_State_data* data = lean_luau_State_fromRepr(state);
    lean_luau_guard_valid(data);
    return lean_io_result_mk_ok(lean_box_uint32((int32_t)lua_absindex(data->state, (int32_t)idx)));
}

LEAN_EXPORT lean_obj_res lean_luau_State_getTop(lean_luau_State state, lean_obj_arg io_) {
    lean_luau_State_data* data = lean_luau_State_fromRepr(state);
    lean_luau_guard_valid(data);
    return lean_io_result_mk_ok(lean_box_uint32((int32_t)lua_gettop(data->state)));
}

LEAN_EXPORT lean_obj_res lean_luau_State_setTop(lean_luau_State state, uint32_t idx, lean_obj_arg io_) {
    lean_luau_State_data* data = lean_luau_State_fromRepr(state);
    lean_luau_guard_valid(data);
    lua_settop(data->state, (int32_t)idx);
    return lean_io_result_mk_ok(lean_box(0));
}

LEAN_EXPORT lean_obj_res lean_luau_State_pushValue(lean_luau_State state, uint32_t idx, lean_obj_arg io_) {
    lean_luau_State_data* data = lean_luau_State_fromRepr(state);
    lean_luau_guard_valid(data);
    lua_pushvalue(data->state, (int32_t)idx);
    return lean_io_result_mk_ok(lean_box(0));
}

LEAN_EXPORT lean_obj_res lean_luau_State_remove(lean_luau_State state, uint32_t idx, lean_obj_arg io_) {
    lean_luau_State_data* data = lean_luau_State_fromRepr(state);
    lean_luau_guard_valid(data);
    lua_remove(data->state, (int32_t)idx);
    return lean_io_result_mk_ok(lean_box(0));
}

LEAN_EXPORT lean_obj_res lean_luau_State_insert(lean_luau_State state, uint32_t idx, lean_obj_arg io_) {
    lean_luau_State_data* data = lean_luau_State_fromRepr(state);
    lean_luau_guard_valid(data);
    lua_insert(data->state, (int32_t)idx);
    return lean_io_result_mk_ok(lean_box(0));
}

LEAN_EXPORT lean_obj_res lean_luau_State_replace(lean_luau_State state, uint32_t idx, lean_obj_arg io_) {
    lean_luau_State_data* data = lean_luau_State_fromRepr(state);
    lean_luau_guard_valid(data);
    lua_replace(data->state, (int32_t)idx);
    return lean_io_result_mk_ok(lean_box(0));
}

LEAN_EXPORT lean_obj_res lean_luau_State_checkStack(lean_luau_State state, uint32_t sz, lean_obj_arg io_) {
    lean_luau_State_data* data = lean_luau_State_fromRepr(state);
    lean_luau_guard_valid(data);
    return lean_io_result_mk_ok(lean_box(lua_checkstack(data->state, (int32_t)sz) != 0));
}

LEAN_EXPORT lean_obj_res lean_luau_State_rawCheckStack(lean_luau_State state, uint32_t sz, lean_obj_arg io_) {
    lean_luau_State_data* data = lean_luau_State_fromRepr(state);
    lean_luau_guard_valid(data);
    lua_rawcheckstack(data->state, (int32_t)sz);
    return lean_io_result_mk_ok(lean_box(0));
}

LEAN_EXPORT lean_obj_res lean_luau_State_xmove(lean_luau_State from, lean_luau_State to, uint32_t n, lean_obj_arg io_) {
    lean_luau_State_data* from_data = lean_luau_State_fromRepr(from);
    lean_luau_State_data* to_data = lean_luau_State_fromRepr(to);
    lean_luau_guard_valid(from_data);
    lean_luau_guard_valid(to_data);
    lua_xmove(from_data->state, to_data->state, (int32_t)n);
    return lean_io_result_mk_ok(lean_box(0));
}

LEAN_EXPORT lean_obj_res lean_luau_State_xpush(lean_luau_State from, lean_luau_State to, uint32_t n, lean_obj_arg io_) {
    lean_luau_State_data* from_data = lean_luau_State_fromRepr(from);
    lean_luau_State_data* to_data = lean_luau_State_fromRepr(to);
    lean_luau_guard_valid(from_data);
    lean_luau_guard_valid(to_data);
    lua_xpush(from_data->state, to_data->state, (int32_t)n);
    return lean_io_result_mk_ok(lean_box(0));
}


// Access functions


// Push functions


// Get functions


// Set functions


// Load and call functions

LEAN_EXPORT lean_obj_res lean_luau_State_load(
    lean_luau_State state, b_lean_obj_arg chunkName, b_lean_obj_arg size,
    lean_pod_BytesView data, uint32_t env, lean_obj_arg io_
) {
    lean_luau_State_data* sdata = lean_luau_State_fromRepr(state);
    lean_luau_guard_valid(sdata);
    return lean_io_result_mk_ok(lean_box_uint32((int32_t)luau_load(
        sdata->state,
        lean_option_is_some(chunkName) ? lean_string_cstr(chunkName) : NULL,
        lean_pod_BytesView_unbox(data)->ptr,
        lean_usize_of_nat(size),
        (int32_t)env)
    ));
}

LEAN_EXPORT lean_obj_res lean_luau_State_call(lean_luau_State state, uint32_t nArgs, uint32_t nResults, lean_obj_arg io_) {
    lean_luau_State_data* data = lean_luau_State_fromRepr(state);
    lean_luau_guard_valid(data);
    lua_call(data->state, (int32_t)nArgs, (int32_t)nResults);
    return lean_io_result_mk_ok(lean_box(0));
}

LEAN_EXPORT lean_obj_res lean_luau_State_pcall(lean_luau_State state, uint32_t nArgs, uint32_t nResults, uint32_t errFunc, lean_obj_arg io_) {
    lean_luau_State_data* data = lean_luau_State_fromRepr(state);
    lean_luau_guard_valid(data);
    return lean_io_result_mk_ok(lean_box_uint32((int32_t)
        lua_pcall(data->state, (int32_t)nArgs, (int32_t)nResults, (int32_t)errFunc)
    ));
}
