# Checks that Windows 7 SP1 can load FSVR's Windows binaries: every function they import must be one that
# Windows 7's DLLs export, as listed in tools/win7_exports.txt. A missing one stops a plug-in or the app
# from loading there ("entry point not found"); what a loaded binary then does is not checked here.
#
#   python tools/check_win7.py bin/CLAP bin/VST3 ...   checks the .exe, .dll, .clap and .vst3 files under them
#   python tools/check_win7.py --make                  writes win7_exports.txt from the Windows 7.1A SDK's
#                                                      import libraries (installed with older Visual Studios)
#
# Needs dumpbin, which vswhere finds in any Visual Studio with the C++ tools.
import glob, os, re, subprocess, sys

HERE = os.path.dirname(os.path.abspath(__file__))
LIST = os.path.join(HERE, "win7_exports.txt")
SDK = r"C:\Program Files (x86)\Microsoft SDKs\Windows\v7.1A\Lib"
# The DLLs FSVR imports from; one more needs adding here and --make run again.
DLLS = ["advapi32", "avrt", "comctl32", "comdlg32", "dwmapi", "gdi32", "kernel32", "mfplat", "mfreadwrite",
        "ole32", "shell32", "user32", "uxtheme", "winmm"]


TOOL = []


def dumpbin(*args):
    if not TOOL:
        vswhere = os.path.expandvars(r"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe")
        TOOL.extend(subprocess.run([vswhere, "-latest", "-find", r"VC\Tools\MSVC\**\bin\Hostx64\x64\dumpbin.exe"],
                               capture_output=True, text=True).stdout.splitlines()[:1])
    return subprocess.run([TOOL[0], "/nologo", *args], capture_output=True, text=True, errors="replace").stdout


def make():
    out = []
    for dll in DLLS:
        names = set()
        for arch in ("x64", ""):   # ponytail: x64 and x86 together; a function only one of them had would pass for both
            lib = [f for f in glob.glob(os.path.join(SDK, arch, "*.lib")) if os.path.basename(f).lower() == dll + ".lib"]
            for line in dumpbin("/exports", lib[0]).splitlines() if lib else []:
                m = re.match(r"^\s+(?:\d+\s+)?([_@]?[A-Za-z]\w*)(@\d+)?$", line)
                if m and line.startswith("    "):
                    names.add(m.group(1).lstrip("_@") if arch == "" else m.group(1))
        if not names:
            sys.exit("no Windows 7 import library for " + dll)
        out += ["[%s.dll]" % dll] + sorted(names)
    with open(LIST, "w", newline="\n") as f:
        f.write("\n".join(out) + "\n")
    print("wrote %s: %d DLLs" % (LIST, len(DLLS)))


def imports(path):
    found, dll = {}, None
    for line in dumpbin("/imports", path).splitlines():
        if line.strip() == "Summary":
            break
        m = re.match(r"^\s{4}(\S+\.dll)$", line, re.I)
        if m:
            dll = found.setdefault(m.group(1).lower(), set())
            continue
        m = re.match(r"^\s+[0-9A-Fa-f]+\s+(\w+)$", line)   # an import by name; one by ordinal reads "Ordinal n"
        if m and dll is not None:
            dll.add(m.group(1))
    return found


def check(paths):
    exports, dll = {}, None
    for line in open(LIST).read().split():
        if line.startswith("["):
            dll = exports.setdefault(line[1:-1], set())
        else:
            dll.add(line)
    files = []
    for p in paths:
        files += [p] if os.path.isfile(p) else [f for f in glob.glob(os.path.join(p, "**", "*"), recursive=True)
                                                 if os.path.isfile(f) and f.lower().endswith((".exe", ".dll", ".clap", ".vst3"))]
    bad = 0
    for f in files:
        for d, names in sorted(imports(f).items()):
            missing = sorted(names - exports[d]) if d in exports else ["(no Windows 7 list for this DLL)"]
            if missing:
                bad += 1
                print("%s: %s: %s" % (f, d, " ".join(missing)))
    print("check_win7: %d files, %s" % (len(files), "%d missing from Windows 7" % bad if bad else "all load on Windows 7"))
    return bad == 0 and len(files) > 0


if __name__ == "__main__":
    if sys.argv[1:] == ["--make"]:
        make()
    else:
        sys.exit(0 if check(sys.argv[1:]) else 1)
