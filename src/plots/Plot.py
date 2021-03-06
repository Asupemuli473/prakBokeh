#!/usr/bin/python
import grpc

import nc_pb2
import nc_pb2_grpc


class Plot:
    HEIGHT = 150
    WIDTH = 300	

    def __init__(self, url, logger, renderer, xrMetaData):
        self.url = url
        self.logger = logger
        self.renderer = renderer
        self.xrMetaData = xrMetaData
        
        channel = grpc.insecure_channel('0.0.0.0:50051')
        self.stub = nc_pb2_grpc.NCServiceStub(channel)

        assert self.xrMetaData != None

    def buildDims(self):
        """
        Function that builds up free and non-free dimensions array, according to the aggDim and aggFn fields
        Returns:
            nothing, but writes self.freeDims and self.nonFreeDims
        """
        self.freeDims = []
        self.nonFreeDims = []

        for d in getattr(self.xrMetaData,self.variable).dims:
            # Skip aggregated dimensions only it a Aggregate-Function is specified
            if d == self.aggDim and self.aggFn != "None":
                # Skip aggregated dimensions
                continue

            # WORKAROUND because Holoview is not working with a kdim with name "height"
            # See issue https://github.com/pyviz/holoviews/issues/3448
            if d == "height":
                self.freeDims.append("hi")
                continue
            if d != "ncells" and (len(getattr(getattr(self.xrMetaData,self.variable),d))-1) > 0:
                self.freeDims.append(d)
            if d != "ncells" and (len(getattr(getattr(self.xrMetaData,self.variable),d))-1) == 0:
                self.nonFreeDims.append(d)

    def getRanges(self):
        """
        Function that returns the ranges of the free dims
        Returns:
            ranges
        """
        ranges = {}
        for d in self.freeDims:
            # WORKAROUND because Holoview is not working with a kdim with name "height"
            # See issue https://github.com/pyviz/holoviews/issues/3448
            if d != "hi":
                ranges[d] = (0,len(getattr(getattr(self.xrMetaData,self.variable),d))-1)
            else:
                ranges[d] = (0,len(getattr(getattr(self.xrMetaData,self.variable),"height"))-1)
        return ranges

    def buildSelectors(self, args):
        """
        This function is build the selectors dictionary
        Returns
            selectors: dictionary
        """
        selectors = {}
        idx = 0

        # Build the data selector for the free dimensions. For those are the DynamicMap shows sliders to the right of the graph
        for d in self.freeDims:
            # WORKAROUND because Holoview is not working with a kdim with name "height"
            # See issue https://github.com/pyviz/holoviews/issues/3448
            if d == "hi":
                selectors["height"] = args[idx]
            else:
                selectors[d] = args[idx]
            idx = idx +1

        # Also select non-free-dimensions. Those are dimensions that are of length 1
        for d in self.nonFreeDims:
            if d == "hi":
                selectors["height"] = 0
            else:
                selectors[d] = 0
            idx = idx +1

        return selectors
