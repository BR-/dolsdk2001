import ninja_syntax
from pathlib import Path
import json
import os
import sys

f = open("build.ninja", "w")
n = ninja_syntax.Writer(f)

n.variable("ninja_required_version", "1.3")
n.newline()

n.variable("charflags", "-char unsigned")
n.variable("cflags", '-nodefaults -proc gekko -fp hard -Cpp_exceptions off -enum int -warn pragmas -pragma "cats off"')
n.variable("debugflags", "-opt level=0 -inline off -schedule off -sym on -DDEBUG")
n.variable("releaseflags", "-O4,p -inline auto -DRELEASE")
n.variable("includes", "-I- -Iinclude -ir src")
n.variable("asflags", "-mgekko -I src -I include")
n.newline()

n.rule(
	name="download_dtk",
	command="python tools\download_dtk.py $in $out",
)
n.build(
	outputs="tools/dtk.exe",
	rule="download_dtk",
	inputs="tools/dtk_version",
)
n.newline()

n.rule(
	name="build_debug",
	command="mwcc_compiler/GC/1.2.5/mwcceppc.exe $debugflags $cflags $includes -MMD -c $in -o $basedir",
	depfile="$basefile.d",
	deps="gcc",
)
n.rule(
	name="build_release",
	command="mwcc_compiler/GC/1.2.5/mwcceppc.exe $releaseflags $cflags $includes -MMD -c $in -o $basedir",
	depfile="$basefile.d",
	deps="gcc",
)
n.newline()

build_debug = Path("build/debug")
build_release = Path("build/release")
baserom = Path("baserom/release")
baseromD = Path("baserom/debug")
source_files = set()
for c in Path("src").glob("**/*.c"):
	o = c.with_suffix(".o")
	source_files.add(str(c))
	vars = {}
	if str(o).replace("\\", "/") == "src/card/CARDRename.o":
		vars["charflags"] = "-char signed"
	n.build(
		outputs=build_debug / o,
		rule="build_debug",
		inputs=c,
		variables={
			"basefile": (build_debug / o).with_suffix(""),
			"basedir": (build_debug / o).parent,
			**vars
		},
	)
	n.build(
		outputs=build_release / o,
		rule="build_release",
		inputs=c,
		variables={
			"basefile": (build_release / o).with_suffix(""),
			"basedir": (build_release / o).parent,
			**vars
		},
	)
	n.newline()

tus = []
missing_source_files = []
for o in baserom.glob("**/*.o"):
	o = o.relative_to(baserom)
	c = o.with_suffix(".c")
	name = str(c.with_suffix(""))
	tus.append({
		"name": name + "D",
		"target_path": str(baseromD / o),
		"base_path": str(build_debug / o),
		"reverse_fn_order": False,
		"complete": str(c) in source_files,
	})
	tus.append({
		"name": name,
		"target_path": str(baserom / o),
		"base_path": str(build_release / o),
		"reverse_fn_order": False,
		"complete": str(c) in source_files,
	})
	# if str(c) not in source_files:
	# 	missing_source_files.append(str(c))
	# 	n.build(
	# 		outputs=c,
	# 		rule="phony",
	# 	)
# n.newline()

configure_script = Path(os.path.relpath(os.path.abspath(sys.argv[0])))
n.variable("configure_args", sys.argv[1:])
n.variable("python", f'"{sys.executable}"')
n.rule(
	name="configure",
	command=f"$python {configure_script} $configure_args",
	generator=True,
)
n.build(
	outputs="build.ninja",
	rule="configure",
	implicit=[
		configure_script,
		"tools/ninja_syntax.py",
		# *missing_source_files,
	]
)

objdiff = {
	"min_version": "0.4.3",
	"custom_make": "ninja",
	"build_target": False,
	"watch_patterns": [
		"*.c",
		"*.h",
	],
	"units": tus,
}
with open("objdiff.json", "w") as fh:
	json.dump(objdiff, fh, indent=2)
