#include <lean/lean.h>
#include <lean_pod.h>
#include <lua.h>
#include <lualib.h>
#include <luau.lean.h>

typedef struct {
    lean_object* obj;
    lean_luau_State_data* main;
    int tag;
} lean_luau_userdata_tagged;

typedef struct {
    lean_object* obj;
    lean_object* dtor; // May be NULL
} lean_luau_userdata;

static lean_object* lean_luau_State_box(lua_State* state, lean_luau_State_data* main) {
    lean_luau_State_data* data = lean_pod_alloc(sizeof(lean_luau_State_data));
    data->state = state;
    if (main == NULL) {
        data->main = data;
        data->taggedUserdataDtors = malloc(LUA_UTAG_LIMIT * sizeof(lean_object*));
        for (size_t i = 0; i < LUA_UTAG_LIMIT; ++i) {
            data->taggedUserdataDtors[i] = NULL;
        }
    }
    else {
        data->main = main;
        data->taggedUserdataDtors = NULL;
    }
    data->referenced = NULL;
    data->referencedCount = 0;
    data->referencedCapacity = 0;
    data->interruptCallback = NULL;
    data->panicCallback = NULL;
    return lean_alloc_external(lean_luau_State_class, (void*)data);
}

static inline void lean_luau_State_reference(lean_luau_State_data* data, lean_obj_arg obj) {
    lean_luau_State_data* main = data->main;
    if (main->referencedCount >= main->referencedCapacity) {
        size_t newCapacity = main->referencedCapacity == 0 ? 4 : (main->referencedCapacity * 2);
        main->referenced = realloc(main->referenced, newCapacity);
        main->referencedCapacity = newCapacity;
    }
    main->referenced[main->referencedCount++] = obj;
}

static lean_object* lean_luau_ioerr(const char* errMsg) {
    return lean_io_result_mk_error(lean_mk_io_user_error(lean_mk_string(errMsg)));
}

#define lean_luau_guard_valid(data)\
    if ((data)->state == NULL || (data)->main->main == NULL) {\
        return lean_luau_ioerr("State is invalid (was closed).");\
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
    lua_State* state = lua_newstate(lean_luau_alloc, NULL);
    lean_object* state_obj = lean_luau_State_box(state, NULL);
    lua_callbacks(state)->userdata = state_obj;
    return lean_io_result_mk_ok(state_obj);
}

LEAN_EXPORT lean_obj_res lean_luau_State_close(b_lean_obj_arg state, lean_obj_arg io_) {
    lean_luau_State_data* data = lean_luau_State_fromRepr(state);
    lean_luau_guard_valid(data);
    if (data != data->main) {
        return lean_luau_ioerr("Expected main state.");
    }
    lua_close(data->state);
    data->state = NULL;
    data->main = NULL;
    for (size_t i = 0; i < data->referencedCount; ++i) {
        lean_dec(data->referenced[i]);
    }
    free(data->referenced);
    data->referenced = NULL;
    data->referencedCount = 0;
    data->referencedCapacity = 0;
    for (size_t i = 0; i < LUA_UTAG_LIMIT; ++i) {
        if (data->taggedUserdataDtors[i] != NULL) {
            lean_dec_ref(data->taggedUserdataDtors[i]);
        }
    }
    free(data->taggedUserdataDtors);
    return lean_io_result_mk_ok(lean_box(0));
}

LEAN_EXPORT lean_obj_res lean_luau_State_newThread(lean_luau_State state, lean_obj_arg io_) {
    lean_luau_State_data* data = lean_luau_State_fromRepr(state);
    lean_luau_guard_valid(data);
    return lean_io_result_mk_ok(lean_luau_State_box(lua_newthread(data->state), data));
}

LEAN_EXPORT lean_obj_res lean_luau_State_mainThread(lean_luau_State state, lean_obj_arg io_) {
    lean_luau_State_data* data = lean_luau_State_fromRepr(state);
    lean_luau_guard_valid(data);
    return lean_io_result_mk_ok(lean_luau_State_box(lua_mainthread(data->state), data));
}

LEAN_EXPORT lean_obj_res lean_luau_State_resetThread(lean_luau_State state, lean_obj_arg io_) {
    lean_luau_State_data* data = lean_luau_State_fromRepr(state);
    lean_luau_guard_valid(data);
    if (data == data->main) {
        return lean_luau_ioerr("Expected non-main state.");
    }
    lua_resetthread(data->state);
    data->state = NULL;
    data->main = NULL;
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
    return lean_io_result_mk_ok(lean_box(
        (lua_isuserdata(data->state, (int32_t)idx) != 0) &&
        !(lua_islightuserdata(data->state, (int32_t)idx) != 0) &&
        lua_userdatatag(data->state, (int32_t)idx) == (LUA_UTAG_LIMIT + 1)
    ));
}

LEAN_EXPORT lean_obj_res lean_luau_State_isFunction(lean_luau_State state, uint32_t idx, lean_obj_arg io_) {
    lean_luau_State_data* data = lean_luau_State_fromRepr(state);
    lean_luau_guard_valid(data);
    return lean_io_result_mk_ok(lean_box(lua_isfunction(data->state, (int32_t)idx) != 0));
}

LEAN_EXPORT lean_obj_res lean_luau_State_isTable(lean_luau_State state, uint32_t idx, lean_obj_arg io_) {
    lean_luau_State_data* data = lean_luau_State_fromRepr(state);
    lean_luau_guard_valid(data);
    return lean_io_result_mk_ok(lean_box(lua_istable(data->state, (int32_t)idx) != 0));
}

LEAN_EXPORT lean_obj_res lean_luau_State_isLightUserdata(lean_luau_State state, uint32_t idx, lean_obj_arg io_) {
    lean_luau_State_data* data = lean_luau_State_fromRepr(state);
    lean_luau_guard_valid(data);
    return lean_io_result_mk_ok(lean_box(lua_islightuserdata(data->state, (int32_t)idx) != 0));
}

LEAN_EXPORT lean_obj_res lean_luau_State_isNil(lean_luau_State state, uint32_t idx, lean_obj_arg io_) {
    lean_luau_State_data* data = lean_luau_State_fromRepr(state);
    lean_luau_guard_valid(data);
    return lean_io_result_mk_ok(lean_box(lua_isnil(data->state, (int32_t)idx) != 0));
}

LEAN_EXPORT lean_obj_res lean_luau_State_isBoolean(lean_luau_State state, uint32_t idx, lean_obj_arg io_) {
    lean_luau_State_data* data = lean_luau_State_fromRepr(state);
    lean_luau_guard_valid(data);
    return lean_io_result_mk_ok(lean_box(lua_isboolean(data->state, (int32_t)idx) != 0));
}

LEAN_EXPORT lean_obj_res lean_luau_State_isVector(lean_luau_State state, uint32_t idx, lean_obj_arg io_) {
    lean_luau_State_data* data = lean_luau_State_fromRepr(state);
    lean_luau_guard_valid(data);
    return lean_io_result_mk_ok(lean_box(lua_isvector(data->state, (int32_t)idx) != 0));
}

LEAN_EXPORT lean_obj_res lean_luau_State_isThread(lean_luau_State state, uint32_t idx, lean_obj_arg io_) {
    lean_luau_State_data* data = lean_luau_State_fromRepr(state);
    lean_luau_guard_valid(data);
    return lean_io_result_mk_ok(lean_box(lua_isthread(data->state, (int32_t)idx) != 0));
}

