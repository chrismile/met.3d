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
import sys

try:
    import ftplib
except ImportError:
    print("Module 'ftplib' not available")

try:
    import bz2
except ImportError:
    print("Module 'bz2' not available")

try:
    import cdo
except ImportError:
    print("Module 'cdo' not available")

try:
    import pickle
except ImportError:
    print("Module 'pickle' not available")

try:
    import logging
except ImportError:
    print("Module 'logging' not available")

try:
    import wxlib.config
except ImportError:
    print("Module 'wxlib.config' not available")



# Initialize a CDO instance (used to convert grib to NetCDF files).
cdo_instance = cdo.Cdo()

# Initialize the global "catalogue" dictionary with the list of models specified in the config file.
catalogue = dict.fromkeys(wxlib.config.dwd_nwp_models)
log = None
console = None


def initialize_logging():
    """
        Function to create the logger along with a 'console' handler, with default logging level set to 'INFO'
        Returns: 'log' object
    """
    global log, console
    if not log:
        log = logging.getLogger()
    if not console:
        console = logging.StreamHandler()
        log.addHandler(console)
    log.setLevel(logging.INFO)
    console.setLevel(logging.INFO)
    return log


# NOTE: logging levels in descending order of severity :
#    logging.CRITICAL > logging.ERROR > logging.WARNING > logging.INFO > logging.DEBUG > logging.NOTSET
def set_logging_level(logger_level):
    """
        Function to set the logging level. If 'INFO' level, print 'info' messages. If 'DEBUG' levele, then additionally
        print the debug messages and  also to a log file named 'wxretrieval.log'.
        Arg:
        :param level : Mode to be set , any of 'CRITICAL', 'ERROR', 'WARNING', 'INFO', 'DEBUG' or 'NOTSET'
    """
    global log, console
    if not log or not console:
        initialize_logging()

    log.setLevel(logger_level)
    console.setLevel(logger_level)
    log.addHandler(console)
    # NOTE: For Complete list of format directives refer https://docs.python.org/3/library/time.html#time.strftime
    datefmt = '%Y-%b-%d %H:%M:%S'
    formatter = logging.Formatter('%(asctime)s %(levelname)s: %(message)s',datefmt)
    console.setFormatter(formatter)

    if logger_level is logging.DEBUG:
        # Additionally write the log to a file named 'wxretieval.log' for debugging purposes.
        logfile_name = 'wxretrieval.log'
        if os.path.exists(logfile_name):
            os.remove(logfile_name)
        handler = logging.FileHandler(logfile_name)
        log.addHandler(handler)
        handler.setFormatter(formatter)


def connect_to_dwd_opendata_ftpserver():
    """
    Function to connect to the repository using FTP.

    Returns: The pointer to the repository object.The quit() method of this
    object should be called to terminate the FTP connection. If unsuccessful returns None.
    """
    repository = None
    message = "Connecting to '" + wxlib.config.dwd_base_url + "'"
    log.info(message)
    try:
        repository = ftplib.FTP(wxlib.config.dwd_base_url)
        repository.login()
        repository.cwd(wxlib.config.dwd_base_dir)
    except ftplib.all_errors as error_message:
        log.critical(error_message)
        message = "Cannot connect to : " + wxlib.config.dwd_base_url
        log.critical(message)
    return repository


def set_local_base_directory(local_path):
    """
    Function to set the local base directory.

    Arg:
    :param local_path: The path of the local base directory to store the data.

    Returns: If successful return True, else if the input variable 'local_path' is not a valid directory, then
    prompt and return False.
    """
    global log
    if not os.path.isdir(local_path):
        message = "The specified path '" + local_path + "' is not a valid directory"
        log.error(message)
        return False
    message = "Changing the working directory to: '" + local_path + "'"
    log.info(message)
    wxlib.config.local_base_directory = local_path
    return True


def get_local_base_directory():
    """
    Function to get the local base directory.

    Returns: The path of the local base directory to store the data.
    """
    return wxlib.config.local_base_directory


def store_catalogue_in_local_base_directory(catalogue_object, catalogue_name):
    """
        Function to store the catalogue (binary file) in the 'local_base_directory'.

        Arg:
        :param catalogue_object : Dictionary object of the catalogue file.
        :param catalogue_name : Desired name of the catalogue file.
    Returns: True if successful else False
    """
    if catalogue_object is None:
        log.error('Input catalogue has no valid entries')
        return False

    catalogue_path = get_local_base_directory()
    catalogue_file_path = catalogue_path + '/' + catalogue_name
    if not os.path.isdir(catalogue_path):
        message = "Creating the directory: '" + catalogue_path + "'"
        log.warning(message)
        os.makedirs(catalogue_path)

    if os.path.exists(catalogue_file_path):
        message = "Overwriting the existing catalogue: '" + catalogue_file_path + "'"
        log.warning(message)

    try:
        with open(catalogue_file_path,'wb') as handle:
            pickle.dump(catalogue_object, handle)
    except (IOError, OSError, pickle.PickleError, pickle.UnpicklingError):
        message = "Unable to write catalogue file: '" + catalogue_file_path + "'"
        log.critical(message)
        return False
    return True


