import subprocess
import shutil
from pathlib import Path

target_libs = ['G2D', 'ai', 'amcnotstub', 'amcstubs', 'ar', 'ax', 'axfx', 'base', 'card', 'db', 'demo', 'dolformat', 'dsp', 'dtk', 'dvd', 'fileCache', 'gx', 'hio', 'mcc', 'mix', 'mtx', 'odemustubs', 'odenotstub', 'os', 'pad', 'perf', 'seq', 'support', 'syn', 'texPalette', 'vi']

print("deleting old files")
shutil.rmtree("baserom/release", ignore_errors=True)
shutil.rmtree("baserom/debug", ignore_errors=True)

rel = " ".join(f"baserom/{x}.a" for x in target_libs)
dbg = " ".join(f"baserom/{x}D.a" for x in target_libs)

print("extracting release")
subprocess.run(f"tools/dtk.exe ar extract {rel} --out baserom/release/src")

print("extracting debug")
subprocess.run(f"tools/dtk.exe ar extract {dbg} --out baserom/debug/src")

print("renaming debug")
for p in Path("baserom/debug/src").iterdir():
	p.rename(p.with_name(p.name[:-1]))

print("running disasm/dump")
for o in Path("baserom").glob("**/*.o"):
	s = o.with_suffix(".s")
	c = o.with_name(o.name[:-2] + "_DWARF.c")
	subprocess.run(f"tools/dtk.exe elf disasm {o} {s}")
	subprocess.run(f"tools/dtk.exe dwarf dump {o} -o {c}")
