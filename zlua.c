#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "zsh.h"

static lua_State *L;

static char *ztrdup_nul(char const *s)
{
	if (s == NULL)
		return ztrdup("");
	return ztrdup(s);
}

static int isinteger(lua_Number f)
{
	return f >= LONG_MIN && f <= LONG_MAX && f == (long)f;
}

static int l_isarray(lua_State *L, int index)
{
	int len = lua_objlen(L, index);

	lua_pushnil(L);
	while (lua_next(L, index)) {
		if (lua_type(L, -2) != LUA_TNUMBER)
			return 0;

		lua_Number f = lua_tointeger(L, -2);
		if (!isinteger(f))
			return 0;

		long l = f;
		if (l < 1 || l > len)
			return 0;

		lua_pop(L, 1);
	}

	return 1;
}

static mnumber lz_tonumber(lua_State *L, int index)
{
	mnumber ret;
	lua_Number f = lua_tonumber(L, index);
	if (isinteger(f)) {
		ret.type = MN_INTEGER;
		ret.u.l = f;
	} else {
		ret.type = MN_FLOAT;
		ret.u.d = f;
	}
	return ret;
}

static char **lz_array_values(lua_State *L, int index)
{
	int len = lua_objlen(L, index);
	char **ret = zalloc((len + 1) * sizeof(*ret));

	for (int i = 0; i < len; i++) {
		lua_rawgeti(L, index, i + 1);
		ret[i] = ztrdup_nul(lua_tostring(L, -1));
		lua_pop(L, 1);
	}

	ret[len] = NULL;

	return ret;
}

static char **lz_assoc_entries(lua_State *L, int index)
{
	int len = 0;

	lua_pushnil(L);
	while (lua_next(L, index)) {
		len += 2;
		lua_pop(L, 1);
	}

	char **ret = zalloc((len + 1) * sizeof(*ret));

	lua_pushnil(L);
	for (int i = 0; lua_next(L, index);) {
		lua_pushvalue(L, -2);
		ret[i++] = ztrdup_nul(lua_tostring(L, -1));
		ret[i++] = ztrdup_nul(lua_tostring(L, -2));
		lua_pop(L, 2);
	}

	ret[len] = NULL;

	return ret;
}

static int l_error_handler(lua_State *L)
{
	luaL_traceback(L, L, NULL, 0);
	fprintf(stderr, "zsh:lua: %s\n", lua_tostring(L, -1));
	fflush(stderr);
	lua_pop(L, 1);
	return 1;
}

static int l_zsh_exec(lua_State *L)
{
	size_t len;
	char const *s = luaL_checklstring(L, 1, &len);

	char *zs;
	char zbuf[256];

	if (len + 1 <= sizeof(zbuf)) {
		memcpy(zbuf, s, len + 1);
		zs = zbuf;
	} else {
		zs = ztrdup(s);
	}

	execstring(zs, 1, 0, "zsh/lua");

	if (zs != zbuf)
		zsfree(zs);

	long x = lastval;
	lua_pushboolean(L, x == 0);
	lua_pushinteger(L, x);
	return 2;
}

static int l_zsh_call_function(lua_State *L)
{
	char const *name = luaL_checkstring(L, 1);

	Shfunc shf = shfunctab->getnode(shfunctab, name);
	if (shf == NULL)
		return luaL_error(L, "no such function: '%s'", name);

	int first = 2;
	int len = lua_gettop(L) - first + 1;

	pushheap();
	LinkList args = newsizedlist(len);

	for (int i = 0; i < len; i++) {
		char const *s = lua_tostring(L, first + i);
		if (s == NULL) {
			popheap();
			return luaL_error(
				L,
				"bad function argument (string or number expected, got %s)",
				lua_typename(L, lua_type(L, i))
			);
		}
		setsizednode(args, i, dupstring(s));
	}

	long retval = doshfunc(shf, args, 1);
	popheap();

	lua_pushinteger(L, retval);
	return 1;
}

static int l_zsh_get_option(lua_State *L)
{
	char const *name = luaL_checkstring(L, 1);

	int optno = optlookup(name);
	if (!optno)
		return luaL_error(L, "no such option: '%s'", name);

	lua_pushboolean(L, opts[optno]);
	return 1;
}

static int l_zsh_set_option(lua_State *L)
{
	char const *name = luaL_checkstring(L, 1);
	int value = lua_toboolean(L, 2) ? 1 : 0;

	int optno = optlookup(name);
	if (!optno)
		return luaL_error(L, "no such option: '%s'", name);

	if (dosetopt(optno, value, 0, opts))
		return luaL_error(L, "can't change option: '%s'", name);

	return 0;
}

static int l_zsh_set_var(lua_State *L)
{
	char const *name = luaL_checkstring(L, 1);
	int type = lua_type(L, 2);

	char *s = ztrdup(name);

	switch (type) {
	case LUA_TNONE:
	case LUA_TNIL:
		unsetparam(s);
		break;
	case LUA_TNUMBER:
		setnparam(s, lz_tonumber(L, 2));
		break;
	case LUA_TSTRING:
		setsparam(s, ztrdup(lua_tostring(L, 2)));
		break;
	case LUA_TTABLE:
		if (l_isarray(L, 2))
			setaparam(s, lz_array_values(L, 2));
		else
			sethparam(s, lz_assoc_entries(L, 2));
		break;
	default:
		zsfree(s);
		return luaL_error(
			L,
			"bad option value (nil, number, string or table expected, got %s)",
			lua_typename(L, type)
		);
	}

	zsfree(s);

	return 0;
}