def read_catalogue_from_local_base_directory(catalogue_name):
    """
        Function to read the catalogue (binary file) from the 'local_base_directory' and set it to the global catalogue.

        Arg:
        :param catalogue_name : Name of the catalogue file.
    Returns: True if successful else False
    """
    global catalogue
    catalogue_path = get_local_base_directory()
    catalogue_file_path = catalogue_path + '/' + catalogue_name
    message = "Reading catalogue: '" + catalogue_file_path + "'"
    log.info(message)
    if not os.path.exists(catalogue_path):
        message = "Catalogue doesn't exist in the path: '" + catalogue_file_path + "'"
        log.critical(message)
        return False
    try:
        with open(catalogue_file_path,'rb') as handle:
            catalogue = pickle.loads(handle.read())
    except (IOError, OSError, pickle.PickleError, pickle.UnpicklingError):
        message = "Unable to read catalogue file: '" + catalogue_file_path + "'"
        log.critical(message)
        return False
    return True


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
    Returns: catalogue object if successful else None
    """
    global log, console, catalogue
    repository = connect_to_dwd_opendata_ftpserver()
    if not repository:
        return None

    if queried_model_list is None:
        queried_model_list = wxlib.config.dwd_nwp_models

    log.debug(queried_model_list)
    log.info("Scanning remote ftp server at DWD to assemble catalogue...:")

    # On the remote DWD server, for a given 'model', 'basetimehour' and
    # 'variable', the directories are structured in the order e.g.,for 'cosmo-
    # d2' model 'https://opendata.dwd.de/weather/nwp/cosmo-d2/grib/basetimehour/
    # variable/', followed by the list of files.Hence, here we recursively query
    # the directory structure and obtain the list of files corresponding to the
    # available models, variables(grid types) and forecast base times of
    # the current day".

    for nwp_model in queried_model_list:

        model_dir = wxlib.config.dwd_base_dir + "/" + nwp_model + "/grib/"


        message = "\n\n******\nChecking for NWP model '" + nwp_model + "' in the directory: '" + model_dir + "'"
        log.info(message)
        repository.cwd(model_dir)

        if basetimehour_list is None:
            basetimehour_list = repository.nlst()
        catalogue[nwp_model] = dict.fromkeys(basetimehour_list)

        for basetimehour in basetimehour_list:


            message = "\n\nChecking for base time hour: '" + basetimehour + "'"
            log.info(message)
            basetimehour_dir = model_dir + basetimehour
            repository.cwd(basetimehour_dir)

            if variable_list is None:
                variable_list = repository.nlst()
            catalogue[nwp_model][basetimehour] = dict.fromkeys(variable_list)

            for variable in variable_list:

                message = "Checking for forecast variable: '" + variable + "'"
                log.info(message)
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
    log.info("Catalogue prepared.")
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
        If successful, the list of files passing the input criteria  else None
    """
    global log, console
    queried_file_list = []
    basetimehour = basetime_string[-2:]
    complete_file_list = catalogue[model_name.lower()][basetimehour][variable.lower()][grid_type]
    if len(complete_file_list) == 0:
        message = "No suitable files found for variable : '" + variable + "' and grid type : '" + grid_type + \
                  "' \nin NWP model : '" + model_name + "' for basetime: " + basetime_string
        log.error(message)
        return None

    # Check if the files are available for the input 'basetime_string'
    for single_grb_file in complete_file_list:
        if basetime_string not in single_grb_file:
            message = 'No files found for ' + basetime_string
            log.error(message)
            return None

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

    for single_grb_file in complete_file_list:
        if leadtimelev_list is not None: # If user specified leadtime and/or level list exists,
                                         # then filter files accordingly
            if any(leadtimelevel in single_grb_file for leadtimelevel in leadtimelev_list):
                message = "Found file: " + single_grb_file
                log.info(message)
                queried_file_list.append(single_grb_file)

                for leadtimelevel in leadtimelev_list:
                    if leadtimelevel in single_grb_file:
                        # Update the flag array as available \
                        # for the leadtimelevel.
                        leadtimelev_list_flag[leadtimelevel] = True
                        break
        else: # If user doesn't specify any leadtime and level list, then all files of the given variable are selected
            queried_file_list.append(single_grb_file)

    message= "The 'leadtime_lev_list_flag' entries, 'True' if any file found satisfying the user specification:\n" \
             + str(leadtimelev_list_flag)
    log.debug(message)

    if leadtimelev_list is not None:  # If user specified leadtime and/or level list exists
        # If any of the 'leadtimes' or 'levels' queried by the user are not
        # available, then return 'None' and exit.
        if False in leadtimelev_list_flag.values():
            log.debug(leadtimelev_list_flag)
            failed_list = [leadtimelev for leadtimelev, flag in
                           leadtimelev_list_flag.items() if flag is False]

            log.debug(failed_list)
            message = "Data not available for *_leadtime_levels_* : " + (','.join(failed_list))
            log.error(message)
            return None
    log.debug(" The list of files satisfying the user specifications:\n")
    log.debug("\n\t".join(queried_file_list))
    return queried_file_list


