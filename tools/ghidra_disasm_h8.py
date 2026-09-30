# Print the instruction listing for address ranges of an H8 project (PLG150AN or AN1X).
#   python tools/ghidra_disasm_h8.py an1x 0x48922-0x48a50 [...]
import sys, io
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding="utf-8", errors="replace")
import os
import pyghidra
pyghidra.start(install_dir=os.environ.get("GHIDRA_INSTALL_DIR", "/opt/ghidra"))
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1].parent / "FS1R_DISASM"
PROJ = {"an1x": ("AN1X", "an1x_v104_cpuview.bin"), "plg150an": ("PLG150AN", "PLG150-AN_cpuview.bin")}
name, prog = PROJ[sys.argv[1]]
ranges = [tuple(int(x, 16) for x in a.split("-")) for a in sys.argv[2:]]
with pyghidra.open_program(None, project_location=str(ROOT / f"{name}_GHIDRA_PROJ"), project_name=name,
                           program_name=prog, analyze=False, nested_project_location=False) as api:
    p = api.getCurrentProgram()
    for lo, hi in ranges:
        print(f"===== {lo:x}-{hi:x}")
        for ins in p.getListing().getInstructions(api.toAddr(f"0x{lo:x}"), True):
            if ins.getAddress().getOffset() >= hi:
                break
            print(f"{ins.getAddress()}  {ins}")
