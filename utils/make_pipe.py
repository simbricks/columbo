#
# Copyright 2022 Max Planck Institute for Software Systems, and
# National University of Singapore
#
# Permission is hereby granted, free of charge, to any person obtaining
# a copy of this software and associated documentation files (the
# "Software"), to deal in the Software without restriction, including
# without limitation the rights to use, copy, modify, merge, publish,
# distribute, sublicense, and/or sell copies of the Software, and to
# permit persons to whom the Software is furnished to do so, subject to
# the following conditions:
#
# The above copyright notice and this permission notice shall be
# included in all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
# EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
# MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
# IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
# CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
# TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
# SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
#

import argparse
import os
import fcntl


def create_named_pipe(names: list[str], path: str = ""):
    print("Start creation of named pipes")
    # page_size = os.sysconf("SC_PAGESIZE")
    for name in names:
        if path != "":
            name = os.path.join(path, name)
        print(f"Start Creation of named pipe with name {name}")
        try:
            os.mkfifo(name)
        except OSError as err:
            print(f"An error occurred while creating the named pipe: {err}")
            return
        except Exception as err:
            print(f"An exception occurred while creating the named pipe: {err}")
    print("Named pipes were successfully created")


def print_named_pipe_buffer_size(name: str):
    fd_w = os.open(name, os.O_RDWR)
    fd = os.open(name, os.O_RDONLY)
    if fd < 0 or fd_w < 0:
        raise Exception("could not open named pipe file to print the underlying size")
    buffer_size = fcntl.fcntl(fd, fcntl.F_GETPIPE_SZ)
    print(f"The pipe {name} has an underlying buffer size of: {buffer_size}")
    os.close(fd_w)
    os.close(fd)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Utility script to create a named pipe with a user defined size.")
    subparsers = parser.add_subparsers(help="commands", dest="commands")

    create_parser = subparsers.add_parser("create", help="create named pipes")
    create_parser.add_argument("pipe_names", nargs="+", type=str, help="The names of the named pipes to create")
    create_parser.add_argument("-p", "--path", type=str, help="Path to a folder in which the named pipe shall be "
                                                              "created")

    check_parser = subparsers.add_parser("check", help="check buffer size of a named pipe")
    check_parser.add_argument("pipe_to_check", type=str, help="Prints the underlying buffer size of a named pipe")

    args = parser.parse_args()

    if args.commands == "create":
        path = str(args.path) if args.path is not None else ""
        create_named_pipe(args.pipe_names, path)
    elif args.commands == "check":
        print_named_pipe_buffer_size(args.pipe_to_check)
    else:
        raise Exception("No command was given")
