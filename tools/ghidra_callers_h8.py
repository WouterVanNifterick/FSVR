# Print the instructions leading into every call of the given functions (H8 project).
#   python tools/ghidra_callers_h8.py an1x 0x564a0 0x564f4 0x56562 [--n 8]
import sys, io
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding="utf-8", errors="replace")
import os
import pyghidra
pyghidra.start(install_dir=os.environ.get("GHIDRA_INSTALL_DIR", "/opt/ghidra"))
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1].parent / "FS1R_DISASM"
PROJ = {"an1x": ("AN1X", "an1x_v104_cpuview.bin"), "plg150an": ("PLG150AN", "PLG150-AN_cpuview.bin")}
name, prog = PROJ[sys.argv[1]]
n = int(sys.argv[sys.argv.index("--n") + 1]) if "--n" in sys.argv else 8
targets = [int(a, 16) for a in sys.argv[2:] if not a.startswith("--") and a != str(n)]
with pyghidra.open_program(None, project_location=str(ROOT / f"{name}_GHIDRA_PROJ"), project_name=name,
                           program_name=prog, analyze=False, nested_project_location=False) as api:
    p = api.getCurrentProgram()
    L = p.getListing()
    fm = p.getFunctionManager()
    for t in targets:
        addr = api.toAddr(f"0x{t:x}")
        for ref in p.getReferenceManager().getReferencesTo(addr):
            if not ref.getReferenceType().isCall():
                continue
            site = ref.getFromAddress()
            fn = fm.getFunctionContaining(site)
            print(f"===== call {t:x} from {site} in {fn.getName() if fn else '?'}")
            ins = L.getInstructionAt(site)
            back = []
            for _ in range(n):
                ins = ins.getPrevious() if ins else None
                if ins is None:
                    break
                back.append(f"{ins.getAddress()}  {ins}")
            for line in reversed(back):
                print("  " + line)
