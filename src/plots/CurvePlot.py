#!/usr/bin/python


import pandas as pd
import xarray as xr
import holoviews as hv
import numpy as np

import nc_pb2

from .Plot import Plot

class CurvePlot(Plot):

    def __init__(self, url, heightDim, dom, logger, renderer, xrMetaData):
        super().__init__(url, logger, renderer, xrMetaData)
        self.heightDim = heightDim
        self.dom = dom

    def getPlotObject(self, variable, title, aggDim="None", aggFn="None", logX=False, logY=False,dataUpdate=True):
        """
        Function that builds up a plot object for Bokeh to display
        Returns:
            : a plot object
        """
        self.variable = variable
        self.title = title
        self.aggDim = aggDim
        self.aggFn = aggFn

        self.logX = logX
        self.logY = logY

        self.dataUpdate = dataUpdate

        # Builds up the free and non-free dimensions array
        self.buildDims()
        return self.buildDynamicMap()

    def buildDynamicMap(self):
        ranges = self.getRanges()

        totalgraphopts = {"height": self.HEIGHT, "width": self.WIDTH}
        dm = hv.DynamicMap(self.buildCurvePlot, kdims=self.freeDims).redim.range(**ranges)
        self.logger.info("Build into Dynamic Map")
        return self.renderer.get_widget(dm.opts(**totalgraphopts),'widgets')

    def buildCurvePlot(self, *args):
        """
        Function that builds up the Curve-Graph
        Args:
            Take multiple arguments. A value for every free dimension.
        Returns:
            The Curve-Graph object
        """
        selectors = self.buildSelectors(args)

        # This part is not needed as a TriMeshGraph is drawn instead
        #if self.aggFn == "mean" and self.aggDim != "lat":
        #    dat = getattr(self.xrMetaData, self.variable).isel(selectors)
        #    dat = dat.mean(aggDim)

        #if self.aggFn == "sum" and self.aggDim != "lat":
        #    dat = getattr(self.xrMetaData, self.variable).isel(selectors)
        #    dat = dat.sum(aggDim)

        if self.dataUpdate == True:
            self.logger.info("Loading data")

            if self.aggDim == "lat" and self.aggFn == "mean":
                self.dat = self.stub.GetAggValuesPerLon(nc_pb2.AggValuesPerLonRequest(filename=self.url, variable=self.variable, alt=int(selectors[self.heightDim]), time=int(selectors['time']), dom=self.dom, aggregateFunction=0)).data
            elif self.aggDim == "lat" and self.aggFn == "sum":
                self.dat = self.stub.GetAggValuesPerLon(nc_pb2.AggValuesPerLonRequest(filename=self.url, variable=self.variable, alt=int(selectors[self.heightDim]), time=int(selectors['time']),dom=self.dom, aggregateFunction=1)).data     
            self.logger.info("Loaded data")

        # TODO Apply unit
        #factor = 1
        #dat = dat * factor

        res = hv.Curve(self.dat, label=self.title).opts(xlabel="Longitude", ylabel=self.variable, logy=self.logY, logx=self.logX) # Todo agg function parameter

        return res
