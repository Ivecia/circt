#!/usr/bin/env python3
from pathlib import Path
from termcolor import colored
import argparse
import subprocess
import tempfile
import sys
import json

parser = argparse.ArgumentParser()
parser.add_argument("input")
parser.add_argument("-t", "--test")
args = parser.parse_args()

num_failed = 0


def run_test(test: str):
  sys.stdout.write("running test " + colored(test, attrs=["bold"]))
  sys.stdout.flush()

  # Get a temporary directory to work in.
  # tmpDir = tempfile.TemporaryDirectory(delete=False)
  # tmpDirPath = Path(tmpDir.name)
  tmpDirPath = Path("/tmp/sby")
  # print(tmpDirPath)

  # Generate the Verilog.
  source_path = tmpDirPath / "source.sv"
  with open(source_path, "wb") as source:
    subprocess.check_call(
        [
            "circt-opt",
            sys.argv[1],
            "--lower-formal-to-hw",
            "--lower-verif-to-sv",
            "--export-verilog",
            "-o",
            "/dev/null",
        ],
        stdout=source,
    )

  # Generate the SymbiYosys script.
  script_path = tmpDirPath / "script.sby"
  with open(script_path, "w") as script:
    code = f"""
      [tasks]
      cover
      bmc
      induction

      [options]
      cover:
      mode cover
      --
      bmc:
      mode bmc
      --
      induction:
      mode prove
      --

      [engines]
      smtbmc z3

      [script]
      read -formal {source_path.name}
      prep -top {test}

      [files]
      {source_path}
    """
    for line in code.strip().splitlines():
      script.write(line.strip() + "\n")
  # print(script_path)

  # Run SymbiYosys.
  log_path = tmpDirPath / "sby.log"
  with open(log_path, "wb") as log:
    result = subprocess.call(
        ["sby", "-f", script_path],
        stdout=log,
        stderr=log,
    )

  if result == 0:
    print(" " + colored("passed", "green"))
  else:
    print(" " + colored("FAILED", "red", attrs=["bold"]))
    global num_failed
    num_failed += 1


if args.test is not None:
  run_test(args.test)
else:
  tests_json = subprocess.check_output(["circt-test", sys.argv[1], "--json"],
                                       universal_newlines=True)
  tests = json.loads(tests_json)
  tests = [x for x in tests if "name" in x and x.get("kind") == "formal"]
  if len(tests) == 0:
    print(f"no tests in `{args.input}`")
    sys.exit(0)
  print(f"running {len(tests)} tests in `{args.input}`")
  for test in tests:
    run_test(test["name"])

if num_failed > 0:
  sys.exit(1)
