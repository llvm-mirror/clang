#!/usr/bin/env python

import argparse
import io
import json
import glob
import multiprocessing
import os
import re
import signal
import subprocess
import string
import shlex
import shutil
import tempfile

threading_factor = int(multiprocessing.cpu_count() * 1.5)
timeout = 86400
EXTERNAL_FUNCTION_MAP_FILENAME = 'externalFnMap.txt'
TEMP_EXTERNAL_FNMAP_FOLDER = 'tmpExternalFnMaps'


parser = argparse.ArgumentParser(
    description='Executes 1st pass of XTU analysis')
parser.add_argument('-b', required=True, dest='buildlog',
                    metavar='build.json',
                    help='Use a JSON Compilation Database')
parser.add_argument('-p', metavar='preanalyze-dir', dest='xtuindir',
                    help='Use directory for generating preanalyzation data '
                         '(default=".xtu")',
                    default='.xtu')
parser.add_argument('-j', metavar='threads', dest='threads',
                    help='Number of threads used (default=' +
                    str(threading_factor) + ')',
                    default=threading_factor)
parser.add_argument('-v', dest='verbose', action='store_true',
                    help='Verbose output of every command executed')
parser.add_argument('--xtu-reparse', dest='reparse', action='store_true',
                    help='Use on-demand reparsing of external TUs (and do not dump ASTs).')
parser.add_argument('--clang-path', metavar='clang-path', dest='clang_path',
                    help='Set path of clang binaries to be used '
                         '(default taken from CLANG_PATH envvar)',
                    default=os.environ.get('CLANG_PATH'))
parser.add_argument('--timeout', metavar='N',
                    help='Timeout for build in seconds (default: %d)' %
                    timeout,
                    default=timeout)
mainargs = parser.parse_args()


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

buildlog_file = open(mainargs.buildlog, 'r')
buildlog = json.load(buildlog_file)
buildlog_file.close()

src_pattern = re.compile('.*\.(C|c|cc|cpp|cxx|ii|m|mm)$', re.IGNORECASE)
src_2_dir = {}
src_2_cmd = {}
src_order = []
cmd_2_src = {}
cmd_order = []
for step in buildlog:
    if src_pattern.match(step['file']):
        if step['file'] not in src_2_dir:
            src_2_dir[step['file']] = step['directory']
        if step['file'] not in src_2_cmd:
            src_2_cmd[step['file']] = step['command']
            src_order.append(step['file'])
        if step['command'] not in cmd_2_src:
            cmd_2_src[step['command']] = [step['file']]
            cmd_order.append(step['command'])
        else:
            cmd_2_src[step['command']].append(step['file'])


def clear_file(filename):
    try:
        os.remove(filename)
    except OSError:
        pass


def get_command_arguments(cmd):
    had_command = False
    args = []
    for arg in cmd.split():
        if had_command and not src_pattern.match(arg):
            args.append(arg)
        if not had_command and arg.find('=') == -1:
            had_command = True
    return args


def get_triple_arch(clang_path, clang_args,source):
    """Returns the architecture part of the target triple in a compilation command """
    arch = ""
    clang_cmd = []
    clang_cmd.append(os.path.join(clang_path, 'clang'))
    clang_cmd.append("-###")
    clang_cmd.extend(clang_args)
    clang_cmd.append(source)    
    clang_out = subprocess.check_output(clang_cmd, stderr=subprocess.STDOUT, shell=False)    
    clang_params=shlex.split(clang_out)
    i=0
    while i<len(clang_params) and clang_params[i]!="-triple":        
        i=i+1
    if i<(len(clang_params) - 1):
        arch=clang_params[i+1].split("-")[0]              
    return arch
    

def generate_ast(source):
    cmd = src_2_cmd[source]
    args = get_command_arguments(cmd)        
    arch=get_triple_arch(clang_path,args,source)    
    ast_path = os.path.abspath(os.path.join(mainargs.xtuindir,
                               os.path.join('/ast/' + arch,
                                            os.path.realpath(source)[1:] +
                                            '.ast')[1:]))
    try:
        os.makedirs(os.path.dirname(ast_path))
    except OSError:
        if os.path.isdir(os.path.dirname(ast_path)):
            pass
        else:
            raise
    dir_command = 'cd ' + src_2_dir[source]
    ast_command = os.path.join(clang_path, 'clang') + ' -emit-ast ' + \
        string.join(args, ' ') + ' -w ' + source + ' -o ' + ast_path
    if mainargs.verbose:
        print dir_command + " && " + ast_command
    subprocess.call(dir_command + " && " + ast_command, shell=True)


