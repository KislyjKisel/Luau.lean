#include <lean/lean.h>
#include <lean_pod.h>
#include <lua.h>
#include <lualib.h>
#include <luau.lean.h>

static lean_object* lean_luau_State_box(lean_luau_State_data* data) {
    data->referenced = NULL;
    data->referencedCount = 0;
    data->referencedCapacity = 0;
    return lean_alloc_external(lean_luau_State_class, (void*)data);
}

static inline void lean_luau_State_reference(lean_luau_State_data* data, lean_obj_arg obj) {
    if (data->referencedCount >= data->referencedCapacity) {
        size_t newCapacity = data->referencedCapacity == 0 ? 4 : (data->referencedCapacity * 2);
        data->referenced = realloc(data->referenced, newCapacity);
        data->referencedCapacity = newCapacity;
    }
    data->referenced[data->referencedCount++] = obj;
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
    data->state = NULL;
    for (size_t i = 0; i < data->referencedCount; ++i) {
        lean_dec(data->referenced[i]);
    }
    free(data->referenced);
    data->referenced = NULL;
    data->referencedCount = 0;
    data->referencedCapacity = 0;
    return lean_io_result_mk_ok(lean_box(0));
}

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
    data->state = NULL;
    for (size_t i = 0; i < data->referencedCount; ++i) {
        lean_dec(data->referenced[i]);
    }
    free(data->referenced);
    data->referenced = NULL;
    data->referencedCount = 0;
    data->referencedCapacity = 0;
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

