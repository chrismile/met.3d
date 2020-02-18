# -*- coding: utf-8 -*-
"""
    wxlib.remote_config
    ~~~~~~~~~~~~~~~~~~~

    Collection of remote configuration details. 

    This file is part of Met.3D -- a research environment for the
    three-dimensional visual exploration of numerical ensemble weather
    prediction data.

    Copyright 2020 Kameswarrao Modali
    Copyright 2020 Marc Rautenhaus

    Regional Computing Center
    Universitaet Hamburg, Hamburg, Germany

    Met.3D is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Met.3D is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Met.3D.  If not, see <http://www.gnu.org/licenses/>.
"""

verbose = True
debug = False

local_base_directory = ''

dwd_base_url = "opendata.dwd.de"
dwd_base_dir = "/weather/nwp/"
dwd_nwp_models = ["cosmo-d2", "icon-eu", "icon"]

dwd_nwp_models_grid_types = {
    "cosmo-d2":
        ["regular-lat-lon_pressure-level",
         "regular-lat-lon_model-level",
         "regular-lat-lon_single-level",
         "regular-lat-lon_time-invariant",
         "rotated-lat-lon_pressure-level",
         "rotated-lat-lon_model-level",
         "rotated-lat-lon_single-level",
         "rotated-lat-lon_time-invariant"
         ],

    "icon-eu":
        ["regular-lat-lon_pressure-level",
         "regular-lat-lon_model-level",
         "regular-lat-lon_single-level",
         "regular-lat-lon_time-invariant"
         ],

    "icon":
        [
            "icosahedral_pressure-level",
            "icosahedral_model-level",
            "icosahedral_single-level",
            "icosahedral_time-invariant"
        ]
}
