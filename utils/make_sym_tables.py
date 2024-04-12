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
import subprocess


def create_symtable(original_file: str, target_file: str):
    command = f"objdump --syms {original_file} > {target_file}"
    ret = subprocess.run(command, shell=True, capture_output=True)
    if ret.returncode != 0:
        raise Exception(f"executing '{command}' yielded an error:\n{ret.stderr}")


def create_symtables(bin_target: dict[str, str]):
    for binary, target in bin_target.items():
        create_symtable(binary, target)


class ParseKVPair(argparse.Action):
    def __call__(self, parser, namespace, values, option_string=None):
        d = getattr(namespace, self.dest) or {}

        if values:
            for item in values:
                split_items = item.split("=", 1)
                key = split_items[
                    0
                ].strip()  # we remove blanks around keys, as is logical
                value = split_items[1]

                d[key] = value
        setattr(namespace, self.dest, d)


if __name__ == "__main__":
    print("START SYMTABLE CREATION")

    parser = argparse.ArgumentParser(description="Utility script to create symtable files from ELF binaries used for "
                                                 "symbol translation within columbo.")

    parser.add_argument("bintarpairs", metavar="KEY=VALUE", nargs="+",
                        help="Set a number of key-value pairs (original-elf-binary=target-symtable-file, do not put "
                             "spaces before or after the = sign)."
                             "If a value contains spaces, you should define it with double quotes: "
                             'foo="this is a sentence". Note that values are always treated as strings.',
                        action=ParseKVPair)

    args = parser.parse_args()

    create_symtables(args.bintarpairs)

    print("FINISHED SYMTABLE CREATION")
