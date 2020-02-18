# -*- coding: utf-8 -*-
"""
    wxlib.retrieve_dwd_example
    ~~~~~~~~~~~~~~~~~~~~~~~~~~

    An example to demonstrate the data retrieval from DWD opendata.

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

import sys
import wxlib.retrieve_nwp

download_data = True
process_data = True

variable = "p"
model_name = "cosmo-d2"
leadtime_list = ["001", "010"]
basetimedate = "20200218"
basetimehour = "00"
basetime_string = basetimedate + basetimehour  # 2020021300
grid_type = "rotated-lat-lon_model-level"
# The following list of vertical levels contains model levels; if querying for a pressure level diagnostic,
# then specify pressure level values instead.
level_list = ["1", "2", "3", "4", "7", "10"]

# None       --- prepares catalogue for all the models from DWD
# input_list --- prepares catalogue for the models in input_list
wxlib.retrieve_nwp.prepare_catalogue_of_available_dwd_data(["cosmo-d2"])

# Overwrite standard directory to store retrieved files.
wxlib.retrieve_nwp.set_local_base_directory("/mnt/data1/wxretrieval")

queried_dataset = dict()
filtered_list = wxlib.retrieve_nwp.determine_remote_files_to_retrieve_dwd_fcvariable(
    variable, model_name, basetime_string, grid_type, leadtime_list, level_list)

if not filtered_list:
    queried_dataset = None
    sys.exit()
else:
    queried_dataset[variable] = dict()
    queried_dataset[variable][grid_type] = filtered_list

if download_data:
    wxlib.retrieve_nwp.download_forecast_data(model_name, basetime_string, queried_dataset)

if process_data:
    wxlib.retrieve_nwp.merge_and_convert_downloaded_files(model_name,
                                                          basetime_string, queried_dataset)
