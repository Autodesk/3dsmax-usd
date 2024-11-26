#
# Copyright 2023 Autodesk
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
import unittest
import os
import glob
import pymxs
try:
    import MaxPlus
except ImportError:
    pass
import sys

from os import path
mxs = pymxs.runtime

# Accumulates tests that failed
test_failed = list()

def get_test_run_type():
    """ Return a string
    Read environment variable '3DSMAX_USD_INTEGRATION_TESTS' to know which specific tests we should be running.
    Default if environment variable is not found. --> ALL
        (Will run all tests)
    """
    return os.environ.get('3DSMAX_USD_INTEGRATION_TESTS', 'ALL')


def get_test_log_file():
    """ Return a string
    Read environment variable '3DSMAX_USD_INTEGRATION_LOG' to know where to write result log.
    Default if environment variable is not found --> "" 
        (Will use 'os.path.join(integration_test_dir, max_version + '_test_log.txt')' as specified in __main__)
    """
    return os.environ.get('3DSMAX_USD_INTEGRATION_LOG','')

def get_error_file():
    """ Return a string
    Read environment variable '3DSMAX_USD_INTEGRATION_ERROR' to know where to write test that failed.
    If environment variable is not found, error report won't be created
    """
    return os.environ.get('3DSMAX_USD_INTEGRATION_ERROR','')

def get_test_run_from_external_script():
    """ Return a string
    Read environment variable '3DSMAX_USD_INTEGRATION' to know if we are running from test runner
    Default if environment variable is not found --> ""
        (Will run as inside 3dsMax)
    """
    return os.environ.get('3DSMAX_USD_INTEGRATION', '')

def CreateTestReport():
    out_file_path = get_error_file()
    if out_file_path:
        with open(out_file_path, "a+") as out_file:
            if(len(test_failed) == 0):
                out_file.write("PASS")
            else:
                for value in test_failed:
                    out_file.write(str(value))
                    out_file.write("\n")

def get_max_version():
    mxs_max_version = mxs.maxversion()
    max_version = str(mxs_max_version[7])
    return max_version


class MaxScriptTestResult(unittest.TextTestResult):
    def addFailure(self, test, err):
        test_failed.append(test)
        if isinstance(test, MaxScriptTestCase):
            for maxscript_assert in test.assert_failures:
                self.failures.append((test, maxscript_assert))
                self.stream.writeln("ERROR")
        else:
            super(MaxScriptTestResult, self).addFailure(test, err)

    def addError(self, test, err):
        test_failed.append(test)
        if isinstance(test, MaxScriptTestCase):
            for maxscript_exception in test.exception_failures:
                self.errors.append((test, maxscript_exception))
                self.stream.writeln("FAIL")
        else:
            super(MaxScriptTestResult, self).addError(test, err)


class MaxScriptTestCase(unittest.TestCase):
    def __init__(self, maxscript):
        super(MaxScriptTestCase, self).__init__()
        self.maxscript = maxscript
        self.assert_failures = []
        self.exception_failures = []

    def __str__(self):
        return "{0}".format(os.path.basename(self.maxscript))

    def id(self):
        return self.maxscript

    def shortDescription(self):
        return self.maxscript

    def setUp(self):
        mxs.AssertReporter.Clear()

    def runTest(self):
        # Set global variable before running the maxscript so that this python script manages 3dsMax asserts instead of `usd_test_utils.mxs`.
        # If this variable is not set, then we will not capture all asserts has they will be cleared by `usd_test_utils.mxs` script.
        script = 'global assert_managed_python = true \nfileIn "{0}"'.format(self.maxscript.replace("\\", "/"))
        try:
            ret = mxs.execute(script)
        except Exception as e:
            # Exception details are available here
            self.assert_failures.append(str(e))
            self.fail(str(e))
            return

        assert_failures = list(mxs.AssertReporter.GetAssertFailures())
        if assert_failures:
            self.assert_failures = assert_failures
            self.fail("\n".join(assert_failures))
        exception_failures = list(mxs.AssertReporter.GetExceptionFailures())
        if exception_failures:
            self.exception_failures = exception_failures
            self.fail("\n".join(exception_failures))

def mxs_suite(test_dir):
    suite = unittest.TestSuite()

    tests_to_run = get_test_run_type()
    if tests_to_run == "ALL":
        scripts = glob.glob(os.path.join(test_dir, "*.ms"))
    else:
        scripts = tests_to_run.split(',')

    tests = (MaxScriptTestCase(script) for script in scripts)
    suite.addTests(tests)
    return suite

def run_tests(test_dir, log_stream):
    test_runner = unittest.TextTestRunner(log_stream, resultclass=MaxScriptTestResult, verbosity=2)
    suite = mxs_suite(test_dir)
    res = test_runner.run(suite)
    if get_test_run_from_external_script():
        CreateTestReport()
        mxs.quitMax(exitCode=0, quiet=True)
    if any(res.errors):
        sys.exit(1)
    if any(res.failures):
        sys.exit(1)

def main():
    max_version = get_max_version()
    this_dir = os.path.dirname(__file__)
    integration_test_dir = os.path.join(this_dir, "Integration")
    log_file = get_test_log_file()
    if not log_file:
        log_file = os.path.join(integration_test_dir, max_version + '_test_log.txt')
    with open(log_file, "w") as log_stream:
        run_tests(integration_test_dir, log_stream)

if __name__ == "__main__":
    main()