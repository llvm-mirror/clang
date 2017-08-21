#!/usr/bin/env python

import argparse
import io
import json
import multiprocessing
import os
import re
import shutil
import signal
import subprocess
import string
import sys
import threading
import time
import uuid
import sys

reload(sys)
sys.setdefaultencoding('utf8')

sys.path.append(os.path.join(os.path.dirname(__file__),
                             '..', '..', 'utils', 'analyzer'))
try:
    import MergeCoverage
except:
    raise

threading_factor = int(multiprocessing.cpu_count() * 1.0)
# timeout = 86400
analyser_output_formats = ['plist-multi-file', 'plist', 'plist-html',
                           'html', 'text']
analyser_output_format = analyser_output_formats[0]
gcov_outdir = 'gcov'
gcov_tmpdir = gcov_outdir + '_tmp'

parser = argparse.ArgumentParser(
            description='Executes 2nd pass of XTU analysis')
parser.add_argument('-b', required=True, metavar='buildlog.json',
                    dest='buildlog',
                    help='Use a JSON Compilation Database')
parser.add_argument('-p', metavar='preanalyze-dir', dest='xtuindir',
                    help='Use directory for reading preanalyzation data '
                         '(default=".xtu")',
                    default='.xtu')
parser.add_argument('-o', metavar='output-dir', dest='xtuoutdir',
                    help='Use directory for output analyzation results '
                         '(default=".xtu-out")',
                    default='.xtu-out')
parser.add_argument('-e', metavar='enabled-checker', nargs='+',
                    dest='enabled_checkers',
                    help='List all enabled checkers')
parser.add_argument('-d', metavar='disabled-checker', nargs='+',
                    dest='disabled_checkers',
                    help='List all disabled checkers')
parser.add_argument('-j', metavar='threads', dest='threads',
                    help='Number of threads used (default=' +
                    str(threading_factor) + ')',
                    default=threading_factor)
parser.add_argument('-v', dest='verbose', action='store_true',
                    help='Verbose output of every command executed')
parser.add_argument('--xtu-reparse', dest='reparse', action='store_true',
                    help='Use on-demand reparsing of external TUs (and do not dump ASTs).')
parser.add_argument('--clang-path', metavar='clang-path', dest='clang_path',
                    help='Set path of clang binaries to be used (default '
                         'taken from CLANG_PATH environment variable)',
                    default=os.environ.get('CLANG_PATH'))
parser.add_argument('--analyze-cc-path', metavar='analyze-cc-path',
                    dest='analyze_path',
                    help='Set path of analyze-cc to be used '
                         '(default is taken from CLANG_ANALYZE_CC_PATH '
                         'environment variable)',
                    default=os.environ.get('CLANG_ANALYZE_CC_PATH'))
parser.add_argument('--output-format', metavar='format',
                    choices=analyser_output_formats,
                    default=analyser_output_format,
                    help='Format for analysis reports '
                         '(one of %s; default is "%s").' %
                    (', '.join(analyser_output_formats),
                     analyser_output_format))
# parser.add_argument('--timeout', metavar='N',
#                     help='Timeout for analysis in seconds (default: %d)' %
#                     timeout, default=timeout)
parser.add_argument('--no-xtu', dest='no_xtu', action='store_true',
                    help='Do not use XTU at all, '
                         'only do normal static analysis')
parser.add_argument('--record-coverage', dest='record_coverage',
                    action='store_true',
                    help='Generate coverage information during analysis')
parser.add_argument('--record-memory-profile', dest='record_memprof',
                    action='store_true',
                    help='Generate Valgrind Massif memory profile information '
                         'during analysis (into xtuoutdir/memprof)')
parser.add_argument('--log-passed-build', metavar='passed-buildlog.json',
                    dest='passed_buildlog',
                    help='Write new buildlog JSON of files passing analysis')
mainargs = parser.parse_args()

concurrent_threads = 0
concurrent_thread_times = [0.0]
concurrent_thread_last_clock = time.time()


