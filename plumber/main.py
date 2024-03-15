import argparse
from random import choices
import sys
import os
import shutil
import subprocess
# import configparser
from typing import Final
from gts.gts_parser import GTSParser
from gts.codegen import CodeGeneratorRV32, CodeGeneratorARMA64, \
        CodegenOffsetException
from gts.ast_state import ExpansionState
from utils.utils import format_str
from classification.measurement_utils import read_measurement_method


def init_configs():
    """ Parse config file """
    pass


def init_compile_args(subparser):
    """ Initialize compile subcommand """
    compile_parser = subparser.add_parser("compile", help="compile verilog rtl"
                                          " design into c++ using Verilator.")
    compile_parser.set_defaults(func=compiler)

    compile_parser.add_argument("-I", "--incdir", nargs="*", required=True,
                                help="header file directory")

    compile_parser.add_argument("src", nargs="*", help="source directory")

    compile_parser.add_argument("--prefix", default="top",
                                help="prefix of generated files")

    compile_parser.add_argument("-Mdir", dest="mdir", default="out",
                                help="output directory of verilated model")

    compile_parser.add_argument("-o", "--output", default="top",
                                help="name of the output binary")


def init_exec_args(subparser):
    """ Initialize exec subcommand """
    exec_parser = subparser.add_parser("exec", help="execute generated"
                                      " testcase on the given hardware")
    exec_parser.set_defaults(func=executor)

    exec_parser.add_argument("mode",
                             choices=["concrete","symbolic"], 
                             help="mode of the execution")

    exec_parser.add_argument("input",
                             help="input file for testing: in case of symbolic"
                             " execution, this should be the address of the"
                             " llvm bitcode, and in concrete execution, it"
                             " should be the path to the directory of the"
                             " generated testcase to execute")

    exec_parser.add_argument("-k", "--ktest", help="ktest file generated klee"
                             " that executes a specific path (it is ignored"
                             " in concrete execution mode)")


def init_codegen_args(subparser):
    """ Initialize codegen subcommand """
    gts_parser = subparser.add_parser("codegen", help="transform a generative"
                                      " testcase Specification (GTS) into"
                                      " assembly code")
    gts_parser.set_defaults(func=generator)

    gts_parser.add_argument("gts", type=str, help="string representation of a"
                            " Generative Testcase Specification")

    gts_parser.add_argument("-a", "--arch",
                            choices=["ARM64","RISCV32"], 
                            help="architecture of the hardware"
                            " to generate testcases for")

    gts_parser.add_argument("-b", "--binary", choices=['header', 'raw'],
                            default=False, help="generate binary equivalet of"
                            " the assembly instructions")

    gts_parser.add_argument("-d", "--deterministic", nargs="?",
                            const="state.json", metavar="STATE_JSON_FILE",
                            default=False, help="Keeps the mappings from"
                            " placeholders (such as 's1' for sets) to actual"
                            " addresses across experiments instead of"
                            " re-randomizing it. The mappings will be stored"
                            " in the specified json file and can later be"
                            " reused for other GTSes. If the filename is"
                            " omitted, it defaults to state.json.")

    gts_parser.add_argument("-o", "--outdir",
                            help="output directory to store the generated code"
                            " files in")


def init_argparse():
    """ Initialize argparse """
    parser = argparse.ArgumentParser(description="A framework to facilitate"
                                     " generating Leakage Templates,"
                                     " leveraging instruction and operand"
                                     " fuzzing and statistical analysis")

    parser.add_argument("-v", "--verbose", action="store_true",
                           default=False, help="Enables more detailed output")

    subparser = parser.add_subparsers(help="supported action subcommands")

    init_codegen_args(subparser)
    init_compile_args(subparser)
    init_exec_args(subparser)

    return parser


def generator(args):
    # parse GTS string and build AST
    parser = GTSParser()
    parser.input(args.gts)
    gts = parser.parse()

    # print AST
    if args.verbose:
        print("====== AST =====")
        print(gts.to_str())

    # expand GTS: resolve all operators until the GTS only consists of sets
    # of directives
    if args.arch == 'ARM64':
        if args.binary:
            raise argparse.ArgumentError(None,
                                         "Currently binary generation is not"
                                         " supported for ARM64")
        generator = CodeGeneratorARMA64()
    elif args.arch == 'RISCV32':
        generator = CodeGeneratorRV32() 

    else:
        raise argparse.ArgumentError(None,
                                     "Given architecture is not supported")

    expanded = gts.expand(ExpansionState(generator))

    # print expanded GTS
    if args.verbose:
        print("====== Expanded GTS: (precondition, main expression) =====")
        print(format_str(str(expanded)))

    # ============ TESTCASE INSTANTIATOR ===========

    # retry loop: sets, tags, etc. are chosen randomly during code
    # generation. If their placeholders are combined with an arithmetic
    # expression in the GTS, the resulting set/tag index may be out of
    # bounds or collide with another placeholder. In this case, retry a few
    # times and hope that we randomly choose values that fall within the
    # allowed ranges.
    retry: int = 3
    while retry > 0:
        try:
            codes = gts.codegen(generator, args.deterministic)
            retry = 0
        except CodegenOffsetException as ex:
            if retry > 1:
                print(f"Error during code generation: {ex}; retry = {retry}")
                generator.reset()
            else:
                print(f"Code generation failed. {ex} Check your arithmetic expressions.")
                sys.exit(1)
            retry -= 1

    # create outdir if it does not exist
    if args.outdir:
        if not os.path.exists(args.outdir):
            os.makedirs(args.outdir)
        with open(os.path.join(args.outdir, "gts.txt"), "w") as gts_file:
            gts_file.write(args.gts)

    if args.verbose or (not args.outdir):
        print("===== Code Generation =====")

    for i, (code_setup, code_main, code_binary, registers_json) in enumerate(codes):
        if args.outdir:
            codedir: str = os.path.join(args.outdir, f"{i:08d}")
            os.makedirs(codedir)
            with open(os.path.join(codedir,
                                   "asm_setup.h"), "w") as code_setup_file:
                code_setup_file.write(code_setup)
            with open(os.path.join(codedir, "asm.h"), "w") as code_main_file:
                code_main_file.write(code_main)

            if args.binary:
                bin_file = "binary.h" if args.binary == "header" else "binary.txt"
                with open(os.path.join(codedir, bin_file), "w") as code_binary_file:
                    if args.binary == "header":
                        code_binary_file.write("#include <cstdint>\n");
                        code_binary_file.write("#include <vector>\n\n");
                        code_binary_file.write("static std::vector<std::uint32_t> testcase {\n");
                        for instr in code_binary:
                            if instr == "\n":
                                code_binary_file.write("," + instr)
                            else:
                                code_binary_file.write(instr)
                        code_binary_file.write("};")
                    else:
                        code_binary_file.write(code_binary)

            with open(os.path.join(codedir, "registers.json"),
                      "w") as registers_json_file:
                registers_json_file.write(registers_json)

        if args.verbose or (not args.outdir):
            print("==== SETUP ====")
            print(code_setup)

            print("==== MAIN ====")
            print(code_main)

            if args.binary:
                print("==== BINARY ====")
                print(code_binary)

            print("==== REGISTERS ====")
            print(registers_json)


