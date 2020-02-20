# -*- coding: utf-8 -*-
"""
    wxlib.retrieve_nwp
    ~~~~~~~~~~~~~~~~~~

    Collection of utility routines to obtain data from DWD open data server.

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

import os
import ftplib
import bz2
import cdo
import sys
import wxlib.config

# Initialize a CDO instance (used to convert grib to NetCDF files).
cdo_instance = cdo.Cdo()

# Initialize the global "catalogue" dictionary with the list of models specified in the config file.
catalogue = dict.fromkeys(wxlib.config.dwd_nwp_models)


def connect_to_dwd_opendata_ftpserver():
    """
    Function to connect to the repository using FTP.

    Returns: The pointer to the repository object.The quit() method of this
    object should be called to terminate the FTP connection.
    """
    repository = ftplib.FTP(wxlib.config.dwd_base_url)
    if not repository:
        print("Unable to connect to: " + wxlib.config.dwd_base_url)
        sys.exit()
    repository.login()
    repository.cwd(wxlib.config.dwd_base_dir)
    return repository


def set_local_base_directory(local_path):
    """
    Function to set the local base directory.

    Arg:
    :param local_path: The path of the local base directory to store the data.

    Returns: If the input variable 'local_path' is not a valid directory, then
    prompt and exit.
    """
    if not os.path.isdir(local_path):
        print('The specified path ' + local_path + 'is not a valid directory')
        sys.exit()
    wxlib.config.local_base_directory = local_path


def get_local_base_directory():
    """
    Function to get the local base directory.

    Returns: The path of the local base directory to store the data.
    """
    return wxlib.config.local_base_directory


def prepare_catalogue_of_available_dwd_data(queried_model_list, basetimehour_list, variable_list):
    """
    Function to prepare a catalogue (nested dictionaries), for the list of
    models queried. Updates the global variable 'catalogue' with the file
    names,corresponding to each available hour,model diagnostic and grid type.

    Arg:
    :param queried_model_list : List of model names.e.g., ["icon","icon-eu"],if
    None specified, then the catalogue is updated for all the available models in
    the repository.
    :param basetimehour_list : List of basetime hours for which the data is needed, e.g., ["00","06"],if
    None specified, then the catalogue is updated for all the available basetime hours, for each of the models in the
    'queried_model_list'
    :param variable_list : List of variable names.e.g., ["p","t"],if None specified, then the catalogue is updated for
    all the available variables, for each basetime hour in the 'basetimehour_list'
    """

    if queried_model_list is None:
        queried_model_list = wxlib.config.dwd_nwp_models

    if wxlib.config.debug:
        print(queried_model_list)

    if wxlib.config.verbose:
        print("Scanning remote ftp server at DWD to assemble catalogue...:")

    repository = connect_to_dwd_opendata_ftpserver()

    # On the remote DWD server, for a given 'model', 'basetimehour' and
    # 'variable', the directories are structured in the order e.g.,for 'cosmo-
    # d2' model 'https://opendata.dwd.de/weather/nwp/cosmo-d2/grib/basetimehour/
    # variable/', followed by the list of files.Hence, here we recursively query
    # the directory structure and obtain the list of files corresponding to the
    # available models, variables(grid types) and forecast base times of
    # the current day".

    for nwp_model in queried_model_list:

        model_dir = wxlib.config.dwd_base_dir + "/" + nwp_model + "/grib/"

        if wxlib.config.verbose:
            print("\n\n******\nChecking for NWP model directory: ", model_dir)

        repository.cwd(model_dir)

        if basetimehour_list is None:
            basetimehour_list = repository.nlst()
        catalogue[nwp_model] = dict.fromkeys(basetimehour_list)

        for basetimehour in basetimehour_list:

            if wxlib.config.verbose:
                print("\n\nChecking for base time hour: ", basetimehour)

            basetimehour_dir = model_dir + basetimehour

            repository.cwd(basetimehour_dir)

            if variable_list is None:
                variable_list = repository.nlst()
            catalogue[nwp_model][basetimehour] = dict.fromkeys(variable_list)

            for variable in variable_list:

                if wxlib.config.verbose:
                    print("Checking for forecast variable: ", variable)

                variable_dir = basetimehour_dir + "/" + variable

                repository.cwd(variable_dir)
                file_list = repository.nlst()
                grid_type_list = wxlib.config.dwd_nwp_models_grid_types[nwp_model]
                catalogue[nwp_model][basetimehour][variable] = \
                    dict.fromkeys(grid_type_list)

                for grid_type in grid_type_list:
                    catalogue[nwp_model][basetimehour][variable][grid_type] = []

                for file in file_list:
                    for grid_type in grid_type_list:
                        if grid_type in file:
                            catalogue[nwp_model][basetimehour][variable][grid_type].append(file)

    repository.quit()
    return catalogue


def determine_remote_files_to_retrieve_dwd_fcvariable(
        variable, model_name, basetime_string, grid_type, leadtime_list, level_list):
    """
    Function to get the list of available files for a given variable from in a
    given model for a specified grid type and lead times as well as levels.

    Args:
    :param variable: model diagnostic name
    :param model_name: model name
    :param basetime_string: Date and Hour(YYYYMMDDHH) at which the forecast
    simulation was started,eg., '2020022300' for forecast starting on
    2020 February 23rd at 00 hrs
    :param grid_type: Grid type eg.,'rotated-lat-lon_model-level', a complete
    list of available grid types for each model is in 'config.py'
    :param leadtime_list:Lead time in hours(xxx), eg., '001'
    :param level_list:List of levels (pressure e.g.,'200',300'
                                      model e.g., '1','2' )


    Returns:
        The list of files passing the input criteria.
    """
    queried_file_list = []
    basetimehour = basetime_string[-2:]
    complete_file_list = catalogue[model_name.lower()][basetimehour][variable.lower()][grid_type]
    if len(complete_file_list) == 0:
        print('No files found for ' + model_name + ' ' + variable + ' ' + basetime_string)
        sys.exit()

    # Check if the files are available for the input 'basetime_string'
    for single_grb_file in complete_file_list:
        if basetime_string not in single_grb_file:
            print('No files found for ' + basetime_string)
            sys.exit()

    # NOTE: concatenate 'level' and 'leadtime' strings, as they appear adjacent
    # to each other in file name e.g., 'cosmo-d2_germany_rotated-lat-lon_model-
    # level_2020021300_001_7_P.grib2.bz2', this reduces the search complexity.

    leadtimelev_list = []
    if leadtime_list is not None:
        leadtime_list = ['_' + str(x) + '_' for x in leadtime_list]
        if level_list is not None:
            level_list = [str(x) + '_' for x in level_list]
            for leadtime in leadtime_list:
                for level in level_list:
                    leadtimelev_list.append(leadtime + level)
        else:
            leadtimelev_list = leadtime_list
    else:
        if level_list is not None:
            leadtimelev_list = ['_' + str(x) + '_' for x in level_list]
        else:
            leadtimelev_list = None

    # Maintain a 'flag' array, to track if, all the queried 'leadtimes' and
    # 'levels' are available.Initialize with 'False', i.e., none are available.

    leadtimelev_list_flag = dict.fromkeys(leadtimelev_list, False)

    if wxlib.config.debug:
        print(leadtimelev_list_flag)

    for single_grb_file in complete_file_list:
        if leadtimelev_list is not None: # If user specified leadtime and/or level list exists,
                                         # then filter files accordingly
            if any(leadtimelevel in single_grb_file for leadtimelevel in leadtimelev_list):
                if wxlib.config.verbose:
                    print(single_grb_file)

                queried_file_list.append(single_grb_file)

                for leadtimelevel in leadtimelev_list:
                    if leadtimelevel in single_grb_file:
                        # Update the flag array as available \
                        # for the leadtimelevel.
                        leadtimelev_list_flag[leadtimelevel] = True
                        break
        else: # If user doesn't specify any leadtime and level list, then all files of the given variable are selected
            queried_file_list.append(single_grb_file)

    # If any of the 'leadtimes' or 'levels' queried by the user are not
    # available, then return 'None' and exit.
    if leadtimelev_list is not None:  # If user specified leadtime and/or level list exists
        # If any of the 'leadtimes' or 'levels' queried by the user are not
        # available, then return 'None' and exit.
        if False in leadtimelev_list_flag.values():
            if wxlib.config.debug:
                print(leadtimelev_list_flag)
            failed_list = [leadtimelev for leadtimelev, flag in
                       leadtimelev_list_flag.items() if flag is False]

        if wxlib.config.debug:
            print(failed_list)
        print("Data not available for: " + (','.join(failed_list)))
        return None

    return queried_file_list


def download_forecast_data(model_name, basetime_string, queried_dataset):
    """
    Function to download a given set of files, for a given model and for
    forecast simulation started at a particular hour using ftp and quit.

    Args:
    :param model_name: Name of the model
    :param basetime_string: Date and Hour(YYYYMMDDHH) at which the forecast
    simulation was started,eg., '2020022300' for forecast starting on
    2020 February 23rd at 00 hrs
    :param queried_dataset: Dictionary of model diagnostic variable names and
                    corresponding list of files names to download
    """
    if not queried_dataset:
        print("Input dataset containes no Files.")
        sys.exit()

    local_base_directory = get_local_base_directory()
    basetimehour = basetime_string[-2:]
    basetimedate = basetime_string[0:8]
    os.chdir(local_base_directory)
    download_path = local_base_directory + '/' + model_name.upper() + '/' + basetimedate

    if not os.path.exists(download_path):
        os.makedirs(download_path)
    os.chdir(download_path)
    model_dir = wxlib.config.dwd_base_dir + "/" + model_name.lower() + "/grib/"
    basetimehour_dir = model_dir + basetimehour
    repository = connect_to_dwd_opendata_ftpserver()

    for variable, grid_type_file_list_dictionary in queried_dataset.items():
        for grid_type, file_list in grid_type_file_list_dictionary.items():
            variable_dir = basetimehour_dir + "/" + variable
            repository.cwd(variable_dir)
            for single_grb_file in file_list:
                if wxlib.config.debug:
                    print("Downloading file: ", single_grb_file)
                fptr = open(single_grb_file, 'wb')
                repository.retrbinary('RETR ' + single_grb_file, fptr.write)

    repository.quit()


def merge_and_convert_downloaded_files(model_name, basetime_string, queried_dataset):
    """
    Function to process the download set of files, for a given model and for
    forecast simulation started at a particular hour. The processing consists of
    uncompressing the files, concatenating all the files of a given model
    diagnostic for a given

    Args:
    :param model_name: Name of the model
    :param basetime_string: Date and Hour(YYYYMMDDHH) at which the forecast
    simulation was started,eg., '2020022300' for forecast starting on
    2020 February 23rd at 00 hrs
    :param queried_dataset: Dictionary of model diagnostic variable names and
                    corresponding list of files names to download
    """
    if not queried_dataset:
        print("Input dataset contains no files.")
        sys.exit()

    if wxlib.config.verbose:
        print("\n\nUncompressing, merging and converting downloaded data...")

    local_base_directory = get_local_base_directory()
    basetimehour = basetime_string[-2:]
    basetimedate = basetime_string[0:8]

    os.chdir(local_base_directory)
    queried_dataset_path = local_base_directory + '/' + model_name.upper() + '/' + basetimedate

    if not os.path.exists(queried_dataset_path):
        print("The local dataset path" + queried_dataset_path + " doesn't exist.")
        sys.exit()

    os.chdir(queried_dataset_path)

    for variable, grid_type_file_list_dictionary in queried_dataset.items():
        for grid_type, file_list in grid_type_file_list_dictionary.items():

            if wxlib.config.verbose:
                print("* ", variable, " (", grid_type, ") ..")

            temp_file_name = file_list[0]
            # 'cosmo-d2_germany_rotated-lat-lon_model-level_2020021300_001_7_P.\
            # grib2.bz2'
            file_name = temp_file_name.split('_')[0:5]
            # cosmo-d2_germany_rotated-lat-lon_model-level_2020021300
            file_name.append(variable.upper())
            file_name = '_'.join(file_name)
            grib_file_name = file_name + '.grib2'
            nc_file_name = file_name + '.nc'

            # Uncompress the files of each 'variable' and for given 'grid type', using bz2 module.
            with open(temp_file_name[:-4], 'wb') as out_file, bz2.BZ2File(temp_file_name, 'rb') as zip_file:
                for data in iter(lambda: zip_file.read(100 * 1024), b''):
                    out_file.write(data)

            # Create a single file for each variable and for given grid type.
            command = "cat " + "*" + grid_type + "*" + variable.upper() + \
                      '.grib2 >' + grib_file_name
            os.system(command)
            # Create the NetCDF file from the concatenated file.
            cdo_instance.copy(input=grib_file_name, output=nc_file_name, options='-f nc')
            # Clean up the unzipped individual files.
            command = "rm " + "*" + grid_type + '_' + basetime_string \
                      + '_*_*_' + variable.upper() + '.grib2*'
            os.system(command)

            if wxlib.config.verbose:
                print("    .. done")