def executable_exists(path, exe_name):
    abs_exe_path = os.path.abspath(os.path.join(path, exe_name))
    return os.path.isfile(abs_exe_path) and os.access(abs_exe_path, os.X_OK)


def find_executable_on_arg_path(path, exe_name):
    exists = executable_exists(path, exe_name)
    return path if exists else None


def find_executable_on_env_path(exe_name):
    paths = os.environ['PATH'].split(os.pathsep)
    return next(
        (path for path in paths if executable_exists(path, exe_name)), None)


def check_executable_available(exe_name, arg_path):
    found_path = None
    if arg_path is not None:
        found_path = find_executable_on_arg_path(arg_path, exe_name)
    else:
        found_path = find_executable_on_env_path(exe_name)

    if found_path is None:
        print 'Executable "{}" not found on PATH provided via {}!'.format(
            exe_name,
            'argument' if arg_path is not None else 'environment')
        sys.exit(1)
    elif mainargs.verbose:
        print 'XTU uses {} dir: {}, taken from {}.'.format(
            exe_name,
            found_path,
            'argument' if arg_path is not None else 'environment')
    return found_path


clang_path = check_executable_available('clang', mainargs.clang_path)
analyze_path = check_executable_available('analyze-cc', mainargs.analyze_path)
if mainargs.record_memprof:
    valgrind_path = check_executable_available('valgrind', "/usr/bin")

analyzer_params = []
if mainargs.enabled_checkers:
    analyzer_params += ['-analyzer-checker', mainargs.enabled_checkers]
if mainargs.disabled_checkers:
    analyzer_params += ['-analyzer-disable-checker', mainargs.disable_checkers]
if mainargs.reparse:
    analyzer_params += ['-analyzer-config',
                        'xtu-reparse='+os.path.abspath(mainargs.buildlog)]
if not mainargs.no_xtu:
    analyzer_params += ['-analyzer-config',
                        'xtu-dir=' + os.path.abspath(mainargs.xtuindir)]
if mainargs.record_coverage:
    gcov_tmppath = os.path.abspath(os.path.join(mainargs.xtuoutdir,
                                                gcov_tmpdir))
    gcov_finalpath = os.path.abspath(os.path.join(mainargs.xtuoutdir,
                                                  gcov_outdir))
    shutil.rmtree(gcov_tmppath, True)
if mainargs.record_memprof:
    # memprof_analyze.sh calls clang with valgrind profiling.
    # We need to wrap it in a script so scan-build/analyze-cc
    # can call it instead of clang.
    memprof_path = os.path.abspath(os.path.join(mainargs.xtuoutdir,
                                                "memprof"))
    memprof_command = os.path.join(os.path.abspath(os.path.dirname(__file__)),
                                   'lib', 'memprof_analyze.py')

analyzer_params += ['-analyzer-stats']
passthru_analyzer_params = []
for param in analyzer_params:
    passthru_analyzer_params += ['-Xanalyzer']
    passthru_analyzer_params += [param]
passthru_analyzer_params += ['--analyzer-output ' + mainargs.output_format]

analyzer_env = os.environ.copy()
analyzer_env['ANALYZE_BUILD_CLANG'] = os.path.join(clang_path, 'clang')
analyzer_env['ANALYZE_BUILD_REPORT_DIR'] = os.path.abspath(mainargs.xtuoutdir)
analyzer_env['ANALYZE_BUILD_PARAMETERS'] = ' '.join(passthru_analyzer_params)
analyzer_env['ANALYZE_BUILD_REPORT_FORMAT'] = mainargs.output_format
analyzer_env['ANALYZE_BUILD_REPORT_FAILURES'] = 'yes'

buildlog_file = open(mainargs.buildlog, 'r')
buildlog = json.load(buildlog_file)
buildlog_file.close()

