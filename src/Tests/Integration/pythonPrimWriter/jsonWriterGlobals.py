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

# Simple script to expose a globals variable across primWriterFromJson.py
# and prim_writer_test.py. 
def initialize(): 
    # Variable used to make sure that the Write() method of the prim writer
    #implemented in primWriterFromJson is eventually called into.
    global jsonPrimWriter_write_called
    jsonPrimWriter_write_called = False