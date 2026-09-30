# Print the instruction listing for address ranges of the FS1R flash program (headless, Linux).
#   python tools/ghidra_disasm_sh2.py 0x3cc6a-0x3cc90 [...]
import sys, io
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding="utf-8", errors="replace")
import pyghidra
from pyghidra.launcher import HeadlessPyGhidraLauncher
from pathlib import Path
l = HeadlessPyGhidraLauncher(install_dir="/opt/ghidra"); l.add_vmargs("-Duser.name=Home"); l.start()
ROOT = Path(__file__).resolve().parents[1].parent / "FS1R_DISASM"
ranges = [tuple(int(x, 16) for x in a.split("-")) for a in sys.argv[1:]]
with pyghidra.open_program(None, project_location=str(ROOT / "FS1R_GHIDRA_PROJ"), project_name="FS1R",
                           program_name="fs1r_sh7044_flash_1.20_256k.bin", analyze=False, nested_project_location=False) as api:
    p = api.getCurrentProgram()
    for lo, hi in ranges:
        print(f"===== {lo:x}-{hi:x}")
        for ins in p.getListing().getInstructions(api.toAddr(f"0x{lo:x}"), True):
            if ins.getAddress().getOffset() >= hi:
                break
            print(f"{ins.getAddress()}  {ins}")
