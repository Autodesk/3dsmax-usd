import maxUsd
from pymxs import runtime as rt
from pxr import UsdShade, Sdf
import usd_utils
import traceback
import os

class mtlXShaderWriter(maxUsd.ShaderWriter):
	def Write(self):
		try:
			stage = self.GetUsdStage()
			material = rt.GetAnimByHandle(self.GetMaterial())
			isMultiTarget = len(self.GetExportArgs().GetAllMaterialConversions()) > 1

			# Get what we need from the Scripted Material.
			# The curent material name, so we know which one to connect.
			curMatName = material.curMatName
			if curMatName == None or curMatName == "":
				rt.UsdExporter.Log(rt.Name("warn"), ("No valid MaterialX material found for " + material.name + ", conversion can't proceed."))
				return
			# The mtlx file path, so we can add it as a reference.
			mtlxFilePath = material.MaterialXFile
			# Make sure we have the absolute path, so we can make it relative to the exported file...
			if not os.path.isabs(mtlxFilePath):
				mtlxFilePath = rt.pathConfig.convertPathToAbsolute(mtlxFilePath)

			# Find the prim where we want to add the reference
			if isMultiTarget:
				refHolderPrim = UsdShade.NodeGraph.Get(stage, self.GetUsdPath().GetParentPath())
			else:
				refHolderPrim = UsdShade.Material.Get(stage, self.GetUsdPath().GetParentPath())
			ref = refHolderPrim.GetPrim().GetReferences()
			
			# In case the material is not exported to the main layer, the path on disk can be different.
			# This need to be taken in consideration before the reference to the MaterialX file.
			targetLayer = stage.GetEditTarget().GetLayer()
			outputFile = self.GetFilename()
			if not targetLayer.anonymous:
				outputFile = targetLayer.identifier
            	
			# Add the reference to the Mtlx file.
			if self.IsUSDZFile():
				rt.UsdExporter.Log(rt.Name("warn"), ("Conversion of MaterialX material : " + material.name + ", is limited for USDZ and will not yield correct results."))
				refPath = mtlxFilePath
			else:
				refPath = usd_utils.safe_relpath(mtlxFilePath, os.path.dirname(outputFile))
			refPath = refPath.replace('\\','/')

			# Set-up the current working directory as this can cause issues when adding the reference :
			# The stage currently live's in memory, and doesn't know where it will end up on disk.
			# So when using a relative path for the reference, it is unable to resolve the path, and so materialX are never added to the stage. 
			# By setting up the current working directory, to the target output file's directory, we help it resolve the reference.
			directory = os.path.dirname(outputFile)
			os.makedirs(directory, exist_ok=True)
			cwd = os.getcwd()
			os.chdir(directory)
			ref.AddReference(refPath, primPath="/MaterialX")
			os.chdir(cwd)

			# Add a pass-through connection between the prim holding the reference and the Material we are using inside of the Mtlx file.
			matXPath = self.GetUsdPath().GetParentPath().AppendPath("Materials").AppendPath(curMatName)
			matXPrim = UsdShade.Material.Get(stage, matXPath)
			surfOutput = matXPrim.GetSurfaceOutput("mtlx")
			if isMultiTarget:
				refHolderSurfOut = refHolderPrim.CreateOutput("mtlx:surface", Sdf.ValueTypeNames.Token)
			else:
				refHolderSurfOut = refHolderPrim.CreateSurfaceOutput("mtlx")
			refHolderSurfOut.ConnectToSource(surfOutput)

			# Here we want to handle the case of exporting to multiple targets.
			# Since this ShaderWriter doesn't really respect the API (no call to SetUsdPrim()),
			# we need to create the pass-through between the NodeGraph encapsulating the MaterialX material and the material prim.
			if isMultiTarget:
				globalMat = UsdShade.Material.Get(stage, refHolderPrim.GetPath().GetParentPath())
				surfaceOutput = globalMat.CreateSurfaceOutput("mtlx")
				surfaceOutput.ConnectToSource(refHolderSurfOut)

		except Exception as e:
			# Quite useful to debug errors in a Python callback
			print('Write() - Error: %s' % str(e))
			print(traceback.format_exc())
		
	@classmethod
	def CanExport(cls, exportArgs):
		if exportArgs.GetConvertMaterialsTo() == "MaterialX":
			return maxUsd.ShaderWriter.ContextSupport.Fallback
		return maxUsd.ShaderWriter.ContextSupport.Unsupported

# Register the writer.
maxUsd.ShaderWriter.Register(mtlXShaderWriter, "MaterialX Material")