def download_forecast_data(model_name, basetime_string, queried_dataset):
    """
    Function to download a given set of files, for a given model and for
    forecast simulation started at a particular hour using ftp. If any error occurs during download
    of a file, prompt error and delete the local empty file and quit ftp.

    Args:
    :param model_name: Name of the model.
    :param basetime_string: Date and Hour(YYYYMMDDHH) at which the forecast.
    simulation was started,eg., '2020022300' for forecast starting on
    2020 February 23rd at 00 hrs.
    :param queried_dataset: Dictionary of model diagnostic variable names and
                    corresponding list of files names to download.

    Returns: True if all files are downloaded successfully, else False.
    """
    global log, console
    if not queried_dataset:
        log.error("Input dataset contains no Files.")
        return False

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
            log.debug(",".join(file_list))
            for single_grb_file in file_list:
                try:
                    message = "Downloading file: '" + single_grb_file + "'"
                    log.info(message)
                    with open(single_grb_file, 'wb') as fptr:
                        repository.retrbinary('RETR ' + single_grb_file, fptr.write)
                        fptr.close()
                        log.info('Done')
                except error_perm:
                    message = "File: '" + single_grb_file + "' not available."
                    log.error(message)
                    os.unlink(single_grb_file)
                    repository.quit()
                    return False
    repository.quit()
    return True


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
    Returns: True if successful, else False
    """
    global log,console
    if not queried_dataset:
        log.error("Input dataset contains no files.")
        return False


    log.info("\n\nUncompressing, merging and converting downloaded data...")

    local_base_directory = get_local_base_directory()
    basetimedate = basetime_string[0:8]

    os.chdir(local_base_directory)
    queried_dataset_path = local_base_directory + '/' + model_name.upper() + '/' + basetimedate

    if not os.path.exists(queried_dataset_path):
        message = "The local dataset path" + queried_dataset_path + " doesn't exist."
        log.error(message)
        return False

    os.chdir(queried_dataset_path)

    for variable, grid_type_file_list_dictionary in queried_dataset.items():
        for grid_type, file_list in grid_type_file_list_dictionary.items():

            message = "* " + variable + " (" + grid_type + ") .."
            log.info(message)

            zip_file_name = file_list[0]
            unzip_file_name = zip_file_name[:-4]
            # 'cosmo-d2_germany_rotated-lat-lon_model-level_2020021300_001_7_P.grib2.bz2'
            file_name = zip_file_name.split('_')[0:5]
            # cosmo-d2_germany_rotated-lat-lon_model-level_2020021300
            file_name.append(variable.upper())
            file_name = '_'.join(file_name)
            grib_file_name = file_name + '.grib2'
            nc_file_name = file_name + '.nc'

            if os.path.exists(grib_file_name) or os.path.exists(nc_file_name):
                message = 'Overwriting the existing files for ' + "'"+ variable + "'"
                log.info(message)
                os.remove(grib_file_name)
                os.remove(nc_file_name)
            try:
                # Uncompress the files of each 'variable' and for given 'grid type', using bz2 module.
                with open(unzip_file_name, 'wb') as unzip_file_pointer, bz2.BZ2File(zip_file_name, 'rb') as zip_file_pointer:
                    for data in iter(lambda: zip_file_pointer.read(), b''):
                        unzip_file_pointer.write(data)
                    unzip_file_pointer.close()
                    zip_file_pointer.close()
            except (IOError, EOFError):
                message = 'Unbale to Uncompress file ' + "'" + variable + "'"
                log.error(message)
                unzip_file_pointer.close()
                zip_file_pointer.close()
                os.unlink(unzip_file_name)
                return False

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
            log.info("    .. done")

    return True
