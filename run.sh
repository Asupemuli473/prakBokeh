#!/bin/bash
(cd cpp/src
./nc_server&)
export BOKEH_PY_LOG_LEVEL="debug"
bokeh serve . --show --dev **/*
