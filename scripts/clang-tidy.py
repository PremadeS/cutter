#!/usr/bin/env python3
import argparse
import glob
import subprocess
import sys
import os

SCAN_DIR = "src"
SKIP_LIST = ["themes", "bindings", "fonts", "img", "translations", "build", "_autogen"]
EXTENSIONS = (".cpp", ".h", ".hpp")

def get_files():
    matched_files = []
    for root, dirs, files in os.walk(SCAN_DIR):
        dirs[:] = [d for d in dirs if d not in SKIP_LIST]
        
        for file in files:
            if file.endswith(EXTENSIONS):
                matched_files.append(os.path.join(root, file))
    return matched_files

def run_tidy(args):
    files = [args.file] if args.file else get_files()

    if not files:
        print("No files found to process.")
        return

    cmd = [args.run_clang_tidy, "-p", args.build_path]
    
    # Add header filter if a specific file is targeted
    # This prevents seeing errors from included headers/other files
    if args.file:
        cmd.append("-header-filter='^$'")   
    
    if args.fix:
        cmd.append("-fix")
    
    cmd.append("-quiet")
    cmd.extend(files)

    result = subprocess.run(cmd)
    
    sys.exit(result.returncode)

def main():
    parser = argparse.ArgumentParser(description="clang-tidy wrapper")
    
    parser.add_argument("-T", "--run-clang-tidy", 
                        default="run-clang-tidy",
                        help="Path of run-clang-tidy binary")
    
    parser.add_argument("-p", "--build-path", default="build", 
                        help="Path to the build directory")
    
    parser.add_argument("-f", "--file", help="Check a specific file only")
    parser.add_argument("-i", "--fix", action="store_true", help="Apply fixes automatically")

    args = parser.parse_args()
    run_tidy(args)

if __name__ == "__main__":
    main()
