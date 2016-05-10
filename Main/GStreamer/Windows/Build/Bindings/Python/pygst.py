# -*- Mode: Python; py-indent-offset: 4 -*-
# pygst - Python bindings for the GStreamer multimedia framework.
# Copyright (C) 1998-2002  James Henstridge
#           (C) 2005 Edward Hervey
#
#   pygst.py: pygst version selection code.
#
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Library General Public
# License as published by the Free Software Foundation; either
# version 2 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Library General Public License for more details.
#
# You should have received a copy of the GNU Library General Public
# License along with this library; if not, write to the
# Free Software Foundation, Inc., 59 Temple Place - Suite 330,
# Boston, MA 02111-1307, USA.
#
# This allows parallel installation of gst-python
#
# In order to have backward compatibility

import sys
import os

__all__ = ['require']

_our_dir = os.path.dirname(os.path.abspath(__file__))
_pygst_dir = os.path.join(_our_dir, 'gst-0.10')

_pygst_version = '0.10'

_pygst_required_version = None

class RequiredVersionError(ValueError, AssertionError):
    # AssertionError is subclassed for compatibility reasons.
    pass


def require(version):
    global _pygst_required_version

    if _pygst_required_version != None:
        if _pygst_required_version != version:
            raise RequiredVersionError, "a different version of gst was already required"
        else:
            return

    if sys.modules.has_key('gst'):
        raise RequiredVersionError, "pygst.require() must be called before importing gst"

    if version != _pygst_version:
        raise RequiredVersionError, "Only version '%s' is available" % _pygst_version

    # move the pygst path to the front
    while _pygst_dir in sys.path:
        sys.path.remove(_pygst_dir)
    sys.path.insert(0, _pygst_dir)
    
    _pygst_required_version = version
