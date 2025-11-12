from pymxs import runtime as mxs
try:
    from pxr import Ar
    import AdskAssetResolver  # type: ignore
except ImportError:
    # the module was not found
    # define a placeholder to support the case in a clean way
    AdskAssetResolver = None


def register_ar():
    if AdskAssetResolver:
        # fetch all project dirs (aka from pathConfig.getProjectSubDirectoryCount/getProjectSubDirectory)
        AdskAssetResolver.AssetResolverContextDataRegistry.RegisterContextData("3ds Max Project Tokens") \
            .AddStaticToken("project", mxs.pathConfig.getCurrentProjectFolder()) \
            .AddStaticToken("animations", mxs.getdir(mxs.Name('animations'))) \
            .AddStaticToken("archives", mxs.getdir(mxs.Name('archives'))) \
            .AddStaticToken("autoback", mxs.getdir(mxs.Name('autoback'))) \
            .AddStaticToken("proxies", mxs.getdir(mxs.Name('proxies'))) \
            .AddStaticToken("downloads", mxs.getdir(mxs.Name('downloads'))) \
            .AddStaticToken("export", mxs.getdir(mxs.Name('export'))) \
            .AddStaticToken("expression", mxs.getdir(mxs.Name('expression'))) \
            .AddStaticToken("fluidsimulations", mxs.getdir(mxs.Name('fluidsimulations'))) \
            .AddStaticToken("image", mxs.getdir(mxs.Name('image'))) \
            .AddStaticToken("import", mxs.getdir(mxs.Name('import'))) \
            .AddStaticToken("image", mxs.getdir(mxs.Name('image'))) \
            .AddStaticToken("matlib", mxs.getdir(mxs.Name('matlib'))) \
            .AddStaticToken("maxstart", mxs.getdir(mxs.Name('maxstart'))) \
            .AddStaticToken("photometric", mxs.getdir(mxs.Name('photometric'))) \
            .AddStaticToken("preview", mxs.getdir(mxs.Name('preview'))) \
            .AddStaticToken("renderassets", mxs.getdir(mxs.Name('renderassets'))) \
            .AddStaticToken("renderoutput", mxs.getdir(mxs.Name('renderoutput'))) \
            .AddStaticToken("renderpresets", mxs.getdir(mxs.Name('renderpresets'))) \
            .AddStaticToken("scene", mxs.getdir(mxs.Name('scene'))) \
            .AddStaticToken("sound", mxs.getdir(mxs.Name('sound'))) \
            .AddStaticToken("vpost", mxs.getdir(mxs.Name('vpost'))) \
            .AddStaticToken("image", mxs.getdir(mxs.Name('image'))) \
            .AddStaticToken("systemimage", mxs.getdir(mxs.Name('systemimage'))) \
            .AddStaticToken("presets", mxs.getdir(mxs.Name('presets')))

mxs.callbacks.addScript(mxs.Name('postProjectFolderChange'), register_ar, id=mxs.Name('3dsmax_register_ar'))
register_ar()
