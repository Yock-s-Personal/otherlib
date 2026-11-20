"""Regenerates build/compile_commands.json, the database clangd reads in VS Code.

The build never writes this file: the Visual Studio generator ignores
CMAKE_EXPORT_COMPILE_COMMANDS. It comes from the Clang Power Tools extension for Visual
Studio, whose script reads the generated solution and writes one clang command per
source file. Nothing is compiled.

  python tools/export_compile_commands.py              (Debug)
  python tools/export_compile_commands.py -c Release

Run it after adding a module, or after changing a target's include directories or
defines. Until then clangd has no entry for the new files, borrows the command of an
unrelated one, and reports includes as missing in code that builds.

Needs a configured build tree (python cli.py build) and the Clang Power Tools extension
installed in Visual Studio. If the editor keeps its old diagnostics afterwards, restart
the clangd language server.
"""
import argparse
import glob
import json
import os
import shutil
import subprocess
import sys

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BUILD_DIR = os.path.join(REPO_ROOT, "build")
DATABASE = os.path.join(BUILD_DIR, "compile_commands.json")
CONFIGS = ["Debug", "Release", "Profile", "ProfileD"]
PLATFORM = "x64"
## the extension folder name is random and changes when the extension updates
EXPORTER_GLOB = os.path.join("Microsoft", "VisualStudio", "*", "Extensions", "*", "Tooling", "v1", "clang-build.ps1")


def log(message):
  print(f"[export_compile_commands] {message}", flush=True)


def find_solution():
  for pattern in ("*.slnx", "*.sln"):
    matches = sorted(glob.glob(os.path.join(BUILD_DIR, pattern)))
    if matches:
      return matches[0]
  return None


def find_exporter(solution):
  candidates = glob.glob(os.path.join(os.environ.get("LOCALAPPDATA", ""), EXPORTER_GLOB))
  if solution.endswith(".slnx"):
    ## every installed Visual Studio carries its own copy and the older ones only read .sln
    def reads_slnx(path):
      with open(path, encoding="utf-8-sig", errors="ignore") as script:
        return ".slnx" in script.read()
    candidates = [c for c in candidates if reads_slnx(c)]
  return max(candidates, key=os.path.getmtime, default=None)


def count_entries():
  ## the exporter writes a byte order mark
  with open(DATABASE, encoding="utf-8-sig") as database:
    return len(json.load(database))


def main(argv):
  parser = argparse.ArgumentParser(description="Regenerate build/compile_commands.json for clangd.")
  parser.add_argument("-c", "--config", default="Debug", choices=CONFIGS, help="build config whose defines the commands carry")
  args = parser.parse_args(argv)

  if os.name != "nt":
    log("this export reads a Visual Studio solution, so it only runs on Windows")
    return 1

  solution = find_solution()
  if solution is None:
    log(f"no solution in '{BUILD_DIR}'; configure the build first with: python cli.py build")
    return 1

  exporter = find_exporter(solution)
  if exporter is None:
    log("Clang Power Tools was not found; install the extension in Visual Studio, or use its")
    log("'Export Compilation Database' command there")
    return 1

  ## the exporter writes the file piece by piece, so a failed run leaves it cut short and
  ##  the whole tree turns red in the editor; the previous file is kept until this one loads
  backup = DATABASE + ".previous"
  if os.path.exists(DATABASE):
    shutil.copyfile(DATABASE, backup)

  log(f"exporting {args.config}|{PLATFORM} from '{solution}' (about a minute)")
  command = ["powershell", "-NoProfile", "-File", exporter, "-dir", solution,
             "-active-config", f"{args.config}|{PLATFORM}", "-export-jsondb"]
  ## the exporter writes into its working directory
  result = subprocess.run(command, cwd=BUILD_DIR, capture_output=True, text=True, errors="replace")

  try:
    entries = count_entries() if result.returncode == 0 else 0
  except (OSError, ValueError):
    entries = 0

  if entries == 0:
    log(f"export failed (exit code {result.returncode}); last output:")
    for line in (result.stdout + result.stderr).strip().splitlines()[-15:]:
      print(f"  {line}")
    if os.path.exists(backup):
      shutil.move(backup, DATABASE)
      log("the previous database was put back")
    return 1

  if os.path.exists(backup):
    os.remove(backup)
  log(f"wrote {entries} entries to '{DATABASE}'")
  return 0


if __name__ == "__main__":
  sys.exit(main(sys.argv[1:]))
