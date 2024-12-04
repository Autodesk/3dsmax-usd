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
import os
import argparse
import subprocess
import tempfile

from os import path


class TestDetails:

    def __init__(self):
        self.executable = ""    # Location of 3dmax.exe to run.
        self.log_file = ""      # Location to save test_runner.py log.
        self.error_file = ""    # File location to save test names that generated errors.
        self.tests = ""         # Test names that user entered as argument on the command line.
        self.env_var_tests = "" # Path of tests to run. (Comma separated)

def parse_user_args(parsed_values):
    parser = argparse.ArgumentParser(
        description='Script to run USD test integration.'
    )
    required_args = parser.add_argument_group('required arguments')
    required_args.add_argument(
        '-m', '--maxpath',
        dest='maxpath',
        help='Location of 3dsMax.exe to use. '
             'Ex: --maxpath "C:\\Program Files\\Autodesk\\3ds Max 2022\\3dsmax.exe"',
        required=True
    )
    parser.add_argument(
        '-l', '--logpath',
        dest='logpath',
        help='Location to save log file. '
             'Ex: --logpath "C:\\Temp\\TestResult.txt"',
        required=False
    )
    parser.add_argument(
        '-t', '--tests',
        dest='tests',
        help='List of test to run. If not specified, will run all available tests. '
             'Ex: --tests test1.ms,test2.ms ...',
        required=False
    )

    try:
        args = parser.parse_args()
    except:
        os._exit(-1)

    parsed_values.executable = args.maxpath

    if args.logpath:
        parsed_values.log_file = args.logpath
    else:
        parsed_values.log_file = ""

    if args.tests:
        parsed_values.env_var_tests = ""
        parsed_values.tests = args.tests
    else:
        parsed_values.env_var_tests = "ALL"
        parsed_values.tests = "ALL"


def check_and_store_valid_tests(test_details):
    error_details = list()
    integration_test_dir = os.path.join(os.path.dirname(__file__), "Integration")
    tests = test_details.tests.split(',')
    for test in tests:
        current_test_path = os.path.join(integration_test_dir, test)
        if not path.exists(current_test_path):
            error_details.append(test)
        else:
            test_details.env_var_tests += current_test_path + ','

    # Remove last ',' from tests
    test_details.env_var_tests = test_details.env_var_tests[:-1]

    return error_details

def main():
    test_run = TestDetails()
    test_run.error_file = tempfile.NamedTemporaryFile().name

    # Parse command line argument
    parse_user_args(test_run)

    # Check if 3dsMax path is valid
    if not path.exists(test_run.executable):
        print("3dsMax.exe not found")
        os._exit(-1)

    # Check if all tests specified exist and save tests in environment variable
    if test_run.tests != "ALL":
        file_check = check_and_store_valid_tests(test_run)
        if file_check:
            print("Test not found:{0}".format(','.join(file_check)))
            os._exit(-1)

    # Pass test to run / log as environment variable
    os.environ['3DSMAX_USD_INTEGRATION'] = "1"
    os.environ['3DSMAX_USD_INTEGRATION_LOG'] = test_run.log_file
    os.environ['3DSMAX_USD_INTEGRATION_TESTS'] = test_run.env_var_tests
    os.environ['3DSMAX_USD_INTEGRATION_ERROR'] = test_run.error_file

    # Create command to launch 3dsMax with python script
    this_dir = os.path.dirname(__file__)
    runner_path = os.path.join(os.path.abspath(this_dir), "test_runner.py")
    if not path.exists(runner_path):
        print("test_runner.py not found in {0}. Exiting.".format(runner_path))
        os._exit(-1)

    test_runner = ("{0} -u PythonHost {1}").format(test_run.executable, runner_path)

    # Launch 3dsMax with script as argument
    subprocess.run(test_runner, shell=False)

    # Check if there are errors reported
    if path.exists(test_run.error_file):
        with open(test_run.error_file, 'r') as error_file:
            errors = error_file.read()

        os.remove(test_run.error_file)

        if errors == "PASS":
            print("\n\nSUCCESS: All tests ran successfully.\n\n")
            os._exit(0)
        else:
            print("\n\nERROR: The following tests were not run successfully:")
            errors_split = errors.split('\n')
            for err in errors_split:
                if err:
                    print("\t-{0}".format(err))
            print("\n")
            if test_run.log_file:
                print("\tPlease see details in: {0}\n".format(test_run.log_file))
            os._exit(-1)
    
    print("\n\nWARNING: Error report file not found, can't report test results status.\n\n")
    os._exit(-1)

# Run tests
main()