src_pattern = re.compile('.*\.(C|c|cc|cpp|cxx|ii|m|mm)$', re.IGNORECASE)
dircmd_separator = ': '
dircmd_2_orders = {}
src_build_steps = 0
all_build_steps = 0
passed_buildlog = []
dircmd_2_original_orders = {}
for step in buildlog:
    if src_pattern.match(step['file']):
        uid = step['directory'] + dircmd_separator + step['command']
        if uid not in dircmd_2_orders:
            dircmd_2_orders[uid] = [src_build_steps]
            dircmd_2_original_orders[uid] = [all_build_steps]
        else:
            dircmd_2_orders[uid].append(src_build_steps)
            dircmd_2_original_orders[uid].append(all_build_steps)
        src_build_steps += 1
    all_build_steps += 1


def get_compiler_and_arguments(cmd):
    had_command = False
    args = []
    for arg in cmd.split():
        if had_command:
            args.append(arg)
        if not had_command and arg.find('=') == -1:
            had_command = True
            compiler = arg
    return compiler, args


def analyze(directory, command):
    compiler, args = get_compiler_and_arguments(command)

    last_src = None
    for cmdpart in command.split():
        if src_pattern.match(cmdpart):
            last_src = cmdpart
    tu_name = ''
    if last_src:
        tu_name += last_src.split(os.sep)[-1]

    print 'Currently analyzing "{}"...'.format(tu_name)

    tu_name += '_' + str(uuid.uuid4())

    cmdenv = analyzer_env.copy()
    #cmdenv['ANALYZE_BUILD_CC'] = compiler
    #cmdenv['ANALYZE_BUILD_CXX'] = compiler
    cmdenv['INTERCEPT_BUILD'] = json.dumps({
        'verbose': 1,
        'cc': [compiler],
        'cxx': [compiler]
        })    
    if mainargs.record_coverage:
        cmdenv['ANALYZE_BUILD_PARAMETERS'] += \
            ' -Xanalyzer -analyzer-config -Xanalyzer record-coverage=' + \
            os.path.join(gcov_tmppath, tu_name)
    analyze_cmd = os.path.join(analyze_path, 'analyze-cc') + \
        ' ' + string.join(args, ' ')
    if mainargs.record_memprof:
        cmdenv['MEMPROF_PATH'] = memprof_path
        cmdenv['ANALYZE_BUILD_CLANG_ORIG'] = \
            analyzer_env['ANALYZE_BUILD_CLANG']
        cmdenv['ANALYZE_BUILD_CLANG'] = memprof_command
        cmdenv['VALGRIND_PATH'] = os.path.join(valgrind_path, "valgrind")
        cmdenv['MEMPROF_OUTFILE'] = tu_name

    if mainargs.verbose:
        print analyze_cmd
        cmdenv['ANALYZE_BUILD_VERBOSE'] = 'DEBUG'

    # Buffer output of subprocess and dump it out at the end, so that
    # the subprocess doesn't continue to write output after the user
    # sends SIGTERM
    runOK = True
    out = '******* Error running command'
    try:
        po = subprocess.Popen(analyze_cmd, shell=True,
                              stderr=subprocess.STDOUT,
                              stdout=subprocess.PIPE,
                              universal_newlines=True,
                              cwd=directory,
                              env=cmdenv)
        out, _ = po.communicate()
        runOK = not po.returncode
    except OSError:
        runOK = False
    if mainargs.verbose:
        sys.stdout.write(out)
    if not runOK:
        prefix = os.path.join(os.path.abspath(mainargs.xtuoutdir), "fails")
    else:
        prefix = os.path.join(os.path.abspath(mainargs.xtuoutdir), "passes")
    with open(os.path.join(prefix, "%s.out" % tu_name), "w") as f:
        f.write("%s\n%s" % (analyze_cmd, out))

    return runOK