def executor(args):
    """ Execute the generated testcase """
    if not args.input:
        sys.exit(0)

    if args.mode == "concrete":
        execute_concrete(args.input)
    elif args.mode == "symbolic":
        execute_symbolic(args.input, args.ktest if args.ktest else None)

def execute_concrete(testdir):
    print("running executor")

    PATH_EXECUTOR_MAKEDIR: Final[str] = "../executor"
    PATH_EXECUTOR_MAKEFILE_CONFIG: Final[str] = os.path.join(
       PATH_EXECUTOR_MAKEDIR, "Makefile.config")
    PATH_EXECUTOR_CODEDIR: Final[str] = "../executor/code"
    PATH_EXECUTOR_LOGFILE: Final[str] = "../executor/uart.log"

    # Parse Makefile.config to find the measurement method
    measurement_method: str = read_measurement_method(
        PATH_EXECUTOR_MAKEFILE_CONFIG)

    # Run experiments, one after another
    for experiment_dir in sorted(os.listdir(testdir)):
        experiment_dir = os.path.join(testdir, experiment_dir)
        if not os.path.isdir(experiment_dir):
            continue
        print(f"running experiment {experiment_dir}...")

        # copy code files into executor directory
        shutil.copy(
            os.path.join(experiment_dir, "asm.h"),
            os.path.join(PATH_EXECUTOR_CODEDIR, "asm.h")
        )
        shutil.copy(
            os.path.join(experiment_dir, "asm_setup.h"),
            os.path.join(PATH_EXECUTOR_CODEDIR, "asm_setup.h")
        )

        # run the executor
        executor_process = subprocess.Popen(["make", "-C",
                                             PATH_EXECUTOR_MAKEDIR,
                                             "clean", "runlog"])
        executor_process.wait()
        if executor_process.returncode != 0:
            raise Exception("Executor process failed with returncode",
                            f"{executor_process.returncode}.")

        # copy measurement log (uart.log) into experiment folder
        path_measurement_logfile: str = os.path.join(experiment_dir,
                                                     f"uart.log")
        shutil.copy(PATH_EXECUTOR_LOGFILE, path_measurement_logfile)


def execute_symbolic(bitcode, ktest):
    klee_command = ["../../plumber-klee/build/bin/klee", "--search=priority", "--emit-all-errors",
                    "--posix-runtime", "--libcxx", "--libc=uclibc"]

    if ktest:
        klee_command.append(f"--replay-ktest-file={ktest}")

    klee_command.append(bitcode)

    proc = subprocess.Popen(klee_command)
    proc.wait()
    if proc.returncode != 0:
        raise Exception(f"Klee failed with returncode {proc.returncode}.")

def compiler(args):
    """ Compile the design using verilator """

    verilator_command = ["make", "-f", "verilate.mk", "CXX=wllvm++",
                         "LINK=wllvm++", 'CFG_CXXFLAGS_STD_NEWEST=""']

    if args.output:
        verilator_command.append(f"OUT_DIR={args.output}")

    if args.prefix:
        verilator_command.append(f"PREFIX={args.prefix}")

    if args.output:
        verilator_command.append(f"OUT_NAME={args.output}")

    if args.mdir:
        verilator_command.append(f"OUT_DIR={args.mdir}")

    inc_dirs="INC_DIR="
    for addr in args.incdir:
        inc_dirs = inc_dirs + " " + addr

    verilator_command.append(inc_dirs)

    src_objs = "SRC_OBJS="
    for file in args.src:
        src_objs = src_objs + " " + file

    verilator_command.append(src_objs)
    verilator_command.append("build")

    proc = subprocess.Popen(verilator_command)
    proc.wait()
    if proc.returncode != 0:
        raise Exception(f"Verilator failed with returncode {proc.returncode}.")


def main():
    args = init_argparse().parse_args()
    args.func(args)
    return 0


if __name__ == "__main__":
    main()
