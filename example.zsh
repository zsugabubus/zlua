#!/usr/bin/zsh
zmodload lua

# Evaluate Lua code from Zsh.
luado 'print("I am Lua", ...)' "passed from Zsh"

# Code is loaded from standard input if no arguments passed.
luado <<"EOF"
-- Execute Zsh code from Lua.
zsh.exec([[
function zsh_rocks() {
  print "Zsh rocks: $1"
}
]])

-- Access Zsh options.
zsh.o.xtrace = not zsh.o.xtrace
print(zsh.o.xtrace)

-- Access Zsh params.
zsh.g.param = nil
print(zsh.gs.param)

zsh.g.param = 10
print(zsh.gn.param)

zsh.g.param = 1 / 3
print(zsh.gn.param)

zsh.g.param = {1, 2, 3}
print(zsh.ga.param)

zsh.g.param = {a=1, b=2}
print(zsh.gh.param)

-- Call Zsh function.
zsh.fn.zsh_rocks("yes!")

-- Define Zsh function.
function zsh.fn.lua_rocks(...)
  print("Lua rocks:", ...)
end
EOF

lua_rocks also