def analyze_work():
    global concurrent_threads
    global concurrent_thread_times
    global concurrent_thread_last_clock
    global dircmd_2_orders
    global buildlog
    global passed_buildlog
    global dircmd_2_original_orders
    global num_passes
    global num_fails
    while len(dircmd_2_orders) > 0:
        found_dircmd_orders = None
        found_dircmd = None
        for dircmd_orders in dircmd_2_orders.items():
            dircmd = dircmd_orders[0].split(dircmd_separator, 2)
            assert len(dircmd) == 2 and len(dircmd[0]) > 0 and \
                len(dircmd[1]) > 0
            found_dircmd_orders = dircmd_orders
            found_dircmd = dircmd
            break
        del dircmd_2_orders[found_dircmd_orders[0]]

        concurrent_thread_current_clock = time.time()
        concurrent_thread_times[concurrent_threads] += \
            concurrent_thread_current_clock - concurrent_thread_last_clock
        concurrent_thread_last_clock = concurrent_thread_current_clock
        concurrent_threads += 1
        if len(concurrent_thread_times) == concurrent_threads:
            concurrent_thread_times.append(0.0)

        result = analyze(found_dircmd[0], found_dircmd[1])
        if (result):
            num_passes += 1
            for order in dircmd_2_original_orders[found_dircmd_orders[0]]:
                passed_buildlog.append(buildlog[order])
        else:
            num_fails += 1

        concurrent_thread_current_clock = time.time()
        concurrent_thread_times[concurrent_threads] += \
            concurrent_thread_current_clock - concurrent_thread_last_clock
        concurrent_thread_last_clock = concurrent_thread_current_clock
        concurrent_threads -= 1
        assert concurrent_threads >= 0

try:
    os.makedirs(os.path.abspath(mainargs.xtuoutdir))
    if mainargs.record_memprof:
        if not os.path.exists(memprof_path):
            os.makedirs(memprof_path)
except OSError:
    print 'Output directory %s already exists!' % \
        os.path.abspath(mainargs.xtuoutdir)
    sys.exit(1)

os.makedirs(os.path.join(os.path.abspath(mainargs.xtuoutdir), "passes"))
num_passes = 0
os.makedirs(os.path.join(os.path.abspath(mainargs.xtuoutdir), "fails"))
num_fails = 0

original_handler = signal.signal(signal.SIGINT, signal.SIG_IGN)
signal.signal(signal.SIGINT, original_handler)

analyze_workers = []
for i in range(int(mainargs.threads)):
    analyze_workers.append(threading.Thread(target=analyze_work))
for worker in analyze_workers:
    worker.start()
try:
    for worker in analyze_workers:
        worker.join(9999999999)
except KeyboardInterrupt:
    exit(1)

os.system('rm -vf ' + os.path.join(os.path.abspath(mainargs.xtuindir),
                                   'visitedFunc.txt'))
try:
    os.removedirs(os.path.abspath(mainargs.xtuoutdir))
    print 'Removing directory %s because it contains no reports' % \
        os.path.abspath(mainargs.xtuoutdir)
except OSError:
    pass

if mainargs.record_coverage:
    MergeCoverage.merge(gcov_tmppath, gcov_finalpath)
    shutil.rmtree(gcov_tmppath, True)

assert concurrent_threads == 0
concurrent_thread_times[0] += time.time() - concurrent_thread_last_clock
sumtime = 0.0
for i in range(len(concurrent_thread_times)):
    sumtime += concurrent_thread_times[i]
print '--- Total running time: %.2fs' % sumtime
for i in range(len(concurrent_thread_times)):
    print '----- ' + \
        (('using %d processes' % i) if i != 0 else 'self time') + \
        ' for %.2fs (%.0f%%)' % (concurrent_thread_times[i],
                                 concurrent_thread_times[i] * 100.0 / sumtime)

print '--- Total files analyzed: {}'.format(num_fails + num_passes)
print '----- Files passed: {}'.format(num_passes)
print '----- Files failed: {}'.format(num_fails)

if mainargs.passed_buildlog is not None:
    passed_buildlog_file = open(mainargs.passed_buildlog, 'w')
    json.dump(passed_buildlog, passed_buildlog_file, indent=4)
    passed_buildlog_file.close()
    print 'Passed buildlog is written to: ' + mainargs.passed_buildlog