LEAN_EXPORT lean_obj_res lean_luau_State_isBuffer(lean_luau_State state, uint32_t idx, lean_obj_arg io_) {
    lean_luau_State_data* data = lean_luau_State_fromRepr(state);
    lean_luau_guard_valid(data);
    return lean_io_result_mk_ok(lean_box(lua_isbuffer(data->state, (int32_t)idx) != 0));
}

LEAN_EXPORT lean_obj_res lean_luau_State_isNone(lean_luau_State state, uint32_t idx, lean_obj_arg io_) {
    lean_luau_State_data* data = lean_luau_State_fromRepr(state);
    lean_luau_guard_valid(data);
    return lean_io_result_mk_ok(lean_box(lua_isnone(data->state, (int32_t)idx) != 0));
}

LEAN_EXPORT lean_obj_res lean_luau_State_isNoneOrNil(lean_luau_State state, uint32_t idx, lean_obj_arg io_) {
    lean_luau_State_data* data = lean_luau_State_fromRepr(state);
    lean_luau_guard_valid(data);
    return lean_io_result_mk_ok(lean_box(lua_isnoneornil(data->state, (int32_t)idx) != 0));
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

// TODO: lean_luau_State_toLightUserdata (Untagged light userdata = light userdata with tag=0)

LEAN_EXPORT lean_obj_res lean_luau_State_toLightUserdataTagged(lean_luau_State state, uint32_t idx, uint32_t tag, lean_obj_arg io_) {
    lean_luau_State_data* data = lean_luau_State_fromRepr(state);
    lean_luau_guard_valid(data);
    lean_object* val = lua_tolightuserdatatagged(data->state, (int32_t)idx, tag);
    if (val == NULL) {
        return lean_io_result_mk_ok(lean_mk_option_none());
    }
    lean_inc(val);
    return lean_io_result_mk_ok(lean_mk_option_some(val));
}

LEAN_EXPORT lean_obj_res lean_luau_State_toUserdata(lean_luau_State state, uint32_t idx, lean_obj_arg io_) {
    lean_luau_State_data* data = lean_luau_State_fromRepr(state);
    lean_luau_guard_valid(data);
    lean_luau_userdata* userdata = lua_touserdatatagged(data->state, (int32_t)idx, LUA_UTAG_LIMIT);
    if (userdata == NULL) {
        return lean_io_result_mk_ok(lean_mk_option_none());
    }
    lean_object* val = userdata->obj;
    lean_inc(val);
    return lean_io_result_mk_ok(lean_mk_option_some(val));
}

LEAN_EXPORT lean_obj_res lean_luau_State_toUserdataTagged(lean_luau_State state, uint32_t idx, uint32_t tag, lean_obj_arg io_) {
    lean_luau_State_data* data = lean_luau_State_fromRepr(state);
    lean_luau_guard_valid(data);
    lean_luau_userdata_tagged* userdata = lua_touserdatatagged(data->state, (int32_t)idx, tag);
    if (userdata == NULL) {
        return lean_io_result_mk_ok(lean_mk_option_none());
    }
    lean_object* val = userdata->obj;
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
    return lean_io_result_mk_ok(lean_mk_option_some(lean_luau_State_box(thread, data)));
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

LEAN_EXPORT lean_obj_res lean_luau_State_pushNil(lean_luau_State state, lean_obj_arg io_) {
    lean_luau_State_data* data = lean_luau_State_fromRepr(state);
    lean_luau_guard_valid(data);
    lua_pushnil(data->state);
    return lean_io_result_mk_ok(lean_box(0));
}

LEAN_EXPORT lean_obj_res lean_luau_State_pushNumber(lean_luau_State state, double n, lean_obj_arg io_) {
    lean_luau_State_data* data = lean_luau_State_fromRepr(state);
    lean_luau_guard_valid(data);
    lua_pushnumber(data->state, n);
    return lean_io_result_mk_ok(lean_box(0));
}

LEAN_EXPORT lean_obj_res lean_luau_State_pushInteger(lean_luau_State state, uint32_t n, lean_obj_arg io_) {
    lean_luau_State_data* data = lean_luau_State_fromRepr(state);
    lean_luau_guard_valid(data);
    lua_pushinteger(data->state, (int32_t)n);
    return lean_io_result_mk_ok(lean_box(0));
}

LEAN_EXPORT lean_obj_res lean_luau_State_pushUnsigned(lean_luau_State state, uint32_t n, lean_obj_arg io_) {
    lean_luau_State_data* data = lean_luau_State_fromRepr(state);
    lean_luau_guard_valid(data);
    lua_pushunsigned(data->state, n);
    return lean_io_result_mk_ok(lean_box(0));
}

LEAN_EXPORT lean_obj_res lean_luau_State_pushString(lean_luau_State state, b_lean_obj_arg s, lean_obj_arg io_) {
    lean_luau_State_data* data = lean_luau_State_fromRepr(state);
    lean_luau_guard_valid(data);
    lua_pushlstring(data->state, lean_string_cstr(s), lean_string_byte_size(s));
    return lean_io_result_mk_ok(lean_box(0));
}

LEAN_EXPORT lean_obj_res lean_luau_State_pushBoolean(lean_luau_State state, uint8_t b, lean_obj_arg io_) {
    lean_luau_State_data* data = lean_luau_State_fromRepr(state);
    lean_luau_guard_valid(data);
    lua_pushboolean(data->state, b != 0);
    return lean_io_result_mk_ok(lean_box(0));
}

LEAN_EXPORT lean_obj_res lean_luau_State_pushThread(lean_luau_State state, lean_obj_arg io_) {
    lean_luau_State_data* data = lean_luau_State_fromRepr(state);
    lean_luau_guard_valid(data);
    return lean_io_result_mk_ok(lean_box(lua_pushthread(data->state) != 0));
}

LEAN_EXPORT lean_obj_res lean_luau_State_pushLightUserdataTagged(lean_luau_State state, lean_obj_arg p, uint32_t tag, lean_obj_arg io_) {
    lean_luau_State_data* data = lean_luau_State_fromRepr(state);
    lean_luau_guard_valid(data);
    lean_luau_State_reference(data, p);
    lua_pushlightuserdatatagged(data->state, (void*)p, tag);
    return lean_io_result_mk_ok(lean_box(0));
}

// TODO: lean_luau_State_pushLightUserdata (Untagged light userdata = light userdata with tag=0)

static void lean_luau_userdata_tagged_dtor(lua_State* state, void* userdata) {
    lean_luau_userdata_tagged* userdata_ = userdata;
    lean_object* dtor = userdata_->main->taggedUserdataDtors[userdata_->tag];
    if (dtor != NULL) {
        lean_inc_ref(dtor);
        lean_object* res = lean_apply_3(dtor, lean_luau_State_box(state, userdata_->main), userdata_->obj, lean_box(0));
        lean_dec_ref(res);
        return;
    }
    lean_dec(userdata_->obj);
}

static void lean_luau_userdata_dtor(void* userdata) {
    lean_luau_userdata* userdata_ = userdata;
    if (userdata_->dtor != NULL) {
        lean_object* res = lean_apply_2(userdata_->dtor, userdata_->obj, lean_box(0));
        lean_dec_ref(res);
        return;
    }
    lean_dec(userdata_->obj);
}

LEAN_EXPORT lean_obj_res lean_luau_State_newUserdataTagged(lean_luau_State state, lean_obj_arg userdata, uint32_t tag, lean_obj_arg io_) {
    lean_luau_State_data* data = lean_luau_State_fromRepr(state);
    lean_luau_guard_valid(data);
    lean_luau_userdata_tagged* dst = lua_newuserdatatagged(data->state, sizeof(lean_luau_userdata_tagged), tag);
    lua_setuserdatadtor(data->state, tag, lean_luau_userdata_tagged_dtor);
    dst->obj = userdata;
    dst->tag = tag;
    dst->main = data->main;
    return lean_io_result_mk_ok(lean_box(0));
}

LEAN_EXPORT lean_obj_res lean_luau_State_newUserdata(lean_luau_State state, lean_obj_arg userdata, lean_obj_arg io_) {
    lean_luau_State_data* data = lean_luau_State_fromRepr(state);
    lean_luau_guard_valid(data);
    lean_luau_userdata* dst = lua_newuserdatadtor(data->state, sizeof(lean_luau_userdata), lean_luau_userdata_dtor);
    dst->obj = userdata;
    dst->dtor = NULL;
    return lean_io_result_mk_ok(lean_box(0));
}

LEAN_EXPORT lean_obj_res lean_luau_State_newUserdataDtor(lean_luau_State state, lean_obj_arg userdata, lean_obj_arg dtor, lean_obj_arg io_) {
    lean_luau_State_data* data = lean_luau_State_fromRepr(state);
    lean_luau_guard_valid(data);
    lean_luau_userdata* dst = lua_newuserdatadtor(data->state, sizeof(lean_luau_userdata), lean_luau_userdata_dtor);
    dst->obj = userdata;
    dst->dtor = dtor;
    return lean_io_result_mk_ok(lean_box(0));
}

LEAN_EXPORT lean_obj_res lean_luau_State_newBuffer(lean_luau_State state, b_lean_obj_arg sz, lean_pod_BytesView src, lean_obj_arg io_) {
    lean_luau_State_data* data = lean_luau_State_fromRepr(state);
    lean_luau_guard_valid(data);
    size_t size = lean_usize_of_nat(sz);
    void* dst = lua_newbuffer(data->state, size);
    memcpy(dst, lean_pod_BytesView_fromRepr(src)->ptr, size);
    return lean_io_result_mk_ok(lean_box(0));
}

typedef struct {
    lean_object* fn;
    lean_object* cont; // May be NULL
    lean_luau_State_data* main;
} lean_luau_CFunction_data;

static void lean_luau_CFunction_data_dtor(void* userdata) {
    lean_luau_CFunction_data* userdata_ = userdata;
    lean_dec_ref(userdata_->fn);
    if (userdata_->cont != NULL) {
        lean_dec_ref(userdata_->cont);
    }
    free(userdata_);
}

lean_object* lean_io_error_to_string(lean_object* err);

static int lean_luau_CFunction_c(lua_State* state) {
    lean_luau_CFunction_data* ud = lua_touserdata(state, lua_upvalueindex(1));
    lean_inc_ref(ud->fn);
    lean_obj_res res = lean_apply_2(ud->fn, lean_luau_State_box(state, ud->main), lean_box(0));
    if (lean_io_result_is_error(res)) {
        lean_object* err = lean_io_result_get_error(res);
        lean_inc(err);
        lean_object* errs = lean_io_error_to_string(err);
        lua_pushstring(state, lean_string_cstr(errs));
        lean_dec_ref(errs);
        lean_dec_ref(res);
        lua_error(state);
    }
    int nresults = (int32_t)lean_unbox_uint32(lean_io_result_get_value(res));
    lean_dec_ref(res);
    return nresults;
}

static int lean_luau_Continuation_c(lua_State* state, int status) {
    lean_luau_CFunction_data* ud = lua_touserdata(state, lua_upvalueindex(1));
    lean_inc_ref(ud->cont);
    lean_obj_res res = lean_apply_3(ud->cont, lean_luau_State_box(state, ud->main), lean_box(status), lean_box(0));
    if (lean_io_result_is_error(res)) {
        lean_object* err = lean_io_result_get_error(res);
        lean_inc(err);
        lean_object* errs = lean_io_error_to_string(err);
        lua_pushstring(state, lean_string_cstr(errs));
        lean_dec_ref(errs);
        lean_dec_ref(res);
        lua_error(state);
    }
    int nresults = (int32_t)lean_unbox_uint32(lean_io_result_get_value(res));
    lean_dec_ref(res);
    return nresults;
}

LEAN_EXPORT lean_obj_res lean_luau_State_pushCClosureK(lean_luau_State state, lean_obj_arg fn, b_lean_obj_arg debugName, uint32_t nup, lean_obj_arg cont, lean_obj_arg io_) {
    lean_luau_State_data* data = lean_luau_State_fromRepr(state);
    lean_luau_guard_valid(data);
    lean_luau_CFunction_data* ud = lua_newuserdatadtor(data->state, sizeof(lean_luau_CFunction_data), lean_luau_CFunction_data_dtor);
    ud->fn = fn;
    ud->cont = cont;
    ud->main = data->main;
    lua_insert(data->state, lua_gettop(data->state) - nup);
    const char* debugName_c = lean_option_is_some(debugName) ? lean_string_cstr(lean_ctor_get(debugName, 0)) : NULL;
    lua_pushcclosurek(data->state, lean_luau_CFunction_c, debugName_c, nup + 1, lean_luau_Continuation_c);
    return lean_io_result_mk_ok(lean_box(0));
}


static void lean_luau_State_pushCClosure_impl(lua_State* state, lean_luau_State_data* main, lean_obj_arg fn, const char* debugName, uint32_t nup) {
    lean_luau_CFunction_data* ud = lua_newuserdatadtor(state, sizeof(lean_luau_CFunction_data), lean_luau_CFunction_data_dtor);
    ud->fn = fn;
    ud->cont = NULL;
    ud->main = main;
    lua_insert(state, lua_gettop(state) - nup);
    lua_pushcclosure(state, lean_luau_CFunction_c, debugName, nup + 1);
}

LEAN_EXPORT lean_obj_res lean_luau_State_pushCClosure(lean_luau_State state, lean_obj_arg fn, b_lean_obj_arg debugName, uint32_t nup, lean_obj_arg io_) {
    lean_luau_State_data* data = lean_luau_State_fromRepr(state);
    lean_luau_guard_valid(data);
    const char* debugName_c = lean_option_is_some(debugName) ? lean_string_cstr(lean_ctor_get(debugName, 0)) : NULL;
    lean_luau_State_pushCClosure_impl(data->state, data->main, fn, debugName_c, nup);
    return lean_io_result_mk_ok(lean_box(0));
}


// Get functions

LEAN_EXPORT lean_obj_res lean_luau_State_getTable(lean_luau_State state, uint32_t idx, lean_obj_arg io_) {
    lean_luau_State_data* data = lean_luau_State_fromRepr(state);
    lean_luau_guard_valid(data);
    return lean_io_result_mk_ok(lean_box(lua_gettable(data->state, (int32_t)idx)));
}

LEAN_EXPORT lean_obj_res lean_luau_State_getField(lean_luau_State state, uint32_t idx, b_lean_obj_arg k, lean_obj_arg io_) {
    lean_luau_State_data* data = lean_luau_State_fromRepr(state);
    lean_luau_guard_valid(data);
    return lean_io_result_mk_ok(lean_box(lua_getfield(data->state, (int32_t)idx, lean_string_cstr(k))));
}

LEAN_EXPORT lean_obj_res lean_luau_State_rawGetField(lean_luau_State state, uint32_t idx, b_lean_obj_arg k, lean_obj_arg io_) {
    lean_luau_State_data* data = lean_luau_State_fromRepr(state);
    lean_luau_guard_valid(data);
    return lean_io_result_mk_ok(lean_box(lua_rawgetfield(data->state, (int32_t)idx, lean_string_cstr(k))));
}

LEAN_EXPORT lean_obj_res lean_luau_State_rawGet(lean_luau_State state, uint32_t idx, lean_obj_arg io_) {
    lean_luau_State_data* data = lean_luau_State_fromRepr(state);
    lean_luau_guard_valid(data);
    return lean_io_result_mk_ok(lean_box(lua_rawget(data->state, (int32_t)idx)));
}

LEAN_EXPORT lean_obj_res lean_luau_State_rawGetI(lean_luau_State state, uint32_t idx, uint32_t n, lean_obj_arg io_) {
    lean_luau_State_data* data = lean_luau_State_fromRepr(state);
    lean_luau_guard_valid(data);
    return lean_io_result_mk_ok(lean_box(lua_rawgeti(data->state, (int32_t)idx, (int32_t)n)));
}

LEAN_EXPORT lean_obj_res lean_luau_State_createTable(lean_luau_State state, uint32_t narr, uint32_t nrec, lean_obj_arg io_) {
    lean_luau_State_data* data = lean_luau_State_fromRepr(state);
    lean_luau_guard_valid(data);
    lua_createtable(data->state, (int32_t)narr, (int32_t)nrec);
    return lean_io_result_mk_ok(lean_box(0));
}

LEAN_EXPORT lean_obj_res lean_luau_State_setReadonly(lean_luau_State state, uint32_t idx, uint8_t enabled, lean_obj_arg io_) {
    lean_luau_State_data* data = lean_luau_State_fromRepr(state);
    lean_luau_guard_valid(data);
    lua_setreadonly(data->state, (int32_t)idx, enabled);
    return lean_io_result_mk_ok(lean_box(0));
}

LEAN_EXPORT lean_obj_res lean_luau_State_getReadonly(lean_luau_State state, uint32_t idx, lean_obj_arg io_) {
    lean_luau_State_data* data = lean_luau_State_fromRepr(state);
    lean_luau_guard_valid(data);
    return lean_io_result_mk_ok(lean_box(lua_getreadonly(data->state, (int32_t)idx) != 0));
}

LEAN_EXPORT lean_obj_res lean_luau_State_setSafeEnv(lean_luau_State state, uint32_t idx, uint8_t enabled, lean_obj_arg io_) {
    lean_luau_State_data* data = lean_luau_State_fromRepr(state);
    lean_luau_guard_valid(data);
    lua_setsafeenv(data->state, (int32_t)idx, enabled);
    return lean_io_result_mk_ok(lean_box(0));
}

LEAN_EXPORT lean_obj_res lean_luau_State_getMetatable(lean_luau_State state, uint32_t objindex, lean_obj_arg io_) {
    lean_luau_State_data* data = lean_luau_State_fromRepr(state);
    lean_luau_guard_valid(data);
    return lean_io_result_mk_ok(lean_box(lua_getmetatable(data->state, (int32_t)objindex) != 0));
}

LEAN_EXPORT lean_obj_res lean_luau_State_getFEnv(lean_luau_State state, uint32_t idx, lean_obj_arg io_) {
    lean_luau_State_data* data = lean_luau_State_fromRepr(state);
    lean_luau_guard_valid(data);
    lua_getfenv(data->state, (int32_t)idx);
    return lean_io_result_mk_ok(lean_box(0));
}


// Set functions

LEAN_EXPORT lean_obj_res lean_luau_State_setTable(lean_luau_State state, uint32_t idx, lean_obj_arg io_) {
    lean_luau_State_data* data = lean_luau_State_fromRepr(state);
    lean_luau_guard_valid(data);
    lua_settable(data->state, (int32_t)idx);
    return lean_io_result_mk_ok(lean_box(0));
}

LEAN_EXPORT lean_obj_res lean_luau_State_setField(lean_luau_State state, uint32_t idx, b_lean_obj_arg k, lean_obj_arg io_) {
    lean_luau_State_data* data = lean_luau_State_fromRepr(state);
    lean_luau_guard_valid(data);
    lua_setfield(data->state, (int32_t)idx, lean_string_cstr(k));
    return lean_io_result_mk_ok(lean_box(0));
}

LEAN_EXPORT lean_obj_res lean_luau_State_rawSetField(lean_luau_State state, uint32_t idx, b_lean_obj_arg k, lean_obj_arg io_) {
    lean_luau_State_data* data = lean_luau_State_fromRepr(state);
    lean_luau_guard_valid(data);
    lua_rawsetfield(data->state, (int32_t)idx, lean_string_cstr(k));
    return lean_io_result_mk_ok(lean_box(0));
}

LEAN_EXPORT lean_obj_res lean_luau_State_rawSet(lean_luau_State state, uint32_t idx, lean_obj_arg io_) {
    lean_luau_State_data* data = lean_luau_State_fromRepr(state);
    lean_luau_guard_valid(data);
    lua_rawset(data->state, (int32_t)idx);
    return lean_io_result_mk_ok(lean_box(0));
}

LEAN_EXPORT lean_obj_res lean_luau_State_rawSetI(lean_luau_State state, uint32_t idx, uint32_t n, lean_obj_arg io_) {
    lean_luau_State_data* data = lean_luau_State_fromRepr(state);
    lean_luau_guard_valid(data);
    lua_rawseti(data->state, (int32_t)idx, (int32_t)n);
    return lean_io_result_mk_ok(lean_box(0));
}

LEAN_EXPORT lean_obj_res lean_luau_State_setMetatable(lean_luau_State state, uint32_t objindex, lean_obj_arg io_) {
    lean_luau_State_data* data = lean_luau_State_fromRepr(state);
    lean_luau_guard_valid(data);
    lua_setmetatable(data->state, (int32_t)objindex);
    return lean_io_result_mk_ok(lean_box(0));
}

LEAN_EXPORT lean_obj_res lean_luau_State_setFEnv(lean_luau_State state, uint32_t idx, lean_obj_arg io_) {
    lean_luau_State_data* data = lean_luau_State_fromRepr(state);
    lean_luau_guard_valid(data);
    return lean_io_result_mk_ok(lean_box(lua_setfenv(data->state, (int32_t)idx) != 0));
}


// Load and call functions

LEAN_EXPORT lean_obj_res lean_luau_State_load(
    lean_luau_State state, b_lean_obj_arg chunkName, b_lean_obj_arg size,
    lean_pod_BytesView data, uint32_t env, lean_obj_arg io_
) {
    lean_luau_State_data* sdata = lean_luau_State_fromRepr(state);
    lean_luau_guard_valid(sdata);
    return lean_io_result_mk_ok(lean_box(0 != luau_load(
        sdata->state,
        lean_string_cstr(chunkName),
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


// Coroutine functions

LEAN_EXPORT lean_obj_res lean_luau_State_yield(lean_luau_State state, uint32_t nArgs, lean_obj_arg io_) {
    lean_luau_State_data* data = lean_luau_State_fromRepr(state);
    lean_luau_guard_valid(data);
    return lean_io_result_mk_ok(lean_box_uint32((int32_t)
        lua_yield(data->state, (int32_t)nArgs)
    ));
}

LEAN_EXPORT lean_obj_res lean_luau_State_break(lean_luau_State state, lean_obj_arg io_) {
    lean_luau_State_data* data = lean_luau_State_fromRepr(state);
    lean_luau_guard_valid(data);
    return lean_io_result_mk_ok(lean_box_uint32((int32_t)lua_break(data->state)));
}

LEAN_EXPORT lean_obj_res lean_luau_State_resume(lean_luau_State state, lean_luau_State from, uint32_t narg, lean_obj_arg io_) {
    lean_luau_State_data* data = lean_luau_State_fromRepr(state);
    lean_luau_guard_valid(data);
    lua_State* from_c = NULL;
    if (lean_option_is_some(from)) {
        lean_luau_State_data* from_data = lean_luau_State_unbox(lean_ctor_get(from, 0));
        lean_luau_guard_valid(from_data);
        from_c = from_data->state;
    }
    return lean_io_result_mk_ok(lean_box(
        lua_resume(data->state, from_c, (int32_t)narg)
    ));
}

LEAN_EXPORT lean_obj_res lean_luau_State_resumeError(lean_luau_State state, lean_luau_State from, lean_obj_arg io_) {
    lean_luau_State_data* data = lean_luau_State_fromRepr(state);
    lean_luau_guard_valid(data);
    lua_State* from_c = NULL;
    if (lean_option_is_some(from)) {
        lean_luau_State_data* from_data = lean_luau_State_unbox(lean_ctor_get(from, 0));
        lean_luau_guard_valid(from_data);
        from_c = from_data->state;
    }
    return lean_io_result_mk_ok(lean_box(lua_resumeerror(data->state, from_c)));
}

LEAN_EXPORT lean_obj_res lean_luau_State_status(lean_luau_State state, lean_obj_arg io_) {
    lean_luau_State_data* data = lean_luau_State_fromRepr(state);
    lean_luau_guard_valid(data);
    return lean_io_result_mk_ok(lean_box(lua_status(data->state)));
}

LEAN_EXPORT lean_obj_res lean_luau_State_isYieldable(lean_luau_State state, lean_obj_arg io_) {
    lean_luau_State_data* data = lean_luau_State_fromRepr(state);
    lean_luau_guard_valid(data);
    return lean_io_result_mk_ok(lean_box(lua_isyieldable(data->state) != 0));
}

LEAN_EXPORT lean_obj_res lean_luau_State_coStatus(lean_luau_State state, lean_luau_State co, lean_obj_arg io_) {
    lean_luau_State_data* data1 = lean_luau_State_fromRepr(state);
    lean_luau_State_data* data2 = lean_luau_State_fromRepr(co);
    lean_luau_guard_valid(data1);
    lean_luau_guard_valid(data2);
    return lean_io_result_mk_ok(lean_box(lua_costatus(data1->state, data2->state)));
}


// GC functions and options

LEAN_EXPORT lean_obj_res lean_luau_State_gc(lean_luau_State state, uint8_t what, uint32_t arg, lean_obj_arg io_) {
    lean_luau_State_data* data = lean_luau_State_fromRepr(state);
    lean_luau_guard_valid(data);
    return lean_io_result_mk_ok(lean_box_uint32((int32_t)lua_gc(data->state, what, (int32_t)arg)));
}


// Memory statistics

LEAN_EXPORT lean_obj_res lean_luau_State_setMemCat(lean_luau_State state, uint32_t category, lean_obj_arg io_) {
    lean_luau_State_data* data = lean_luau_State_fromRepr(state);
    lean_luau_guard_valid(data);
    lua_setmemcat(data->state, category);
    return lean_io_result_mk_ok(lean_box(0));
}

LEAN_EXPORT lean_obj_res lean_luau_State_totalBytes(lean_luau_State state, uint32_t category, lean_obj_arg io_) {
    lean_luau_State_data* data = lean_luau_State_fromRepr(state);
    lean_luau_guard_valid(data);
    return lean_io_result_mk_ok(lean_box_usize(lua_totalbytes(data->state, category)));
}


// Miscellaneous functions

LEAN_EXPORT lean_obj_res lean_luau_State_error(lean_luau_State state, lean_obj_arg io_) {
    lean_luau_State_data* data = lean_luau_State_fromRepr(state);
    lean_luau_guard_valid(data);
    lua_error(data->state);
}

LEAN_EXPORT lean_obj_res lean_luau_State_next(lean_luau_State state, uint32_t idx, lean_obj_arg io_) {
    lean_luau_State_data* data = lean_luau_State_fromRepr(state);
    lean_luau_guard_valid(data);
    return lean_io_result_mk_ok(lean_box(lua_next(data->state, (int32_t)idx) != 0));
}

LEAN_EXPORT lean_obj_res lean_luau_State_rawIter(lean_luau_State state, uint32_t idx, uint32_t iter, lean_obj_arg io_) {
    lean_luau_State_data* data = lean_luau_State_fromRepr(state);
    lean_luau_guard_valid(data);
    return lean_io_result_mk_ok(lean_box_uint32((int32_t)
        lua_rawiter(data->state, (int32_t)idx, (int32_t)iter)
    ));
}

LEAN_EXPORT lean_obj_res lean_luau_State_concat(lean_luau_State state, uint32_t n, lean_obj_arg io_) {
    lean_luau_State_data* data = lean_luau_State_fromRepr(state);
    lean_luau_guard_valid(data);
    lua_concat(data->state, (int32_t)n);
    return lean_io_result_mk_ok(lean_box(0));
}

LEAN_EXPORT lean_obj_res lean_luau_State_clock(lean_luau_State state, lean_obj_arg io_) {
    lean_luau_State_data* data = lean_luau_State_fromRepr(state);
    lean_luau_guard_valid(data);
    return lean_io_result_mk_ok(lean_box_float(lua_clock(data->state)));
}

LEAN_EXPORT lean_obj_res lean_luau_State_setUserdataDtor(lean_luau_State state, uint32_t tag, lean_obj_arg dtor, lean_obj_arg io_) {
    lean_luau_State_data* data = lean_luau_State_fromRepr(state);
    lean_luau_guard_valid(data);
    lean_object** dtors = data->main->taggedUserdataDtors;
    if (dtors[tag] != NULL) {
        lean_dec_ref(dtors[tag]);
    }
    dtors[tag] = dtor;
    return lean_io_result_mk_ok(lean_box(0));
}

LEAN_EXPORT lean_obj_res lean_luau_State_resetUserdataDtor(lean_luau_State state, uint32_t tag, lean_obj_arg io_) {
    lean_luau_State_data* data = lean_luau_State_fromRepr(state);
    lean_luau_guard_valid(data);
    lean_object** dtors = data->main->taggedUserdataDtors;
    if (dtors[tag] != NULL) {
        lean_dec_ref(dtors[tag]);
    }
    dtors[tag] = NULL;
    return lean_io_result_mk_ok(lean_box(0));
}

LEAN_EXPORT lean_obj_res lean_luau_State_setUserdataMetatable(lean_luau_State state, uint32_t tag, uint32_t idx, lean_obj_arg io_) {
    lean_luau_State_data* data = lean_luau_State_fromRepr(state);
    lean_luau_guard_valid(data);
    lua_setuserdatametatable(data->state, tag, (int32_t)idx);
    return lean_io_result_mk_ok(lean_box(0));
}

LEAN_EXPORT lean_obj_res lean_luau_State_getUserdataMetatable(lean_luau_State state, uint32_t tag, lean_obj_arg io_) {
    lean_luau_State_data* data = lean_luau_State_fromRepr(state);
    lean_luau_guard_valid(data);
    lua_getuserdatametatable(data->state, tag);
    return lean_io_result_mk_ok(lean_box(0));
}

LEAN_EXPORT lean_obj_res lean_luau_State_setLightUserdataName(lean_luau_State state, uint32_t tag, b_lean_obj_arg name, lean_obj_arg io_) {
    lean_luau_State_data* data = lean_luau_State_fromRepr(state);
    lean_luau_guard_valid(data);
    lua_setlightuserdataname(data->state, tag, lean_string_cstr(name));
    return lean_io_result_mk_ok(lean_box(0));
}

LEAN_EXPORT lean_obj_res lean_luau_State_getLightUserdataName(lean_luau_State state, uint32_t tag, lean_obj_arg io_) {
    lean_luau_State_data* data = lean_luau_State_fromRepr(state);
    lean_luau_guard_valid(data);
    return lean_io_result_mk_ok(lean_mk_string(lua_getlightuserdataname(data->state, tag)));
}

LEAN_EXPORT lean_obj_res lean_luau_State_cloneFunction(lean_luau_State state, uint32_t idx, lean_obj_arg io_) {
    lean_luau_State_data* data = lean_luau_State_fromRepr(state);
    lean_luau_guard_valid(data);
    lua_clonefunction(data->state, (int32_t)idx);
    return lean_io_result_mk_ok(lean_box(0));
}

LEAN_EXPORT lean_obj_res lean_luau_State_clearTable(lean_luau_State state, uint32_t idx, lean_obj_arg io_) {
    lean_luau_State_data* data = lean_luau_State_fromRepr(state);
    lean_luau_guard_valid(data);
    lua_cleartable(data->state, (int32_t)idx);
    return lean_io_result_mk_ok(lean_box(0));
}


// Reference system

LEAN_EXPORT lean_obj_res lean_luau_State_ref(lean_luau_State state, uint32_t idx, lean_obj_arg io_) {
    lean_luau_State_data* data = lean_luau_State_fromRepr(state);
    lean_luau_guard_valid(data);
    return lean_io_result_mk_ok(lean_box_uint32((int32_t)lua_ref(data->state, (int32_t)idx)));
}

LEAN_EXPORT lean_obj_res lean_luau_State_unref(lean_luau_State state, uint32_t ref, lean_obj_arg io_) {
    lean_luau_State_data* data = lean_luau_State_fromRepr(state);
    lean_luau_guard_valid(data);
    lua_unref(data->state, (int32_t)ref);
    return lean_io_result_mk_ok(lean_box(0));
}


// Callbacks

static void lean_luau_interrupt_callback(lua_State* state, int gc) {
    lean_luau_State_data* data = lean_luau_State_unbox(lua_callbacks(state)->userdata);
    if (data->main == NULL || data->main->main == NULL || data->main->interruptCallback == NULL) {
        return;
    }
    lean_inc_ref(data->main->interruptCallback);
    lean_dec_ref(lean_apply_3(
        data->main->interruptCallback,
        lean_luau_State_box(state, data->main),
        lean_box_uint32((int32_t)gc),
        lean_box(0)
    ));
}

LEAN_EXPORT lean_obj_res lean_luau_State_setInterruptCallback(lean_luau_State state, lean_obj_arg fn, lean_obj_arg io_) {
    lean_luau_State_data* data = lean_luau_State_fromRepr(state);
    lean_luau_guard_valid(data);
    if (data->interruptCallback != NULL) {
        lean_dec_ref(data->interruptCallback);
    }
    data->interruptCallback = fn;
    lua_callbacks(data->state)->interrupt = lean_luau_interrupt_callback;
    return lean_io_result_mk_ok(lean_box(0));
}

LEAN_EXPORT lean_obj_res lean_luau_State_resetInterruptCallback(lean_luau_State state, lean_obj_arg io_) {
    lean_luau_State_data* data = lean_luau_State_fromRepr(state);
    lean_luau_guard_valid(data);
    if (data->interruptCallback != NULL) {
        lean_dec_ref(data->interruptCallback);
        data->interruptCallback = NULL;
    }
    lua_callbacks(data->state)->interrupt = NULL;
    return lean_io_result_mk_ok(lean_box(0));
}

static void lean_luau_panic_callback(lua_State* state, int errcode) {
    lean_luau_State_data* data = lean_luau_State_unbox(lua_callbacks(state)->userdata);
    if (data->main == NULL || data->main->main == NULL || data->main->panicCallback == NULL) {
        return;
    }
    lean_inc_ref(data->main->panicCallback);
    lean_dec_ref(lean_apply_3(
        data->main->panicCallback,
        lean_luau_State_box(state, data->main),
        lean_box_uint32((int32_t)errcode),
        lean_box(0)
    ));
}

LEAN_EXPORT lean_obj_res lean_luau_State_setPanicCallback(lean_luau_State state, lean_obj_arg fn, lean_obj_arg io_) {
    lean_luau_State_data* data = lean_luau_State_fromRepr(state);
    lean_luau_guard_valid(data);
    if (data->panicCallback != NULL) {
        lean_dec_ref(data->panicCallback);
    }
    data->panicCallback = fn;
    lua_callbacks(data->state)->panic = lean_luau_panic_callback;
    return lean_io_result_mk_ok(lean_box(0));
}

LEAN_EXPORT lean_obj_res lean_luau_State_resetPanicCallback(lean_luau_State state, lean_obj_arg io_) {
    lean_luau_State_data* data = lean_luau_State_fromRepr(state);
    lean_luau_guard_valid(data);
    if (data->panicCallback != NULL) {
        lean_dec_ref(data->panicCallback);
        data->panicCallback = NULL;
    }
    lua_callbacks(data->state)->panic = NULL;
    return lean_io_result_mk_ok(lean_box(0));
}


// luaL_*

LEAN_EXPORT lean_obj_res lean_luau_State_register(lean_luau_State state, b_lean_obj_arg libname, b_lean_obj_arg funcs, lean_obj_arg io_) {
    lean_luau_State_data* data = lean_luau_State_fromRepr(state);
    lean_luau_guard_valid(data);
    const char* libname_c = lean_string_cstr(libname);
    size_t size = lean_array_size(funcs);
    // check whether lib already exists
    luaL_findtable(data->state, LUA_REGISTRYINDEX, "_LOADED", 1);
    lua_getfield(data->state, -1, libname_c); // get _LOADED[libname_c]
    if (!lua_istable(data->state, -1))
    {                  // not found?
        lua_pop(data->state, 1); // remove previous result
        // try global variable (and create one if it does not exist)
        if (luaL_findtable(data->state, LUA_GLOBALSINDEX, libname_c, size) != NULL)
            luaL_error(data->state, "name conflict for module '%s'", libname_c);
        lua_pushvalue(data->state, -1);
        lua_setfield(data->state, -3, libname_c); // _LOADED[libname_c] = new table
    }
    lua_remove(data->state, -2);
    for (size_t i = 0; i < size; ++i) {
        lean_object* pair = lean_array_get_core(funcs, i);
        const char* name = lean_string_cstr(lean_ctor_get(pair, 0));
        lean_object* fn = lean_ctor_get(pair, 1);
        lean_inc_ref(fn);
        lean_luau_State_pushCClosure_impl(data->state, data->main, fn, name, 0);
        lua_setfield(data->state, -2, name);
    }
    return lean_io_result_mk_ok(lean_box(0));
}

LEAN_EXPORT lean_obj_res lean_luau_State_getMetaField(lean_luau_State state, uint32_t obj, b_lean_obj_arg event, lean_obj_arg io_) {
    lean_luau_State_data* data = lean_luau_State_fromRepr(state);
    lean_luau_guard_valid(data);
    return lean_io_result_mk_ok(lean_box(
        luaL_getmetafield(data->state, (int32_t)obj, lean_string_cstr(event)) != 0
    ));
}

LEAN_EXPORT lean_obj_res lean_luau_State_callMeta(lean_luau_State state, uint32_t obj, b_lean_obj_arg event, lean_obj_arg io_) {
    lean_luau_State_data* data = lean_luau_State_fromRepr(state);
    lean_luau_guard_valid(data);
    return lean_io_result_mk_ok(lean_box(
        luaL_callmeta(data->state, (int32_t)obj, lean_string_cstr(event)) != 0
    ));
}

LEAN_EXPORT lean_obj_res lean_luau_State_typeError(lean_luau_State state, uint32_t narg, b_lean_obj_arg tname, lean_obj_arg io_) {
    lean_luau_State_data* data = lean_luau_State_fromRepr(state);
    lean_luau_guard_valid(data);
    luaL_typeerrorL(data->state, (int32_t)narg, lean_string_cstr(tname));
}

LEAN_EXPORT lean_obj_res lean_luau_State_typeError_2(lean_luau_State state, uint32_t narg, uint8_t type, lean_obj_arg io_) {
    lean_luau_State_data* data = lean_luau_State_fromRepr(state);
    lean_luau_guard_valid(data);
    luaL_typeerrorL(data->state, (int32_t)narg, lua_typename(data->state, type));
}

LEAN_EXPORT lean_obj_res lean_luau_State_argError(lean_luau_State state, uint32_t narg, b_lean_obj_arg extraMsg, lean_obj_arg io_) {
    lean_luau_State_data* data = lean_luau_State_fromRepr(state);
    lean_luau_guard_valid(data);
    luaL_argerrorL(data->state, (int32_t)narg, lean_string_cstr(extraMsg));
}

LEAN_EXPORT lean_obj_res lean_luau_State_checkString(lean_luau_State state, uint32_t numArg, lean_obj_arg io_) {
    lean_luau_State_data* data = lean_luau_State_fromRepr(state);
    lean_luau_guard_valid(data);
    return lean_io_result_mk_ok(lean_mk_string(luaL_checkstring(data->state, (int32_t)numArg)));
}

LEAN_EXPORT lean_obj_res lean_luau_State_optString(lean_luau_State state, uint32_t numArg, b_lean_obj_arg def, lean_obj_arg io_) {
    lean_luau_State_data* data = lean_luau_State_fromRepr(state);
    lean_luau_guard_valid(data);
    if (lua_isnoneornil(data->state, numArg)) {
        lean_inc_ref(def);
        return lean_io_result_mk_ok(def);
    }
    return lean_io_result_mk_ok(lean_mk_string(luaL_checkstring(data->state, (int32_t)numArg)));
}

LEAN_EXPORT lean_obj_res lean_luau_State_checkNumber(lean_luau_State state, uint32_t numArg, lean_obj_arg io_) {
    lean_luau_State_data* data = lean_luau_State_fromRepr(state);
    lean_luau_guard_valid(data);
    return lean_io_result_mk_ok(lean_box_float(luaL_checknumber(data->state, (int32_t)numArg)));
}

LEAN_EXPORT lean_obj_res lean_luau_State_optNumber(lean_luau_State state, uint32_t numArg, double def, lean_obj_arg io_) {
    lean_luau_State_data* data = lean_luau_State_fromRepr(state);
    lean_luau_guard_valid(data);
    return lean_io_result_mk_ok(lean_box_float(luaL_optnumber(data->state, (int32_t)numArg, def)));
}

LEAN_EXPORT lean_obj_res lean_luau_State_checkBoolean(lean_luau_State state, uint32_t numArg, lean_obj_arg io_) {
    lean_luau_State_data* data = lean_luau_State_fromRepr(state);
    lean_luau_guard_valid(data);
    return lean_io_result_mk_ok(lean_box(luaL_checkboolean(data->state, (int32_t)numArg) != 0));
}

LEAN_EXPORT lean_obj_res lean_luau_State_optBoolean(lean_luau_State state, uint32_t numArg, uint8_t def, lean_obj_arg io_) {
    lean_luau_State_data* data = lean_luau_State_fromRepr(state);
    lean_luau_guard_valid(data);
    return lean_io_result_mk_ok(lean_box(luaL_optboolean(data->state, (int32_t)numArg, def) != 0));
}

LEAN_EXPORT lean_obj_res lean_luau_State_checkUnsigned(lean_luau_State state, uint32_t numArg, lean_obj_arg io_) {
    lean_luau_State_data* data = lean_luau_State_fromRepr(state);
    lean_luau_guard_valid(data);
    return lean_io_result_mk_ok(lean_box_uint32(luaL_checkunsigned(data->state, (int32_t)numArg)));
}

LEAN_EXPORT lean_obj_res lean_luau_State_optUnsigned(lean_luau_State state, uint32_t numArg, uint32_t def, lean_obj_arg io_) {
    lean_luau_State_data* data = lean_luau_State_fromRepr(state);
    lean_luau_guard_valid(data);
    return lean_io_result_mk_ok(lean_box_uint32(luaL_optunsigned(data->state, (int32_t)numArg, def)));
}

LEAN_EXPORT lean_obj_res lean_luau_State_checkInteger(lean_luau_State state, uint32_t numArg, lean_obj_arg io_) {
    lean_luau_State_data* data = lean_luau_State_fromRepr(state);
    lean_luau_guard_valid(data);
    return lean_io_result_mk_ok(lean_box_uint32(luaL_checkinteger(data->state, (int32_t)numArg)));
}

LEAN_EXPORT lean_obj_res lean_luau_State_optInteger(lean_luau_State state, uint32_t numArg, uint32_t def, lean_obj_arg io_) {
    lean_luau_State_data* data = lean_luau_State_fromRepr(state);
    lean_luau_guard_valid(data);
    return lean_io_result_mk_ok(lean_box_uint32((int32_t)luaL_optinteger(data->state, (int32_t)numArg, (int32_t)def)));
}

LEAN_EXPORT lean_obj_res lean_luau_State_checkStackL(lean_luau_State state, uint32_t sz, b_lean_obj_arg msg, lean_obj_arg io_) {
    lean_luau_State_data* data = lean_luau_State_fromRepr(state);
    lean_luau_guard_valid(data);
    luaL_checkstack(data->state, (int32_t)sz, lean_string_cstr(msg));
    return lean_io_result_mk_ok(lean_box(0));
}

LEAN_EXPORT lean_obj_res lean_luau_State_checkType(lean_luau_State state, uint32_t narg, uint8_t t, lean_obj_arg io_) {
    lean_luau_State_data* data = lean_luau_State_fromRepr(state);
    lean_luau_guard_valid(data);
    luaL_checktype(data->state, (int32_t)narg, t);
    return lean_io_result_mk_ok(lean_box(0));
}

LEAN_EXPORT lean_obj_res lean_luau_State_checkAny(lean_luau_State state, uint32_t narg, uint8_t t, lean_obj_arg io_) {
    lean_luau_State_data* data = lean_luau_State_fromRepr(state);
    lean_luau_guard_valid(data);
    luaL_checkany(data->state, (int32_t)narg);
    return lean_io_result_mk_ok(lean_box(0));
}

LEAN_EXPORT lean_obj_res lean_luau_State_newMetatable(lean_luau_State state, b_lean_obj_arg tname, lean_obj_arg io_) {
    lean_luau_State_data* data = lean_luau_State_fromRepr(state);
    lean_luau_guard_valid(data);
    return lean_io_result_mk_ok(lean_box(luaL_newmetatable(data->state, lean_string_cstr(tname)) != 0));
}

LEAN_EXPORT lean_obj_res lean_luau_State_checkUDataTagged(lean_luau_State state, uint32_t idx, uint32_t tag, b_lean_obj_arg tname, lean_obj_arg io_) {
    lean_luau_State_data* data = lean_luau_State_fromRepr(state);
    lean_luau_guard_valid(data);
    const char* tname_c = lean_string_cstr(tname);
    int idx_c = (int32_t)idx;
    if (lua_userdatatag(data->state, idx_c) != tag) {
        luaL_typeerrorL(data->state, idx_c, tname_c);
    }
    lean_luau_userdata* userdata = luaL_checkudata(data->state, idx_c, tname_c);
    lean_inc(userdata->obj);
    return lean_io_result_mk_ok(userdata->obj);
}

LEAN_EXPORT lean_obj_res lean_luau_State_checkUData(lean_luau_State state, uint32_t idx, b_lean_obj_arg tname, lean_obj_arg io_) {
    return lean_luau_State_checkUDataTagged(state, idx, LUA_UTAG_LIMIT, tname, lean_box(0));
}

LEAN_EXPORT lean_obj_res lean_luau_State_checkBuffer(lean_luau_State state, uint32_t numArg, lean_obj_arg io_) {
    lean_luau_State_data* data = lean_luau_State_fromRepr(state);
    lean_luau_guard_valid(data);
    size_t len;
    void* buf = luaL_checkbuffer(data->state, (int32_t)numArg, &len);
    lean_object* res = lean_alloc_sarray(1, len, len);
    memcpy(lean_sarray_cptr(res), buf, len);
    return lean_io_result_mk_ok(res);
}

LEAN_EXPORT lean_obj_res lean_luau_State_where(lean_luau_State state, uint32_t lvl, lean_obj_arg io_) {
    lean_luau_State_data* data = lean_luau_State_fromRepr(state);
    lean_luau_guard_valid(data);
    luaL_where(data->state, (int32_t)lvl);
    return lean_io_result_mk_ok(lean_box(0));
}

LEAN_EXPORT lean_obj_res lean_luau_State_toStringL(lean_luau_State state, uint32_t idx, lean_obj_arg io_) {
    lean_luau_State_data* data = lean_luau_State_fromRepr(state);
    lean_luau_guard_valid(data);
    return lean_io_result_mk_ok(lean_mk_string(luaL_tolstring(data->state, (int32_t)idx, NULL)));
}

LEAN_EXPORT lean_obj_res lean_luau_State_findTable(lean_luau_State state, uint32_t idx, b_lean_obj_arg fname, uint32_t szhint, lean_obj_arg io_) {
    lean_luau_State_data* data = lean_luau_State_fromRepr(state);
    lean_luau_guard_valid(data);
    const char* res = luaL_findtable(data->state, (int32_t)idx, lean_string_cstr(fname), (int32_t)szhint);
    if (res == NULL) {
        return lean_io_result_mk_ok(lean_mk_option_none());
    }
    return lean_io_result_mk_ok(lean_mk_option_some(lean_mk_string(res)));
}

LEAN_EXPORT lean_obj_res lean_luau_State_typeNameL(lean_luau_State state, uint32_t idx, lean_obj_arg io_) {
    lean_luau_State_data* data = lean_luau_State_fromRepr(state);
    lean_luau_guard_valid(data);
    return lean_io_result_mk_ok(lean_mk_string(luaL_typename(data->state, (int32_t)idx)));
}

LEAN_EXPORT lean_obj_res lean_luau_State_openBase(lean_luau_State state, lean_obj_arg io_) {
    lean_luau_State_data* data = lean_luau_State_fromRepr(state);
    lean_luau_guard_valid(data);
    luaopen_base(data->state);
    return lean_io_result_mk_ok(lean_box(0));
}

LEAN_EXPORT lean_obj_res lean_luau_State_openCoroutine(lean_luau_State state, lean_obj_arg io_) {
    lean_luau_State_data* data = lean_luau_State_fromRepr(state);
    lean_luau_guard_valid(data);
    luaopen_coroutine(data->state);
    return lean_io_result_mk_ok(lean_box(0));
}

LEAN_EXPORT lean_obj_res lean_luau_State_openTable(lean_luau_State state, lean_obj_arg io_) {
    lean_luau_State_data* data = lean_luau_State_fromRepr(state);
    lean_luau_guard_valid(data);
    luaopen_table(data->state);
    return lean_io_result_mk_ok(lean_box(0));
}

LEAN_EXPORT lean_obj_res lean_luau_State_openOs(lean_luau_State state, lean_obj_arg io_) {
    lean_luau_State_data* data = lean_luau_State_fromRepr(state);
    lean_luau_guard_valid(data);
    luaopen_os(data->state);
    return lean_io_result_mk_ok(lean_box(0));
}

LEAN_EXPORT lean_obj_res lean_luau_State_openString(lean_luau_State state, lean_obj_arg io_) {
    lean_luau_State_data* data = lean_luau_State_fromRepr(state);
    lean_luau_guard_valid(data);
    luaopen_string(data->state);
    return lean_io_result_mk_ok(lean_box(0));
}

LEAN_EXPORT lean_obj_res lean_luau_State_openBit32(lean_luau_State state, lean_obj_arg io_) {
    lean_luau_State_data* data = lean_luau_State_fromRepr(state);
    lean_luau_guard_valid(data);
    luaopen_bit32(data->state);
    return lean_io_result_mk_ok(lean_box(0));
}

LEAN_EXPORT lean_obj_res lean_luau_State_openBuffer(lean_luau_State state, lean_obj_arg io_) {
    lean_luau_State_data* data = lean_luau_State_fromRepr(state);
    lean_luau_guard_valid(data);
    luaopen_buffer(data->state);
    return lean_io_result_mk_ok(lean_box(0));
}

LEAN_EXPORT lean_obj_res lean_luau_State_openUtf8(lean_luau_State state, lean_obj_arg io_) {
    lean_luau_State_data* data = lean_luau_State_fromRepr(state);
    lean_luau_guard_valid(data);
    luaopen_utf8(data->state);
    return lean_io_result_mk_ok(lean_box(0));
}

LEAN_EXPORT lean_obj_res lean_luau_State_openMath(lean_luau_State state, lean_obj_arg io_) {
    lean_luau_State_data* data = lean_luau_State_fromRepr(state);
    lean_luau_guard_valid(data);
    luaopen_math(data->state);
    return lean_io_result_mk_ok(lean_box(0));
}

LEAN_EXPORT lean_obj_res lean_luau_State_openDebug(lean_luau_State state, lean_obj_arg io_) {
    lean_luau_State_data* data = lean_luau_State_fromRepr(state);
    lean_luau_guard_valid(data);
    luaopen_debug(data->state);
    return lean_io_result_mk_ok(lean_box(0));
}

LEAN_EXPORT lean_obj_res lean_luau_State_openLibs(lean_luau_State state, lean_obj_arg io_) {
    lean_luau_State_data* data = lean_luau_State_fromRepr(state);
    lean_luau_guard_valid(data);
    luaL_openlibs(data->state);
    return lean_io_result_mk_ok(lean_box(0));
}

LEAN_EXPORT lean_obj_res lean_luau_State_sandbox(lean_luau_State state, lean_obj_arg io_) {
    lean_luau_State_data* data = lean_luau_State_fromRepr(state);
    lean_luau_guard_valid(data);
    luaL_sandbox(data->state);
    return lean_io_result_mk_ok(lean_box(0));
}

LEAN_EXPORT lean_obj_res lean_luau_State_sandboxThread(lean_luau_State state, lean_obj_arg io_) {
    lean_luau_State_data* data = lean_luau_State_fromRepr(state);
    lean_luau_guard_valid(data);
    luaL_sandboxthread(data->state);
    return lean_io_result_mk_ok(lean_box(0));
}
