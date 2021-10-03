# This Software is part of Simics. The rights to copy, distribute,
# modify, or otherwise make use of this Software may be licensed only
# pursuant to the terms of an applicable license agreement.
#
# Copyright 2010-2021 Intel Corporation

import sys
import os
import testparams
import glob

def tests(suite):
    imported_test_path = os.path.join("..", "..", "sample-risc", "test")
    pattern = os.path.join(imported_test_path, 's-*.py')
    tests = sorted(glob.glob(pattern))
    if not tests:
        suite.add_test("no-subtests", no_subtests)

    quoted_path = imported_test_path.replace("\\", "\\\\")

    for script in sorted(tests):
        test = script.split(os.sep)[-1]
        suite.add_simics_test(
            script, name = test.replace(".py", ""),
            extra_args = [
                '-e', 'cd "%s"' % quoted_path,
                '-e', f'@sys.path.append("{imported_test_path}")',
                '-e', '$sample_risc_class="sample-risc"',
                '-e', '$sample_risc_core_class="sample-risc-core"'])