def map_functions(params):
    command, sources, directory, clang_path, ctuindir, reparse = params
    ctuindir = os.path.abspath(ctuindir)
    args = get_command_arguments(command)
    arch = get_triple_arch(clang_path, args, sources[0])
    funcmap_command = [os.path.join(clang_path, 'clang-func-mapping')]
    funcmap_command.extend(sources)
    funcmap_command.append('--')
    funcmap_command.extend(args)
    output = []
    os.chdir(directory);
    if mainargs.verbose:
        print funcmap_command
    fn_out = subprocess.check_output(funcmap_command)
    fn_list = fn_out.splitlines()
    for fn_txt in fn_list:
        dpos = fn_txt.find(" ")
        mangled_name = fn_txt[0:dpos]
        path = fn_txt[dpos + 1:]
        ast_path = path
        if not reparse:
            ast_path = os.path.join("ast", arch, path[1:] + ".ast")
        output.append(mangled_name + "@" + arch + " " + ast_path)
    extern_fns_map_folder = os.path.join(ctuindir,
                                         TEMP_EXTERNAL_FNMAP_FOLDER)
    if output:
        with tempfile.NamedTemporaryFile(mode='w',
                                         dir=extern_fns_map_folder,
                                         delete=False) as out_file:
            out_file.write("\n".join(output) + "\n")


def create_external_fn_maps(ctuindir):
    files = glob.glob(os.path.join(ctuindir, TEMP_EXTERNAL_FNMAP_FOLDER,
                                   '*'))
    extern_fns_map_file = os.path.join(ctuindir,
                                       EXTERNAL_FUNCTION_MAP_FILENAME)
    mangled_to_asts = {}
    for filename in files:
        with open(filename, 'rb') as in_file:
            for line in in_file:
                mangled_name, ast_file = line.strip().split(' ', 1)
                if mangled_name not in mangled_to_asts:
                    mangled_to_asts[mangled_name] = {ast_file}
                else:
                    mangled_to_asts[mangled_name].add(ast_file)
    with open(extern_fns_map_file, 'wb') as out_file:
        for mangled_name, ast_files in mangled_to_asts.iteritems():
            if len(ast_files) == 1:
                out_file.write('%s %s\n' % (mangled_name, ast_files.pop()))


if not os.path.exists(mainargs.xtuindir):
    os.makedirs(mainargs.xtuindir)
clear_file(os.path.join(mainargs.xtuindir, 'cfg.txt'))
clear_file(os.path.join(mainargs.xtuindir, 'definedFns.txt'))
clear_file(os.path.join(mainargs.xtuindir, 'externalFns.txt'))
clear_file(os.path.join(mainargs.xtuindir, 'externalFnMap.txt'))

original_handler = signal.signal(signal.SIGINT, signal.SIG_IGN)
if not mainargs.reparse:   #only generate AST dumps is reparse is off     
    ast_workers = multiprocessing.Pool(processes=int(mainargs.threads))
    signal.signal(signal.SIGINT, original_handler)
    try:
        res = ast_workers.map_async(generate_ast, src_order)
        # Block with timeout so that signals don't get ignored, python bug 8296
        res.get(mainargs.timeout)
    except KeyboardInterrupt:
        ast_workers.terminate()
        ast_workers.join()
        exit(1)
    else:
        ast_workers.close()
        ast_workers.join()


shutil.rmtree(os.path.join(mainargs.xtuindir, TEMP_EXTERNAL_FNMAP_FOLDER), ignore_errors=True)
os.mkdir(os.path.join(mainargs.xtuindir, TEMP_EXTERNAL_FNMAP_FOLDER))
original_handler = signal.signal(signal.SIGINT, signal.SIG_IGN)
funcmap_workers = multiprocessing.Pool(processes=int(mainargs.threads))
signal.signal(signal.SIGINT, original_handler)
try:
    res = funcmap_workers.map_async(map_functions,
                                    [(cmd, cmd_2_src[cmd], src_2_dir[cmd_2_src[cmd][0]],
                                      clang_path, mainargs.xtuindir, mainargs.reparse) for cmd in cmd_order])
    res.get(mainargs.timeout)
except KeyboardInterrupt:
    funcmap_workers.terminate()
    funcmap_workers.join()
    exit(1)
else:
    funcmap_workers.close()
    funcmap_workers.join()


# Generate externalFnMap.txt

create_external_fn_maps(mainargs.xtuindir)
shutil.rmtree(os.path.join(mainargs.xtuindir, TEMP_EXTERNAL_FNMAP_FOLDER), ignore_errors=True)

