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

import logging
import datetime
import wxlib.retrieve_nwp

# CONFIGURATION
# =============================================================================

# Model and parameters to be retrieved.
model_name = "cosmo-d2"
grid_type = "rotated-lat-lon_model-level"

# Get the current UTC data at 00Z and use it as base time.
base_time = datetime.datetime.utcnow().replace(hour=0, minute=0, second=0, microsecond=0)
# .. or explicitly set a date:
# base_time = datetime.datetime(2020, 2, 25, 0)

variable_list = ["p", "t"]
leadtime_list = [1, 10]
# List of vertical levels can contain either model levels or pressure levels.
level_list = [1, 2, 3, 4, 7, 10]

# Directory in which data will be stored.
local_data_dir_path = '/mnt/data1/wxretrieval'

# RETRIEVAL
# =============================================================================

# Initialize logging system.
# See https://docs.python.org/3/howto/logging.html#logging-basic-tutorial
logging.basicConfig(level=logging.INFO,
                    format="[%(asctime)s %(filename)s:%(lineno)s %(levelname)s] %(message)s",
                    datefmt="%Y-%m-%d %H:%M:%S")

# Tell retrieval library to store files in the directory specified above.
wxlib.retrieve_nwp.set_local_base_directory(local_data_dir_path)

# Prepare catalogue of remotely available data: check which data is available on the remote
# server and which variables are stored in which files.
catalogue = wxlib.retrieve_nwp.prepare_catalogue_of_available_dwd_data([model_name], [base_time], variable_list)
if catalogue is None:
    logging.warning('Catalogue is not valid.')
    exit()

# Optionally you can store the catalogue in a file to be used by different scripts.
test_catalogue_storage = False
if test_catalogue_storage:
    catalogue_file_name = 'dwd_opendata_catalogue.bin'
    if catalogue is not None:
        if not wxlib.retrieve_nwp.store_catalogue_in_local_base_directory(catalogue, catalogue_file_name):
            exit()
    else:
        logging.warning('Catalogue has no valid entries.')
        exit()

    if not wxlib.retrieve_nwp.read_catalogue_from_local_base_directory(catalogue_file_name):
        exit()

remote_datafiles = dict()
for variable in variable_list:
    remote_files_for_variable = wxlib.retrieve_nwp.determine_remote_files_to_retrieve_dwd_fcvariable(
        variable, model_name, base_time, grid_type, leadtime_list, level_list)

    if remote_files_for_variable is None:
        logging.critical(
            "No remote data files were discovered for variable '%s', grid type '%s', for NWP model '%s'"
            " at base time '%s' at lead times '%s' and vertical levels '%s'." % (
                variable, grid_type, model_name, base_time.isoformat(),
                ",".join([str(x) for x in leadtime_list]), ",".join([str(x) for x in level_list])))
        exit()
    else:
        remote_datafiles[variable] = dict()
        remote_datafiles[variable][grid_type] = remote_files_for_variable

# Download forecast data, merge the individual GRIB file and convert to a NetCDF file.
wxlib.retrieve_nwp.download_forecast_data(model_name, base_time, remote_datafiles)
wxlib.retrieve_nwp.merge_and_convert_downloaded_files(model_name, base_time, remote_datafiles)
