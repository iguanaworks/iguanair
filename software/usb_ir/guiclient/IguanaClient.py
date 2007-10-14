#!/usr/bin/python

#***************************************************************************
#** IguanaClient.py ********************************************************
# **************************************************************************
# *
# * TODO: DESCRIBE AND DOCUMENT THIS FILE
# *
# * Copyright (C) 2007, IguanaWorks Incorporated (http://iguanaworks.net)
# * Author: Joseph Dunn <jdunn@iguanaworks.net>
# *
# * Distributed under the GPL version 2.
# * See LICENSE for license details.
# */


import os
import sys

import wx
import wx.xrc

import iguanaIR

resources = wx.xrc.XmlResource(os.path.join(sys.path[0], 'client.xrc'))

class IgFrame(wx.Frame):
    def __init__(self):
        self.PostCreate(wx.PreFrame())
        resources.LoadOnFrame(self, None, type(self).__name__)
        self.Show()

# fire off the application
app = wx.PySimpleApp()
frame = IgFrame()
#signal.signal(signal.SIGINT, unblockMe)
app.MainLoop()
