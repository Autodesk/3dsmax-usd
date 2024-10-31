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

-- This script is used to convert a spline object to CPP code.
-- This is used to create the viewport icon for the USDStageObject.
spl = selection[1]
for s = 1 to (numsplines spl) do
(
	format "\n//Spline %\n" s
	format "spline = baseShape.NewSpline();\n"
	for k = 1 to (numknots spl s) do 
	(
		p = (getKnotPoint spl s k)
		format "p = Point3(%,%,0.0);\n" p[1] p[2]
		inVec = (getInVec spl s k)
		format "in = Point3(%,%,0.0);\n" inVec[1] inVec[2]
		outVec = (getOutVec spl s k)
		format "out = Point3(%,%,0.0);\n" outVec[1] outVec[2]
		format "k.SetKnot(p);\n"
		format "k.SetInVec(in);\n"
		format "k.SetOutVec(out);\n"
		format "spline->AddKnot(k);\n"
	)
	format "spline->SetClosed();\n"
)