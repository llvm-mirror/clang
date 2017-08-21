#!/usr/bin/env python
# This script is called by analyze-cc to perform memory profiling using
# valgrind massif. It is called instead of the original clang executable.
# If -### parameter is passed to it (printing clangs compilation commands)
# it does not invoke valgrind, only returns clangs output and changes to clang
# executable name in the output to this script, otherwise it calls clang with
# memory profiling.

import os
import subprocess
import sys
import shlex


def run_command(command, cwd=None):
    """ Run a given command and report the execution.

    :param command: array of tokens
    :param cwd: the working directory where the command will be executed
    :return: output of the command
    """
    def decode_when_needed(result):
        """ check_output returns bytes or string depend on python version """
        return result.decode('utf-8') if isinstance(result, bytes) else result

    try:
        directory = os.path.abspath(cwd) if cwd else os.getcwd()
        print('exec command %s in %s', command, directory)
        output = subprocess.check_output(command,
                                         cwd=directory,
                                         stderr=subprocess.STDOUT)
        return decode_when_needed(output)
    except subprocess.CalledProcessError as ex:
        ex.output = decode_when_needed(ex.output)
        print(ex.output)
        sys.exit(ex.returncode)


# input is a clang output string
# returns replaces clang cmd by this script
def replace_clang_command(clang_print):
    lines = clang_print.splitlines()
    clang_cmd = shlex.split(lines[-1])
    clang_cmd[0] = __file__
    for i in range(0, len(clang_cmd)):
        clang_cmd[i] = '"' + clang_cmd[i] + '"'
    lines[-1] = " ".join(clang_cmd)
    ret = "\n".join(lines)
    return ret


this_script = __file__
use_valgrind = True

for param in sys.argv[1:]:
    if param == "-###":
        use_valgrind = False

if use_valgrind is False:
    clang_command = [os.environ["ANALYZE_BUILD_CLANG_ORIG"]]
    clang_command.extend(sys.argv[1:])
    output = run_command(clang_command)
    ret = replace_clang_command(output)
    print ret
    sys.exit(0)
else:
    clang_command = [os.environ["VALGRIND_PATH"], "-q", "--time-unit=B",
                     "--tool=massif", "--massif-out-file=" +
                     os.path.join(
                         os.environ["MEMPROF_PATH"],
                         os.environ["MEMPROF_OUTFILE"] + ".%p.massif"),
                     os.environ["ANALYZE_BUILD_CLANG_ORIG"]]
    clang_command.extend(sys.argv[1:])
    output = run_command(clang_command)
    print output
    sys.exit(0)
