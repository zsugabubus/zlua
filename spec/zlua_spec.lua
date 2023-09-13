local module_path = assert(os.getenv('ZLUA_MODULE_PATH'))

local function shesc(s)
	return string.format("'%s'", string.gsub(s, "'", [['"'"']]))
end

local function capture(cmdline)
	local stdout =
		io.popen(string.format('zsh -c %s 2>&1', shesc(string.format('module_path=%s; %s', shesc(module_path), cmdline))))
	local output = stdout:read('*a')
	stdout:close()
	return output
end

local function assert_capture(cmdline, expected_output)
	assert.are.same(expected_output, capture(cmdline))
end

it('can load module', function()
	assert_capture('zmodload lua', '')
end)

describe('loader', function()
	local UNSET = 'unset ZLUADIR ZDOTDIR HOME && '

	it('should fail to load module if cannot determine $ZLUADIR', function()
		assert_capture(UNSET .. [[zmodload lua >&/dev/null; echo -n $?]], '1')
	end)

	it('should not load relative files for security reasons', function()
		local UNSAFE_FILE = "no file '[^/]"

		-- Test that has.no.match() would actually find something.
		local output = capture([[ZLUADIR=. && zmodload lua && luado 'require("/////")']])
		assert.has.match(UNSAFE_FILE, output)

		local output = capture([[zmodload lua && luado 'require("spec/print-ok")']])
		assert.has.match("module 'spec/print%-ok' not found", output)
		assert.has.match("no file '/", output)
		assert.has.no.match(UNSAFE_FILE, output)
	end)

	it('should use $ZLUADIR', function()
		assert_capture(UNSET .. [[local ZLUADIR=. && zmodload lua && luado 'require("spec/print-ok")']], 'ok\n')
	end)

	it('should use $ZDOTDIR and set $ZLUADIR', function()
		assert_capture(
			UNSET .. [[local ZDOTDIR=. && zmodload lua && luado 'require("spec/print-ok")'; echo -n ${+ZLUADIR}]],
			'ok\n1'
		)
	end)

	it('should use $HOME and set $ZLUADIR', function()
		assert_capture(
			UNSET .. [[local HOME=. && zmodload lua && luado 'require("spec/print-ok")'; echo -n ${+ZLUADIR}]],
			'ok\n1'
		)
	end)
end)

describe('luado', function()
	it('should be a built-in command', function()
		assert_capture([[zmodload lua && which luado]], 'luado: shell built-in command\n')
	end)

	it('with no arguments should load code from stdin', function()
		assert_capture([[zmodload lua && luado <<<a=1 && luado <<<print(a) ]], '1\n')
	end)

	it('should set ...', function()
		assert_capture([[zmodload lua && luado 'print(...)' 'ok' 1]], 'ok\t1\n')
	end)

	describe('should set $?', function()
		local function case_helper(code, expected, stdin)
			it(string.format("to %d with '%s'%s", expected, code, stdin and ' from stdin' or ''), function()
				assert_capture(
					string.format([[zmodload lua && luado %s%s &>/dev/null; echo -n $?]], stdin and '<<<' or '', shesc(code)),
					tostring(expected)
				)
			end)
		end

		local function case(code, expected)
			case_helper(code, expected, false)
			case_helper(code, expected, true)
		end

		case('a=1', 0)
		case('return', 0)
		case('return nil', 0)
		case('return true', 0)
		case('return false', 1)
		case('return 0', 0)
		case('return 1', 1)
		case('return 1.5', 1)
		case('return 42', 42)
		case('error(0)', 127)
		case('(syntax error', 127)
	end)

	it('should print syntax error', function()
		assert.has.match('unexpected symbol near', capture([[zmodload lua && luado '(']]))
		assert.has.match('unexpected symbol near', capture([[zmodload lua && luado '(' somearg]]))
	end)

	it('should print syntax error from stdin', function()
		assert.has.match('unexpected symbol near', capture([[zmodload lua && luado <<<(]]))
	end)

	it('should print runtime error and traceback', function()
		local output = capture([[zmodload lua && luado 'error("my error")']])
		assert.has.match('zsh:luado:.*: my error', output)
		assert.has.match('zsh:lua: stack traceback', output)
	end)
end)

describe('zsh.exec', function()
	it('should execute given code', function()
		assert_capture([[zmodload lua && luado 'zsh.exec("noglob print hello * world")']], 'hello * world\n')
	end)

	it('should return with correct status', function()
		assert_capture([[zmodload lua && luado 'print(zsh.exec(""))']], 'true\t0\n')
		assert_capture([[zmodload lua && luado 'print(zsh.exec("(exit 3)"))']], 'false\t3\n')
		assert_capture([[zmodload lua && luado 2>/dev/null 'print(zsh.exec("(syntax error"))']], 'false\t1\n')
	end)

	it('should work with large input', function()
		assert_capture(string.format([[zmodload lua && luado 'zsh.exec("%s")']], string.rep(' ', 10000)), '')
	end)
end)

describe('zsh.o', function()
	local function case(name, value)
		it(string.format("should get/set '%s' to %s", name, value), function()
			assert_capture(
				string.format(
					[=[
					zmodload lua &&
					luado 'zsh.o.%s = %s' &&
					if [[ -o %s ]]; then
						echo true
					else
						echo false
					fi &&
					luado 'print(zsh.o.%s)'
					]=],
					name,
					value,
					name,
					name
				),
				value and 'true\ntrue\n' or 'false\nfalse\n'
			)
		end)
	end

	case('extended_glob', true)
	case('extended_glob', false)
	case('extendedglob', true)
	case('extendedglob', false)
end)

describe('zsh.fn', function()
	it('should cache function objects', function()
		assert_capture([[zmodload lua && luado 'print(zsh.fn.zshfunc == zsh.fn.zshfunc)']], 'true\n')
	end)

	it('should error when attempt to call non-existing function', function()
		assert.has.match("zsh:luado:.*: no such function: 'zshfunc'", capture([[zmodload lua && luado 'zsh.fn.zshfunc()']]))
	end)

	it('should error if invalid argument passed', function()
		assert.has.match(
			'zsh:luado:.*: bad function argument',
			capture([[zshfunc() {} && zmodload lua && luado 'zsh.fn.zshfunc(function() end)']])
		)
	end)

	it('should call function with correct arguments', function()
		assert_capture(
			[[function zshfunc() { printf '%s\t' "$@" } && zmodload lua && luado 'zsh.fn.zshfunc("a", 1)']],
			'a\t1\t'
		)
	end)

	describe('should return correct status', function()
		local function case(code, expected)
			test(string.format("when '%s'", code), function()
				assert_capture(
					string.format(
						[[function zshfunc() {%s} && zmodload lua && luado 'x = zsh.fn.zshfunc(); print(type(x), x)']],
						code
					),
					expected
				)
			end)
		end

		case('', 'number\t0\n')
		case('return 1', 'number\t1\n')
		case('return 3', 'number\t3\n')
	end)

	it('should create shell function', function()
		assert_capture(
			[[zmodload lua && luado 'function zsh.fn.zshfunc(...) print(...); return 3 end' && zshfunc 'a' 42; echo $?]],
			'a\t42\n3\n'
		)
	end)
end)

describe('zsh.g', function()
	local function case(value, expected)
		it(string.format("should set param with '%s'", value), function()
			assert_capture(
				string.format([[zmodload lua && luado 'zsh.g.X = %s' && echo -n "(${(t)X})${(kv)X}"]], value),
				expected
			)
		end)
	end

	it('should unset param', function()
		assert_capture([[X= && zmodload lua && echo -n $+X && luado 'zsh.g.X = nil' && echo -n $+X]], '10')
	end)

	case('"hello"', '(scalar)hello')
	case('1', '(integer)1')
	case('1.5', '(float)1.5000000000')
	case('{}', '(array)')
	case('{"a", 1}', '(array)a 1')
	case('{[0]="a"}', '(association)0 a')
	case('{[2]="a"}', '(association)2 a')
	case('{a=1}', '(association)a 1')
end)

describe('zsh.gs', function()
	local function case(name, code, expected)
		it(name, function()
			assert_capture(string.format([[%s && zmodload lua && luado 'print(type(zsh.gs.X), zsh.gs.X)']], code), expected)
		end)
	end

	case('should get non-existing param', ':', 'nil\tnil\n')
	case('should get string param', 'local X=10', 'string\t10\n')
	case('should get integer param', 'integer X=10', 'string\t10\n')
	case('should get float param', 'float X=1.5', 'string\t1.500000000e+00\n')
	case('should get array param', 'local X=(a 1)', 'string\ta 1\n')
	case('should get association param', 'local -A X=([a]=1 [b]=2)', 'string\t1 2\n')
end)

describe('zsh.gn', function()
	local function case(name, code, expected)
		it(name, function()
			assert_capture(string.format([[%s && zmodload lua && luado 'print(type(zsh.gn.X), zsh.gn.X)']], code), expected)
		end)
	end

	case('should get non-existing param', ':', 'number\t0\n')
	case('should get string param', 'local X=10', 'number\t10\n')
	case('should get integer param', 'integer X=10', 'number\t10\n')
	case('should get float param', 'float X=1.5', 'number\t1.5\n')
end)

describe('zsh.ga', function()
	it('should get non-existing param', function()
		assert_capture([[zmodload lua && luado 'print(zsh.ga.X)']], 'nil\n')
	end)

	it('should get array param', function()
		assert_capture([[local X=(a 1) && zmodload lua && luado 'local X = zsh.ga.X; print(#X, X[1], X[2])']], '2\ta\t1\n')
	end)
end)

describe('zsh.gh', function()
	it('should get non-existing param', function()
		assert_capture([[zmodload lua && luado 'print(zsh.gh.X)']], 'nil\n')
	end)

	it('should get association param', function()
		assert_capture(
			[[local -A X=([a]=apple [b]=banana) && zmodload lua && luado 'print(zsh.gh.X.a, zsh.gh.X.b)']],
			'apple\tbanana\n'
		)
	end)
end)