static int l_zsh_get_var_string(lua_State *L)
{
	char const *name = luaL_checkstring(L, 1);

	char *s = ztrdup(name);
	char *value = getsparam(s);
	zsfree(s);

	if (value == NULL)
		return 0;

	lua_pushstring(L, value);
	return 1;
}

static int l_zsh_get_var_number(lua_State *L)
{
	char const *name = luaL_checkstring(L, 1);

	char *s = ztrdup(name);
	mnumber value = getnparam(s);
	zsfree(s);

	if (value.type == MN_INTEGER)
		lua_pushinteger(L, value.u.l);
	else
		lua_pushnumber(L, value.u.d);
	return 1;
}

static int l_zsh_get_var_array(lua_State *L)
{
	char const *name = luaL_checkstring(L, 1);

	char *s = ztrdup(name);
	char **values = getaparam(s);
	zsfree(s);

	if (values == NULL)
		return 0;

	int len = 0;
	while (values[len] != NULL)
		len++;

	lua_createtable(L, len, 0);
	for (int i = 0; i < len; i++) {
		lua_pushstring(L, values[i]);
		lua_rawseti(L, -2, i + 1);
	}
	return 1;
}

static int l_zsh_get_var_assoc(lua_State *L)
{
	char const *name = luaL_checkstring(L, 1);

	char *s = ztrdup(name);
	char **keys = gethparam(s);
	char **values = gethkparam(s);
	zsfree(s);

	if (keys == NULL)
		return 0;

	int len = 0;
	while (keys[len] != NULL)
		len++;

	lua_createtable(L, 0, len);
	for (int i = 0; i < len; i++) {
		lua_pushstring(L, values[i]);
		lua_pushstring(L, keys[i]);
		lua_rawset(L, -3);
	}
	return 1;
}

static luaL_Reg const ZSH_REGISTRY[] = {
	{"exec", l_zsh_exec},
	{"call_function", l_zsh_call_function},
	{"set_option", l_zsh_set_option},
	{"get_option", l_zsh_get_option},
	{"set_var", l_zsh_set_var},
	{"get_var_string", l_zsh_get_var_string},
	{"get_var_number", l_zsh_get_var_number},
	{"get_var_array", l_zsh_get_var_array},
	{"get_var_assoc", l_zsh_get_var_assoc},
	{NULL, NULL},
};

static int bin_luado(char *nam, char **args, Options ops, int func)
{
	(void)ops, (void)func;

	lua_pushcfunction(L, l_error_handler);

	int err;
	int nargs = 0;
	if (args[0] == NULL) {
		/* luaL_loadfile(L, NULL) uses stdin that probably saves same
		 * internal state that makes it non-reentrant. We must re-open
		 * stdin every time. */
		err = luaL_loadfile(L, "/dev/stdin");
	} else {
		err = luaL_loadstring(L, args[0]);

		for (int i = 1; !err && args[i] != NULL; i++) {
			lua_pushstring(L, args[i]);
			nargs++;
		}
	}

	if (!err)
		err = lua_pcall(L, nargs, 1, -(nargs + 2));

	int retval;
	if (err) {
		fprintf(stderr, "zsh:%s: %s\n", nam, lua_tostring(L, -1));
		fflush(stderr);
		retval = 127;
	} else if (lua_type(L, -1) == LUA_TNUMBER) {
		retval = lua_tointeger(L, -1);
	} else {
		retval = lua_isnoneornil(L, -1) || lua_toboolean(L, -1) ? 0 : 1;
	}
	lua_settop(L, 0);

	return retval;
}

static struct builtin bintab[] = {
	BUILTIN("luado", 0, bin_luado, 0, -1, 0, NULL, NULL),
};

static struct features module_features = {
	/* clang-format off */
	bintab, sizeof(bintab) / sizeof(*bintab),
	NULL, 0,
	NULL, 0,
	NULL, 0,
	0
	/* clang-format on */
};

static const char *l_stringchunk_reader(lua_State *L, void *data, size_t *size)
{
	(void)L;

	char const **s = data;
	char const *x = *s;
	*s = NULL;
	if (x != NULL)
		*size = strlen(x);
	return x;
}

static int l_loadstringchunk(lua_State *L, char const *s, char const *chunkname)
{
	return lua_load(L, l_stringchunk_reader, &s, chunkname);
}

int setup_(Module m)
{
	(void)m;

	L = luaL_newstate();

	if (L == NULL) {
		fprintf(stderr, "zsh:lua: failed to allocate context\n");
		fflush(stderr);
		return 1;
	}

	luaL_openlibs(L);
	luaL_register(L, "zsh", ZSH_REGISTRY);

	static char const ZLUA_LUA_STRING[] = {
#include "zlua.lua.h"
		'\0',
	};

	l_loadstringchunk(L, ZLUA_LUA_STRING, "zlua");
	if (lua_pcall(L, 0, 0, 0)) {
		fprintf(stderr, "zsh:lua: %s\n", lua_tostring(L, -1));
		fflush(stderr);
		return 1;
	}

	return 0;
}

int features_(Module m, char ***features)
{
	(void)m;
	*features = featuresarray(m, &module_features);
	return 0;
}

int enables_(Module m, int **enables)
{
	(void)m;
	return handlefeatures(m, &module_features, enables);
}

int boot_(Module m)
{
	(void)m;
	return 0;
}

int cleanup_(Module m)
{
	(void)m;
	return setfeatureenables(m, &module_features, NULL);
}

int finish_(Module m)
{
	(void)m;
	lua_close(L);
	return 0;
}
