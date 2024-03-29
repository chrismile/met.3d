Met.3D -- 3D visualization of atmospheric ensemble simulation data
==================================================================

*********************************************************************
Documentation is available online at

  http://met3d.wavestoweather.de   and   http://met3d.readthedocs.org/

Please refer to these websites for more details.
*********************************************************************

### Information regarding multi-var trajectory functionality:
- Please use the build-linux.sh script to compile the program with multi-var trajectory functionality.
- Currently, only Ubuntu 20.04 and Ubuntu 22.04 are supported by the script.
  To build the program on other operating systems, it is recommended to adapt the build script.

[![DOI](https://zenodo.org/badge/DOI/10.5281/zenodo.8082371.svg)](https://doi.org/10.5281/zenodo.8082371)

*********************************************************************

Met.3D is an open-source visualization tool for interactive, three-dimensional
visualization of numerical ensemble weather predictions and similar numerical
atmospheric model datasets. The tool is implemented in C++ and OpenGL-4 and
runs on standard commodity hardware. It has originally been designed for
weather forecasting during atmospheric research field campaigns, however, is
not restricted to this application. Besides being used as a visualization tool,
Met.3D is intended to serve as a framework to implement and evaluate new 3D and
ensemble visualization techniques for the atmospheric sciences.

The Met.3D reference publication has been published in "Geoscientific
Model Development" and is available online:

   Rautenhaus, M., Kern, M., Schäfler, A., and Westermann, R.:
   "Three-dimensional visualization of ensemble weather forecasts -- Part 1:
   The visualization tool Met.3D (version 1.0)", Geosci. Model Dev., 8,
   2329-2353, doi:10.5194/gmd-8-2329-2015, 2015.
   Available at: http://www.geosci-model-dev.net/8/2329/2015/gmd-8-2329-2015.html

Met.3D is developed at the Regional Computing Center of Universitaet Hamburg,
Hamburg, Germany, and at the Computer Graphics & Visualization Group, Technische
Universitaet Muenchen, Garching, Germany. We hope you find the tool useful for
your work, too. Please let us know about your experiences.


Features:
=========

Met.3D runs under Linux and Windows. Its only "special" requirement is an
OpenGL-4 capable graphics card. A standard consumer (i.e. gaming) model works
fine. Met.3D's current features include:

* Interactive 2D horizontal sections in a 3D context, including filled and line
  contours, pseudo-colour plots and wind barbs.
* Interactive 2D vertical sections in a 3D context along arbitrary waypoints,
  including filled and line contours.
* 3D isosurface volume renderer that supports multiple isosurfaces. Isosurfaces
  can be coloured according to an auxiliary variable.
* 3D direct volume rendering (DVR) with an interactive transfer function editor.
* Surface shadows and interactive vertical axes to improve spatial perception.
* Navigation for forecast initialisation and valid time and ensemble member.
* Interactive computation of ensemble statistical quantities (mean, standard
  deviation, probabilities, ...) of any ensemble data field.
* 3D trajectory rendering for trajectories computed internally or externally
  (the latter provided in NetCDF format).
* Computation of 3D streamlines and trajectories from wind fields, following
  computational schemes in the LAGRANTO model.
* On-the-fly gridding of ensemble trajectories into 3D probability volumes.
* Support for 3D model data that is regular in longitude and latitude in the
  horizontal. In the vertical, the following is supported: (a) levels of
  constant pressure, (b) sigma-hybrid-pressure-levels (ECMWF model), (c) any
  type of structured grid as long as a 3D field with pressure values is
  available (this can be used, e.g. for data on geometric height levels).
* Limited support for data on rotated grids (e.g., by the DWD COSMO model;
  data can be displayed but not overlaid with data defined on geographic
  lon/lat coordinates).
* Interactive ensemble Skew-T diagrams for analysis of vertical profiles.
* Data can be read from CF-compliant NetCDF files and from ECMWF GRIB files.
* A comprehensive session manager to save the current state of the program
  and to revert earlier visualization steps.
* Interactive visual analysis of probability volumes.
* Multithreaded data pipeline architecture.
* Modular architecture designed to allow the straightforward implementation
  of additional visualization/data processing/data analysis modules.


Documentation:
==============

An evolving USER GUIDE is available online at

                      http://met3d.readthedocs.org/

(and yes, any help with this is highly appreciated).

In addition to the reference publication, a second paper available online from
Geoscientific Model Development describes the specific application case of
forecasting warm conveyor belt situations with Met.3D:

   Rautenhaus, M., Grams, C. M., Schäfler, A., and Westermann, R.:
   "Three-dimensional visualization of ensemble weather forecasts --Part 2:
   Forecasting warm conveyor belt situations for aircraft-based field campaigns",
   Geosci. Model Dev., 8, 2355-2377, doi:10.5194/gmd-8-2355-2015, 2015.
   Available at: http://www.geosci-model-dev.net/8/2355/2015/gmd-8-2355-2015.html

In addition, Met.3D has been used in a number of scientific publications.
Please refer to https://met3d.wavestoweather.de/publications.html for a list.


Installation and configuration from source code:
================================================

Installation and configuration notes can be found online in the user
documentation at http://met3d.readthedocs.org/.


Binary distribution:
====================

We provide a Linux binary distribution of Met.3D, please see
https://met3d.wavestoweather.de/downloads.html

To run Met.3D from this distribution, simply unpack the archive and
run "met3d". Please let us know if you encounter any problems.
