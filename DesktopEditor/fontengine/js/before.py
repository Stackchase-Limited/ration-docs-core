#!/usr/bin/env python

import sys
import os
sys.path.append("../../../../build_tools/scripts")
import base

base.configure_common_apps()

# graphics/pro/js/before.py has no shebang and is not executable, so it
# cannot be run as a program; hand it to the interpreter by name.
base.cmd_in_dir("./../../graphics/pro/js", "python3", ["./before.py"])
