project(
	'zlua',
	'c',
	default_options: [
		'c_std=c99',
		'warning_level=3',
		'b_lundef=false',
		'buildtype=minsize',
		'debug=false',
	],
)

byte_array_gen = generator(
	find_program('hexdump'),
	output: '@PLAINNAME@.h',
	arguments: ['-ve', '/1 "%u,"', '--', '@INPUT@'],
	capture: true,
)

module_path = run_command(
	'zsh',
	'-c',
	'print $module_path',
	check: true,
).stdout().strip()

summary({
	'module_path': module_path,
}, section: 'Directories')

shared_library(
	'ua',
	'zlua.c',
	byte_array_gen.process('zlua.lua'),
	dependencies: [
		dependency('luajit'),
	],
	install: true,
	install_dir: module_path,
	# FIXME: Surely there is some other way to avoid prefix.
	name_prefix: 'l',
)

test(
	'busted',
	find_program('busted'),
	workdir: meson.source_root(),
	env: ['ZLUA_MODULE_PATH=' + meson.build_root()]
)

test(
	'lint',
	find_program('lint'),
	workdir: meson.source_root(),
)
