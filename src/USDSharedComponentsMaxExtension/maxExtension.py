#
# Copyright 2024 Autodesk
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

import sys
from typing import Sequence
if sys.version_info.major == 3 and sys.version_info.minor == 11:
    try:
        import shiboken6  # type: ignore
    except ImportError:
        print("WARN: PySide6 is not installed. You can install pip and PySide6 with the scripts below:")
        print('"{}" -m ensurepip --upgrade --user'.format(sys.executable))
        print('"{}" -m pip install --user PySide6==6.5.3'.format(sys.executable))
        status = False
else:
    try:
        import shiboken2  # type: ignore
    except ImportError:
        print("WARN: PySide2 is not installed. You can install pip and PySide2 with the scripts below:")
        print('"{}" -m ensurepip --upgrade --user'.format(sys.executable))
        print('"{}" -m pip install --user PySide2==5.15.1'.format(sys.executable))
        status = False

from pxr import Tf, Usd
if hasattr(Tf, 'PreparePythonModule'):
    Tf.PreparePythonModule('_maxExtension')
else:
    from . import _maxExtension
    Tf.PrepareModule(_maxExtension, locals())
    del _maxExtension
del Tf

import ufe
from usdSharedComponents.common.persistentStorage import PersistentStorage
from usdSharedComponents.common.theme import Theme
from usdSharedComponents.common.host import Host

class MaxPersistentStorage(PersistentStorage):

    def __init__(self):
        pass

    def set(self, group: str, key: str, value: object):
        return SetToPersistentStorage(group, key, value)

    def get(self, group: str, key: str, default: object = None) -> object:
        return GetFromPersistentStorage(group, key, default)

PersistentStorage.injectInstance(MaxPersistentStorage())

class MaxTheme(Theme):

    def __init__(self):
        self._palette : Theme.Palette = None
        self._icons = {}
        self._colorTheme : str = None
        self._uiScaleFactor: float = None
        pass

    @property
    def colorTheme(self) -> str:
        if self._colorTheme is None:
            self._colorTheme = GetColorTheme()
        return self._colorTheme

    @property
    def palette(self) -> Theme.Palette:
        if self._palette is None:
            self._palette = Theme.Palette()
            # default palette is for the dark color theme, but as 3dsMax also
            # supports a light theme, we need to adjust the palette here
            if self.colorTheme == "light":
                try:
                    from PySide6.QtGui import QColor # type: ignore
                except ImportError:
                    from PySide2.QtGui import QColor # type: ignore
                self._palette.colorResizeBorderActive = QColor(0x87a6ce)
        return self._palette

    @property
    def uiScaleFactor(self) -> float:
        ### Returns the UI scale factor.
        if self._uiScaleFactor is None:
            self._uiScaleFactor = GetUIScaleFactor()
        return self._uiScaleFactor

    def icon(self, name: str):
        ### Returns the icon with the given name.
        if name in self._icons:
            return self._icons[name]

        try:
            from PySide6.QtGui import QIcon # type: ignore
        except ImportError:
            from PySide2.QtGui import QIcon # type: ignore

        # first we check if the icon is a special one
        mapping = {"add": "Plus", "delete": "Delete"}
        if name in mapping:
            result: QIcon = GetMaxMultiResIcon(mapping[name])
            if result.isNull():
                raise ValueError(f"Icon '{name}' not found")
            self._icons[name] = result
            return result

        # then we check if we have a SVG or PNG to load...
        result = self.themedIcon(name, self.colorTheme)

        if result.isNull():
            raise ValueError(f"Icon '{name}' not found")
        self._icons[name] = result

        return result

Theme.injectInstance(MaxTheme())

class MaxHost(Host):

    def __init__(self):
        pass

    @property
    def canPick(self) -> bool:
        return True
    
    @property
    def canDrop(self) -> bool:
        return False

    def pick(self, stage: Usd.Stage, *, dialogTitle:str="") -> Sequence[Usd.Prim]:
        import maxUsd
        result = maxUsd.PickItems(stage, dialogTitle=dialogTitle)
        if result is None:
            return []
        result = list(map(lambda x: maxUsd.GetUsdPrim(x.path()), result))
        return result

Host.injectInstance(MaxHost())

# UfeSelecion (contains the prim) and attributeName to be used to create the widget
def CreateCollectionWidget(ufeSelection, attributeName):

    if ufeSelection is None:
        return None

    import maxUsd
    from pxr import Usd

    # for now only single selection is supported for simplicity.
    collection: Usd.CollectionAPI = None
    firstPrim: Usd.Prim = None

    usdPrim = None
    for item in ufeSelection:
        currentPrim = maxUsd.GetUsdPrim(item.path())
        if currentPrim.HasAPI(Usd.CollectionAPI):
            usdPrim = currentPrim
            collection = Usd.CollectionAPI.Get(usdPrim, attributeName)
            break

    if collection is None:
        return None

    from usdSharedComponents.collection.widget import CollectionWidget

    widget = CollectionWidget(usdPrim, collection)
    return widget