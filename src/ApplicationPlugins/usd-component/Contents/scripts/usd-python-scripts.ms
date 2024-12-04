--
-- Copyright 2023 Autodesk
--
-- Licensed under the Apache License, Version 2.0 (the "License");
-- you may not use this file except in compliance with the License.
-- You may obtain a copy of the License at
--
--     http://www.apache.org/licenses/LICENSE-2.0
--
-- Unless required by applicable law or agreed to in writing, software
-- distributed under the License is distributed on an "AS IS" BASIS,
-- WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
-- See the License for the specific language governing permissions and
-- limitations under the License.
--

/*
 * Appends usd python scripts ptah to max's python path
*/
(

function insertScriptsToSysPath = 
(    
    local script_file = getSourceFileName()
    local script_path = substituteString (getFilenamePath(script_file)) "\\" "/"
    local pysys = python.import("sys")

    if pysys.path.__contains__(script_path) then (
        return false
    ) else (
        if doesFileExist script_path then (
            --favor prepend because we currently do not want users to override the path by mistake when setting PYTHONPATH
            pysys.path.insert 0 script_path
            pysys.path.insert 0 (script_path+"/materials")
        ) else (
            logsystem.logEntry ("Could not find usd python path at " + script_path) broadcast:true warning:true
        )
    )
)

insertScriptsToSysPath()
)