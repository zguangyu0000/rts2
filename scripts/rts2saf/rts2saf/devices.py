#!/usr/bin/python
#
# (C) 2013, Markus Wildi
#
#   
#
#   This program is free software; you can redistribute it and/or modify
#   it under the terms of the GNU General Public License as published by
#   the Free Software Foundation; either version 2, or (at your option)
#   any later version.
#
#   This program is distributed in the hope that it will be useful,
#   but WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#   GNU General Public License for more details.
#
#   You should have received a copy of the GNU General Public License
#   along with this program; if not, write to the Free Software Foundation,
#   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
#
#   Or visit http://www.gnu.org/licenses/gpl.html.
#

__author__ = 'wildi.markus@bluewin.ch'

import sys
import re
import os

# ToDo read, write to real devices
class Filter(object):
    """Class for filter properties"""
    def __init__(self, debug=None, name=None, OffsetToEmptySlot=None, lowerLimit=None, upperLimit=None, stepSize=None, exposureFactor=1., focFoff=None):
        self.debug=debug
        self.name= name
        self.OffsetToEmptySlot= OffsetToEmptySlot# [tick]
        self.relativeLowerLimit= lowerLimit# [tick]
        self.relativeUpperLimit= upperLimit# [tick]
        self.exposureFactor   = exposureFactor 
        self.stepSize  = stepSize # [tick]
        self.focFoff=focFoff # range


# ToDo read, write to real devices
class FilterWheel(object):
    """Class for filter wheel properties"""
    def __init__(self, debug=None, name=None, ccdFilterOffsets=list(), filters=list(), logger=None):
        self.debug=debug
        self.name= name
        self.ccdFilterOffsets=ccdFilterOffsets # from CCD
        self.filters=filters # list of Filter
        self.logger=logger
        self.emptySlots=None # set at run time ToDo go away??

    def check(self, proxy=None):
        proxy.refresh()
        if not self.name in 'FAKE_FTW':
            try:
                name=proxy.getDevice(self.name)
            except:
                self.logger.error('FilterWheel: filter wheel device: {0} not present'.format(self.name))        
                return False

        return True

# ToDo read, write to real devices
class Focuser(object):
    """Class for focuser properties"""
    def __init__(self, debug=None, name=None, resolution=None, absLowerLimit=None, absUpperLimit=None, lowerLimit=None, upperLimit=None, stepSize=None, speed=None, temperatureCompensation=None, focFoff=None, focDef=None, logger=None):
        self.debug=debug
        self.name= name
        self.resolution=resolution 
        self.absLowerLimit=absLowerLimit
        self.absUpperLimit=absUpperLimit
        self.lowerLimit=lowerLimit
        self.upperLimit=upperLimit
        self.stepSize=stepSize
        self.speed=speed
        self.temperatureCompensation=temperatureCompensation
        self.focFoff=focFoff # will be set at run time
        self.focDef=focDef # will be set at run time
        self.focMn=None # will be set at run time
        self.focMx=None # will be set at run time
        self.logger=logger

    def check(self, proxy=None):

        proxy.refresh()
        try:
            name=proxy.getDevice(self.name)
        except:
            self.logger.error('check : focuser device: {0} not present'.format(self.name))        
            return False

        focMin=focMax=None
        try:
            self.focMn= proxy.getDevice(self.name)['foc_min'][1]
            self.focMx= proxy.getDevice(self.name)['foc_max'][1]
        except Exception, e:
            self.logger.warn('check:  {0} has no foc_min or foc_max properties, using absulute limits'.format(self.name))
            self.focMn=self.absLowerLimit
            self.focMx=self.absUpperLimit
        return True

    def writeFocDef(self, proxy=None, focDef=None):
        if proxy!=None and focDef!=None:
            proxy.setValue(self.name,'FOC_DEF', focDef)
            # there are slow devices, e.g. dummy focuser
            while True:
                proxy.refresh()
                df = float(proxy.getSingleValue(self.name,'FOC_DEF'))
                if abs(df-focDef)< self.resolution:
                    self.focDef=int(df)
                    break
        else:
            self.logger.error('writeFocDef: {0} specify JSON proxy and FOC_DEF'.format(self.name))


# ToDo read, write to real devices
class CCD(object):
    """Class for CCD properties"""
    def __init__(self, debug=None, name=None, ftws=None, binning=None, windowOffsetX=None, windowOffsetY=None, windowHeight=None, windowWidth=None, pixelSize=None, baseExposure=None, logger=None):
        
        self.debug=debug
        self.name= name
        self.ftws=ftws
        self.binning=binning
        self.windowOffsetX=windowOffsetX
        self.windowOffsetY=windowOffsetY
        self.windowHeight=windowHeight
        self.windowWidth=windowWidth
        self.pixelSize= pixelSize
        self.baseExposure= baseExposure
        self.logger=logger
        self.ftOffsets=None

    def check(self, proxy=None):
        proxy.refresh()
        try:
            proxy.getDevice(self.name)
        except:
            self.logger.error('CCD: camera device: {0} not present'.format(self.name))        
            return False

        # There is no real filter wheel
        # TODO
        for ftw in self.ftws: 
            if ftw.name in 'FAKE_FTW':
                if self.debug: self.logger.debug('CCD: using FAKE_FTW')        
                #  OffsetToEmptySlot set in config.py
                return True
        # ToDo check al of them
        # check presence of a filter wheel
        try:
            ftwn=proxy.getValue(self.name, 'wheel')
        except Exception, e:
            self.logger.error('CCD: no filter wheel present')
            return False

        return True

