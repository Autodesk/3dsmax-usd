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

import sys
if sys.version_info.major == 3 and sys.version_info.minor == 11:
    try:
        import shiboken6
    except ImportError:
        print("WARN: PySide6 is not installed. You can install pip and PySide6 with the scripts below:")
        print('"{}" -m ensurepip --upgrade --user'.format(sys.executable))
        print('"{}" -m pip install --user PySide6==6.5.3'.format(sys.executable))
        status = False
else:
    try:
        import shiboken2
    except ImportError:
        print("WARN: PySide2 is not installed. You can install pip and PySide2 with the scripts below:")
        print('"{}" -m ensurepip --upgrade --user'.format(sys.executable))
        print('"{}" -m pip install --user PySide2==5.15.1'.format(sys.executable))
        status = False

from pxr import Tf
if hasattr(Tf, 'PreparePythonModule'):
    Tf.PreparePythonModule('_maxUsd')
else:
    from . import _maxUsd
    Tf.PrepareModule(_maxUsd, locals())
    del _maxUsd
del Tf

__version__ = "0.9.4"
__version_info__ = (0, 9, 4, "", "")

class AnimatedAttributeHelper:
    """Helper class to import prim attributes that may or not be animated"""
    def __init__(self, setter):
        """Construtor
        @type setter: method
        @param setter: the method to call to set the attribute
        (expects fct(value, usdTimeCode, maxFrame) -> bool)
        """
        self.setter = setter
    def __call__(self):
        """Self called method to return the setter method wrapped to handle an animated attribute"""
        def AssignAttributeValue(value, usdTimeCode, maxFrame, animated) -> bool:
            import pymxs
            if animated:
                with pymxs.attime(maxFrame):
                    return self.setter(value, usdTimeCode, maxFrame)
            else:
                return self.setter(value, usdTimeCode, maxFrame)
        return AssignAttributeValue