LEAN_EXPORT lean_obj_res lean_luau_State_pop(lean_luau_State state, uint32_t n, lean_obj_arg io_) {
    lean_luau_State_data* data = lean_luau_State_fromRepr(state);
    lean_luau_guard_valid(data);
    lua_pop(data->state, (int32_t)n);
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

LEAN_EXPORT lean_obj_res lean_luau_State_isNumber(lean_luau_State state, uint32_t idx, lean_obj_arg io_) {
    lean_luau_State_data* data = lean_luau_State_fromRepr(state);
    lean_luau_guard_valid(data);
    return lean_io_result_mk_ok(lean_box(lua_isnumber(data->state, (int32_t)idx) != 0));
}

LEAN_EXPORT lean_obj_res lean_luau_State_isString(lean_luau_State state, uint32_t idx, lean_obj_arg io_) {
    lean_luau_State_data* data = lean_luau_State_fromRepr(state);
    lean_luau_guard_valid(data);
    return lean_io_result_mk_ok(lean_box(lua_isstring(data->state, (int32_t)idx) != 0));
}

LEAN_EXPORT lean_obj_res lean_luau_State_isCFunction(lean_luau_State state, uint32_t idx, lean_obj_arg io_) {
    lean_luau_State_data* data = lean_luau_State_fromRepr(state);
    lean_luau_guard_valid(data);
    return lean_io_result_mk_ok(lean_box(lua_iscfunction(data->state, (int32_t)idx) != 0));
}

LEAN_EXPORT lean_obj_res lean_luau_State_isLFunction(lean_luau_State state, uint32_t idx, lean_obj_arg io_) {
    lean_luau_State_data* data = lean_luau_State_fromRepr(state);
    lean_luau_guard_valid(data);
    return lean_io_result_mk_ok(lean_box(lua_isLfunction(data->state, (int32_t)idx) != 0));
}

LEAN_EXPORT lean_obj_res lean_luau_State_isUserdata(lean_luau_State state, uint32_t idx, lean_obj_arg io_) {
    lean_luau_State_data* data = lean_luau_State_fromRepr(state);
    lean_luau_guard_valid(data);
    return lean_io_result_mk_ok(lean_box(lua_isuserdata(data->state, (int32_t)idx) != 0));
}

LEAN_EXPORT lean_obj_res lean_luau_State_type(lean_luau_State state, uint32_t idx, lean_obj_arg io_) {
    lean_luau_State_data* data = lean_luau_State_fromRepr(state);
    lean_luau_guard_valid(data);
    int ty = lua_type(data->state, (int32_t)idx);
    return lean_io_result_mk_ok(ty == LUA_TNONE ? lean_mk_option_none() : lean_mk_option_some(lean_box(ty)));
}

LEAN_EXPORT lean_obj_res lean_luau_State_typeName(lean_luau_State state, uint8_t tp, lean_obj_arg io_) {
    lean_luau_State_data* data = lean_luau_State_fromRepr(state);
    lean_luau_guard_valid(data);
    return lean_io_result_mk_ok(lean_mk_string(lua_typename(data->state, tp)));
}

LEAN_EXPORT lean_obj_res lean_luau_State_equal(lean_luau_State state, uint32_t idx1, uint32_t idx2, lean_obj_arg io_) {
    lean_luau_State_data* data = lean_luau_State_fromRepr(state);
    lean_luau_guard_valid(data);
    return lean_io_result_mk_ok(lean_box(lua_equal(data->state, (int32_t)idx1, (int32_t)idx2)));
}

LEAN_EXPORT lean_obj_res lean_luau_State_rawEqual(lean_luau_State state, uint32_t idx1, uint32_t idx2, lean_obj_arg io_) {
    lean_luau_State_data* data = lean_luau_State_fromRepr(state);
    lean_luau_guard_valid(data);
    return lean_io_result_mk_ok(lean_box(lua_rawequal(data->state, (int32_t)idx1, (int32_t)idx2)));
}

LEAN_EXPORT lean_obj_res lean_luau_State_lessThan(lean_luau_State state, uint32_t idx1, uint32_t idx2, lean_obj_arg io_) {
    lean_luau_State_data* data = lean_luau_State_fromRepr(state);
    lean_luau_guard_valid(data);
    return lean_io_result_mk_ok(lean_box(lua_lessthan(data->state, (int32_t)idx1, (int32_t)idx2)));
}

LEAN_EXPORT lean_obj_res lean_luau_State_toNumberX(lean_luau_State state, uint32_t idx, lean_obj_arg io_) {
    lean_luau_State_data* data = lean_luau_State_fromRepr(state);
    lean_luau_guard_valid(data);
    int isnum;
    double val = lua_tonumberx(data->state, (int32_t)idx, &isnum);
    if (isnum == 0) {
        return lean_io_result_mk_ok(lean_mk_option_none());
    }
    return lean_io_result_mk_ok(lean_mk_option_some(lean_box_float(val)));
}

LEAN_EXPORT lean_obj_res lean_luau_State_toNumber(lean_luau_State state, uint32_t idx, lean_obj_arg io_) {
    lean_luau_State_data* data = lean_luau_State_fromRepr(state);
    lean_luau_guard_valid(data);
    return lean_io_result_mk_ok(lean_box_float(lua_tonumber(data->state, (int32_t)idx)));
}

LEAN_EXPORT lean_obj_res lean_luau_State_toIntegerX(lean_luau_State state, uint32_t idx, lean_obj_arg io_) {
    lean_luau_State_data* data = lean_luau_State_fromRepr(state);
    lean_luau_guard_valid(data);
    int isnum;
    int val = lua_tointegerx(data->state, (int32_t)idx, &isnum);
    if (isnum == 0) {
        return lean_io_result_mk_ok(lean_mk_option_none());
    }
    return lean_io_result_mk_ok(lean_mk_option_some(lean_box_uint32((int32_t)val)));
}

LEAN_EXPORT lean_obj_res lean_luau_State_toInteger(lean_luau_State state, uint32_t idx, lean_obj_arg io_) {
    lean_luau_State_data* data = lean_luau_State_fromRepr(state);
    lean_luau_guard_valid(data);
    return lean_io_result_mk_ok(lean_box_uint32((int32_t)lua_tointeger(data->state, (int32_t)idx)));
}

LEAN_EXPORT lean_obj_res lean_luau_State_toUnsignedX(lean_luau_State state, uint32_t idx, lean_obj_arg io_) {
    lean_luau_State_data* data = lean_luau_State_fromRepr(state);
    lean_luau_guard_valid(data);
    int isnum;
    unsigned int val = lua_tounsignedx(data->state, (int32_t)idx, &isnum);
    if (isnum == 0) {
        return lean_io_result_mk_ok(lean_mk_option_none());
    }
    return lean_io_result_mk_ok(lean_mk_option_some(lean_box_uint32(val)));
}

LEAN_EXPORT lean_obj_res lean_luau_State_toUnsigned(lean_luau_State state, uint32_t idx, lean_obj_arg io_) {
    lean_luau_State_data* data = lean_luau_State_fromRepr(state);
    lean_luau_guard_valid(data);
    return lean_io_result_mk_ok(lean_box_uint32(lua_tounsigned(data->state, (int32_t)idx)));
}

LEAN_EXPORT lean_obj_res lean_luau_State_toBoolean(lean_luau_State state, uint32_t idx, lean_obj_arg io_) {
    lean_luau_State_data* data = lean_luau_State_fromRepr(state);
    lean_luau_guard_valid(data);
    return lean_io_result_mk_ok(lean_box(lua_toboolean(data->state, (int32_t)idx) != 0));
}

LEAN_EXPORT lean_obj_res lean_luau_State_toString(lean_luau_State state, uint32_t idx, lean_obj_arg io_) {
    lean_luau_State_data* data = lean_luau_State_fromRepr(state);
    lean_luau_guard_valid(data);
    const char* s = lua_tostring(data->state, (int32_t)idx);
    if (s == NULL) {
        return lean_io_result_mk_ok(lean_mk_option_none());
    }
    return lean_io_result_mk_ok(lean_mk_option_some(lean_mk_string(s)));
}

LEAN_EXPORT lean_obj_res lean_luau_State_objLen(lean_luau_State state, uint32_t idx, lean_obj_arg io_) {
    lean_luau_State_data* data = lean_luau_State_fromRepr(state);
    lean_luau_guard_valid(data);
    return lean_io_result_mk_ok(lean_box_uint32((int32_t)
        lua_objlen(data->state, (int32_t)idx)
    ));
}

static lean_obj_res lean_luau_CFunction_boxer(lean_obj_arg cfn, lean_luau_State state, lean_obj_arg io_) {
    lean_luau_State_data* data = lean_luau_State_unbox(state);
    lean_luau_guard_valid(data);
    ((lua_CFunction)lean_pod_Ptr_unbox(cfn))(data->state);
    lean_dec_ref(state);
    lean_dec_ref(cfn);
    return lean_io_result_mk_ok(lean_box(0));
}

LEAN_EXPORT lean_obj_res lean_luau_State_toCFunction(lean_luau_State state, uint32_t idx, lean_obj_arg io_) {
    lean_luau_State_data* data = lean_luau_State_fromRepr(state);
    lean_luau_guard_valid(data);
    lua_CFunction fn = lua_tocfunction(data->state, (int32_t)idx);
    if (fn == NULL) return lean_io_result_mk_ok(lean_mk_option_none());
    lean_object* obj = lean_alloc_closure(lean_luau_CFunction_boxer, 3, 1);
    lean_closure_arg_cptr(obj)[0] = lean_pod_Ptr_box((void*)fn);
    return lean_io_result_mk_ok(lean_mk_option_some(obj));
}

LEAN_EXPORT lean_obj_res lean_luau_State_toLightUserdata(lean_luau_State state, uint32_t idx, lean_obj_arg io_) {
    lean_luau_State_data* data = lean_luau_State_fromRepr(state);
    lean_luau_guard_valid(data);
    lean_object* val = lua_tolightuserdata(data->state, (int32_t)idx);
    if (val == NULL) {
        return lean_io_result_mk_ok(lean_mk_option_none());
    }
    lean_inc(val);
    return lean_io_result_mk_ok(lean_mk_option_some(val));
}

LEAN_EXPORT lean_obj_res lean_luau_State_toLightUserdataTagged(lean_luau_State state, uint32_t idx, uint32_t tag, lean_obj_arg io_) {
    lean_luau_State_data* data = lean_luau_State_fromRepr(state);
    lean_luau_guard_valid(data);
    lean_object* val = lua_tolightuserdatatagged(data->state, (int32_t)idx, (int32_t)tag);
    if (val == NULL) {
        return lean_io_result_mk_ok(lean_mk_option_none());
    }
    lean_inc(val);
    return lean_io_result_mk_ok(lean_mk_option_some(val));
}

LEAN_EXPORT lean_obj_res lean_luau_State_toUserdata(lean_luau_State state, uint32_t idx, lean_obj_arg io_) {
    lean_luau_State_data* data = lean_luau_State_fromRepr(state);
    lean_luau_guard_valid(data);
    lean_object** val_ptr = lua_touserdata(data->state, (int32_t)idx);
    if (val_ptr == NULL) {
        return lean_io_result_mk_ok(lean_mk_option_none());
    }
    lean_object* val = *val_ptr;
    lean_inc(val);
    return lean_io_result_mk_ok(lean_mk_option_some(val));
}

LEAN_EXPORT lean_obj_res lean_luau_State_toUserdataTagged(lean_luau_State state, uint32_t idx, uint32_t tag, lean_obj_arg io_) {
    lean_luau_State_data* data = lean_luau_State_fromRepr(state);
    lean_luau_guard_valid(data);
    lean_object** val_ptr = lua_touserdatatagged(data->state, (int32_t)idx, (int32_t)tag);
    if (val_ptr == NULL) {
        return lean_io_result_mk_ok(lean_mk_option_none());
    }
    lean_object* val = *val_ptr;
    lean_inc(val);
    return lean_io_result_mk_ok(lean_mk_option_some(val));
}

LEAN_EXPORT lean_obj_res lean_luau_State_userdataTag(lean_luau_State state, uint32_t idx, lean_obj_arg io_) {
    lean_luau_State_data* data = lean_luau_State_fromRepr(state);
    lean_luau_guard_valid(data);
    return lean_io_result_mk_ok(lean_box_uint32((int32_t)
        lua_userdatatag(data->state, (int32_t)idx)
    ));
}

LEAN_EXPORT lean_obj_res lean_luau_State_lightUserdataTag(lean_luau_State state, uint32_t idx, lean_obj_arg io_) {
    lean_luau_State_data* data = lean_luau_State_fromRepr(state);
    lean_luau_guard_valid(data);
    return lean_io_result_mk_ok(lean_box_uint32((int32_t)
        lua_lightuserdatatag(data->state, (int32_t)idx)
    ));
}

LEAN_EXPORT lean_obj_res lean_luau_State_toThread(lean_luau_State state, uint32_t idx, lean_obj_arg io_) {
    lean_luau_State_data* data = lean_luau_State_fromRepr(state);
    lean_luau_guard_valid(data);
    lua_State* thread = lua_tothread(data->state, (int32_t)idx);
    if (thread == NULL) {
        return lean_io_result_mk_ok(lean_mk_option_none());
    }
    lean_luau_State_data* newData = lean_pod_alloc(sizeof(lean_luau_State_data));
    newData->state = thread;
    newData->isInterpreter = false;
    return lean_io_result_mk_ok(lean_mk_option_some(lean_luau_State_box(newData)));
}

LEAN_EXPORT lean_obj_res lean_luau_State_toBuffer(lean_luau_State state, uint32_t idx, lean_obj_arg io_) {
    lean_luau_State_data* data = lean_luau_State_fromRepr(state);
    lean_luau_guard_valid(data);
    size_t len;
    void* bytes = lua_tobuffer(data->state, (int32_t)idx, &len);
    if (bytes == NULL) {
        return lean_io_result_mk_ok(lean_mk_option_none());
    }
    lean_object* ba = lean_alloc_sarray(1, len, len);
    memcpy(lean_sarray_cptr(ba), bytes, len);
    return lean_io_result_mk_ok(lean_mk_option_some(ba));
}


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
