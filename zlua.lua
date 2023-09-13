local zsh = zsh

zsh.o = setmetatable({}, {
	__index = function(_, name)
		return zsh.get_option(name)
	end,
	__newindex = function(_, name, value)
		zsh.set_option(name, value)
	end,
})

zsh.g = setmetatable({}, {
	__index = function()
		error('write-only')
	end,
	__newindex = function(_, name, value)
		zsh.set_var(name, value)
	end,
})

local function impl_typed_reader(namespace, fn)
	zsh[namespace] = setmetatable({}, {
		__index = function(_, name)
			return fn(name)
		end,
		__newindex = function()
			error('read-only')
		end,
	})
end

impl_typed_reader('gs', zsh.get_var_string)
impl_typed_reader('gn', zsh.get_var_number)
impl_typed_reader('ga', zsh.get_var_array)
impl_typed_reader('gh', zsh.get_var_assoc)

local ZshFunctionMt = {
	__call = function(self, ...)
		return zsh.call_function(self.name, self.name, ...)
	end,
}

local zsh_fn_cache = setmetatable({}, {
	__mode = 'kv',
	__index = function(self, name)
		local t = setmetatable({ name = name }, ZshFunctionMt)
		self[name] = t
		return t
	end,
})

zsh._fn_refs = {}
zsh.fn = setmetatable({}, {
	__index = zsh_fn_cache,
	__newindex = function(_, name, fn)
		assert(type(fn) == 'function')
		local ref = #zsh._fn_refs + 1
		zsh._fn_refs[ref] = fn
		zsh.exec(string.format([['builtin' function %s() { luado 'return zsh._fn_refs[%d](...)' $@ }]], name, ref))
	end,
})

do
	zsh.g.ZLUADIR = assert(zsh.gs.ZLUADIR or zsh.gs.ZDOTDIR or zsh.gs.HOME)

	local function remove_relative(path)
		return string.gsub(path, '[^;]+', function(file)
			if string.sub(file, 1, 1) == '/' then
				return file
			end
			return ''
		end)
	end

	local function sanitize(which, ext)
		package[which] =
			string.format('%s/?%s;%s/lua/?%s;%s', zsh.gs.ZLUADIR, ext, zsh.gs.ZLUADIR, ext, remove_relative(package[which]))
	end

	sanitize('path', '.lua')
	sanitize('cpath', '.so')
end
