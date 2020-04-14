#!/usr/bin/python


import pandas as pd
import xarray as xr
import holoviews as hv
import numpy as np

import nc_pb2

from .Plot import Plot

class HeightProfilePlot(Plot):
    NLONS = 360

    def __init__(self, url, dom, logger, renderer, xrMetaData):
        super().__init__(url, logger, renderer, xrMetaData)
        self.dom = dom
    
    def getPlotObject(self, variable, title, aggDim="None", aggFn="None",cm="Magma", cSymmetric=False, cLogZ=False, cLevels=0, dataUpdate=True):
        """
        Function that builds up a plot object for Bokeh to display
        Returns:
            : a plot object
        """
        self.variable = variable
        self.title = title

        self.aggDim = aggDim
        self.aggFn = aggFn

        self.cm = cm

        # Implement maximum and minimum to use this part TODO
        #self.useFixColoring = useFixColoring
        #self.fixColoringMin = fixColoringMin
        #self.fixColoringMax = fixColoringMax

        self.cSymmetric = cSymmetric
        self.cLogZ = cLogZ

        self.cLevels = cLevels

        self.dataUpdate = dataUpdate

        # Builds up the free and non-free dimensions array
        self.buildDims()
        return self.buildDynamicMap()

    def buildDynamicMap(self):
        ranges = self.getRanges()

        totalgraphopts = {"height": self.HEIGHT, "width": self.WIDTH}

        if "hi" in self.freeDims:
            self.freeDims.remove("hi")
        if "lev" in self.freeDims:
            self.freeDims.remove("lev")
        if "alt" in self.freeDims:
            self.freeDims.remove("alt")

        if len(self.freeDims) > 0:
            self.logger.info("Show with DynamicMap")
            dm = hv.DynamicMap(self.buildHeightProfilePlot, kdims=self.freeDims).redim.range(**ranges)
            return self.renderer.get_widget(dm.opts(**totalgraphopts), 'widget')
        else:
            # This is needed as DynamicMap is not working with an empty kdims array.
            self.logger.info("Show without DynamicMap")
            return self.renderer.get_plot(self.buildHeightProfilePlot().opts(**totalgraphopts))

    def buildHeightProfilePlot(self, *args):
        """
        Function that builds up the HeightProfile-Graph
        Args:
            Take multiple arguments.
        Returns:
            The Curve-Graph object
        """

        self.logger.info("FreeDims: %s" % str(self.freeDims))
        selectors = self.buildSelectors(args)
        self.logger.info("Loading data")

        # has_height= hasattr(self.xrMetaData, "height")

        # # If there is no height dimension we take alt instead
        # if has_height == False:
        #     del selectors['alt']
                
        if self.dataUpdate == True:
            reponse = 0;
            if self.aggFn == "mean":
                response = self.stub.GetHeightProfile(nc_pb2.HeightProfileRequest(filename=self.url, variable=self.variable, time=selectors['time'], dom=self.dom, aggregateFunction=0))     
            elif self.aggFn == "sum":
                response = self.stub.GetHeightProfile(nc_pb2.HeightProfileRequest(filename=self.url, variable=self.variable, time=selectors['time'], dom=self.dom, aggregateFunction=1))
            self.dat = [response.result[i].data for i in range(0,len(response.result))]

        # TODO Apply unit
        #factor = 1
        #dat = dat * factor

        res = hv.Image((range(self.NLONS), range(len(response.result)), self.dat), datatype=['grid'], label=self.title).opts(xlabel="Longitude", ylabel="height",cmap=self.cm,symmetric=self.cSymmetric,logz=self.cLogZ,color_levels=self.cLevels,colorbar=True)
        
        return res
