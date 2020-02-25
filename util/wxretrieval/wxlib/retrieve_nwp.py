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
import pickle
import logging
import cdo
import wxlib.config

# Initialize a CDO instance (used to convert grib to NetCDF files).
cdo_instance = cdo.Cdo()

# Initialize the global "catalogue" dictionary with the list of models specified in the config file.
catalogue = dict.fromkeys(wxlib.config.dwd_nwp_models)


def connect_to_dwd_opendata_ftpserver():
    """
    Function to connect to the repository using FTP.

    Returns: The pointer to the repository object.The quit() method of this
    object should be called to terminate the FTP connection. If unsuccessful returns None.
    """
    repository = None
    logging.info("Connecting to remote ftp server '%s'..." % wxlib.config.dwd_base_url)
    try:
        repository = ftplib.FTP(wxlib.config.dwd_base_url)
        repository.login()
        repository.cwd(wxlib.config.dwd_base_dir)
    except ftplib.all_errors as error_message:
        logging.error("Unable to connect to '%s': %s" % (wxlib.config.dwd_base_url, error_message))
    return repository


def set_local_base_directory(local_path):
    """
    Function to set the local base directory.

    Arg:
    :param local_path: The path of the local base directory to store the data.

    Returns: If successful return True, else if the input variable 'local_path' is not a valid directory, then
    prompt and return False.
    """
    if not os.path.isdir(local_path):
        logging.error("Local data directory '%s' is not a valid directory." % local_path)
        return False
    logging.info("Setting the local data directory to '%s'." % local_path)
    wxlib.config.local_base_directory = local_path
    return True


def get_local_base_directory():
    """
    Function to get the local base directory.

    Returns: The path of the local base directory to store the data.
    """
    return wxlib.config.local_base_directory


def write_dictionary_to_file(dictionary_object, dictionary_file_path):
    """
    Function to write a dictionary to a give file and path.

    Arg:
    :param dictionary_object: The dictionary object to be written out.
    :param dictionary_file_path: The file name and path to store the dictionary.

    Returns: True if successful else False
    """
    if dictionary_object is None:
        logging.error("Input dictionary object has no valid entries.")
        return False

    if not os.path.isdir(dictionary_file_path):
        logging.warning("Directory '%s' does not exist." % dictionary_file_path)

    if os.path.exists(dictionary_file_path):
        logging.warning("Overwriting existing dictionary file '%s'." % dictionary_file_path)

    try:
        with open(dictionary_file_path, 'wb') as handle:
            pickle.dump(dictionary_object, handle)
    except (IOError, OSError, pickle.PickleError, pickle.UnpicklingError):
        logging.error("Unable to write dictionary file '%s'" % dictionary_file_path)
        return False

    return True


def read_dictionary_from_file(dictionary_file_path):
    """
    Function to read a dictionary from a given path and file.

    Arg:
    :param dictionary_file_path: The file name and path to read the dictionary.

    Returns: If successful returns the dictionary object read in from file, else None.
    """
    if not os.path.exists(dictionary_file_path):
        logging.error("Dictionary file '%s' does not exist." % dictionary_file_path)
        return None
    try:
        with open(dictionary_file_path, 'rb') as handle:
            dictionary_object = pickle.loads(handle.read())
            return dictionary_object
    except (IOError, OSError, pickle.PickleError, pickle.UnpicklingError):
        logging.error("Unable to read dictionary file '%s'" % dictionary_file_path)
        return None


def store_catalogue_in_local_base_directory(catalogue_object, catalogue_name):
    """
    Function to store the catalogue (binary file) in the 'local_base_directory'.

    Arg:
    :param catalogue_object : Dictionary object of the catalogue file.
    :param catalogue_name : Desired name of the catalogue file.

    Returns: True if successful else False
    """
    if catalogue_object is None:
        logging.error("Input catalogue has no valid entries.")
        return False

    catalogue_path = get_local_base_directory()
    catalogue_file_path = os.path.join(catalogue_path, catalogue_name)

    if not os.path.isdir(catalogue_path):
        logging.warning("Creating directory '%s'." % catalogue_path)
        os.makedirs(catalogue_path)

    return write_dictionary_to_file(catalogue_object, catalogue_file_path)


def read_catalogue_from_local_base_directory(catalogue_name):
    """
    Function to read the catalogue (binary file) from the 'local_base_directory' and set it to the global catalogue.

    Arg:
    :param catalogue_name : Name of the catalogue file.

    Returns: True if successful else False
    """
    global catalogue
    catalogue_file_path = os.path.join(get_local_base_directory(), catalogue_name)
    logging.info("Reading catalogue from file '%s'." % catalogue_file_path)
    catalogue = read_dictionary_from_file(catalogue_file_path)
    return catalogue is not None


def prepare_catalogue_of_available_dwd_data(queried_model_list, basetimes_list, variable_list):
    """
    Function to prepare a catalogue (nested dictionaries), for the list of
    models queried. Updates the global variable 'catalogue' with the file
    names,corresponding to each available hour,model diagnostic and grid type.

    Arg:
    :param queried_model_list : List of model names.e.g., ["icon","icon-eu"],if
    None specified, then the catalogue is updated for all the available models in
    the repository.
    :param basetimes_list : List of base times (datetime.datetime). If 'None' specified, the catalogue
    is updated for all base times available on the remote server.
    :param variable_list : List of variable names.e.g., ["p","t"],if None specified, then the catalogue is updated for
    all the available variables, for each base time in 'basetimes_list'

    Returns: catalogue object if successful else None
    """
    global catalogue
    repository = connect_to_dwd_opendata_ftpserver()
    if not repository:
        return None

    if queried_model_list is None:
        queried_model_list = wxlib.config.dwd_nwp_models

    logging.info("***** Scanning remote ftp server at DWD to assemble catalogue...:")
    logging.info("[Models to be queried: %s]" % ", ".join(queried_model_list))
    logging.info("[Base times to be queried: %s]" % ", ".join([t.isoformat() for t in basetimes_list]))

    # On the remote DWD server, for a given 'model', 'base_hour_str' and
    # 'variable', the directories are structured in the order e.g.,for 'cosmo-
    # d2' model 'https://opendata.dwd.de/weather/nwp/cosmo-d2/grib/basetimehour/
    # variable/', followed by the list of files.Hence, here we recursively query
    # the directory structure and obtain the list of files corresponding to the
    # available models, variables(grid types) and forecast base times of
    # the current day".

    for nwp_model in queried_model_list:
        model_dir = os.path.join(wxlib.config.dwd_base_dir, nwp_model, "grib")

        logging.info("**** Checking for NWP model '%s' in remote directory '%s'..." % (nwp_model, model_dir))
        repository.cwd(model_dir)

        # Create list of strings representing the base times, to be used as subdirectory names
        # on the DWD opendata server.
        if basetimes_list is None:
            basetimes_strlist = repository.nlst()
        else:
            basetimes_strlist = [t.strftime("%H") for t in basetimes_list]

        catalogue[nwp_model] = dict.fromkeys(basetimes_strlist)

        for base_hour_str in basetimes_strlist:
            logging.info("*** Checking for base time hour '%s'." % base_hour_str)
            basetimehour_dir = os.path.join(model_dir, base_hour_str)
            repository.cwd(basetimehour_dir)

            if variable_list is None:
                variable_list = repository.nlst()
            catalogue[nwp_model][base_hour_str] = dict.fromkeys(variable_list)

            for variable in variable_list:
                logging.info("** Checking for forecast variable '%s'." % variable)
                variable_dir = os.path.join(basetimehour_dir, variable)

                repository.cwd(variable_dir)
                file_list = repository.nlst()
                grid_type_list = wxlib.config.dwd_nwp_models_grid_types[nwp_model]
                catalogue[nwp_model][base_hour_str][variable] = \
                    dict.fromkeys(grid_type_list)

                for grid_type in grid_type_list:
                    catalogue[nwp_model][base_hour_str][variable][grid_type] = []

                for file in file_list:
                    for grid_type in grid_type_list:
                        if grid_type in file:
                            catalogue[nwp_model][base_hour_str][variable][grid_type].append(file)

    repository.quit()
    logging.info("Catalogue of remote data files has been prepared.")
    return catalogue


def determine_remote_files_to_retrieve_dwd_fcvariable(
        variable, model_name, basetime, grid_type, leadtime_list, level_list):
    """
    Function to get the list of available files for a given variable from in a
    given model for a specified grid type and lead times as well as levels.

    Args:
    :param variable: model diagnostic name
    :param model_name: model name
    :param basetime: Datetime object specifying forecast base time
    :param grid_type: Grid type eg.,'rotated-lat-lon_model-level', a complete
    list of available grid types for each model is in 'config.py'
    :param leadtime_list: Lead time in hours(xxx), e.g. [3, 6, 9, 12]
    :param level_list: List of levels (pressure, e.g. [200, 300] or model level, e.g. [1, 2, 3, 4])

    Returns:
    If successful, the list of files passing the input criteria, else None
    """
    queried_file_list = []
    base_hour_str = basetime.strftime("%H")
    basetime_str = basetime.strftime("%Y%m%d%H")
    complete_file_list = catalogue[model_name.lower()][base_hour_str][variable.lower()][grid_type]
    if len(complete_file_list) == 0:
        logging.error("No data files found for variable '%s', grid type '%s', for NWP model '%s' at base time '%s'."
                      % (variable, grid_type, model_name, basetime_str))
        return None

    # Check if the files are available for the input 'basetime_str'
    for single_grb_file in complete_file_list:
        if basetime_str not in single_grb_file:
            logging.error("No data files found for base time '%s'." % basetime_str)
            return None

    # NOTE: concatenate 'level' and 'leadtime' strings, as they appear adjacent
    # to each other in file name e.g., 'cosmo-d2_germany_rotated-lat-lon_model-
    # level_2020021300_001_7_P.grib2.bz2', this reduces the search complexity.

    leadtimelev_list = []
    if leadtime_list is not None:
        leadtime_list = ["_%03i_" % x for x in leadtime_list]
        if level_list is not None:
            level_list = ["%i_" % x for x in level_list]
            for leadtime in leadtime_list:
                for level in level_list:
                    leadtimelev_list.append(leadtime + level)
        else:
            leadtimelev_list = leadtime_list
    else:
        if level_list is not None:
            leadtimelev_list = ["_%i_" % x for x in level_list]
        else:
            leadtimelev_list = None

    # Maintain a 'flag' array, to track if, all the queried 'leadtimes' and
    # 'levels' are available.Initialize with 'False', i.e., none are available.

    leadtimelev_list_flag = dict.fromkeys(leadtimelev_list, False)

    for single_grb_file in complete_file_list:
        if leadtimelev_list is not None:  # If user specified leadtime and/or level list exists,
            # then filter files accordingly
            if any(leadtimelevel in single_grb_file for leadtimelevel in leadtimelev_list):
                logging.info("Querying remote file '%s' for download.." % single_grb_file)
                queried_file_list.append(single_grb_file)

                for leadtimelevel in leadtimelev_list:
                    if leadtimelevel in single_grb_file:
                        # Update the flag array as available \
                        # for the leadtimelevel.
                        leadtimelev_list_flag[leadtimelevel] = True
                        break
        else:  # If user doesn't specify any leadtime and level list, then all files of the given variable are selected
            queried_file_list.append(single_grb_file)

    logging.debug("Debug: 'leadtime_lev_list_flag' entries ('True' if any file found satisfying the user "
                  "specification):\n%s" % str(leadtimelev_list_flag))

    if leadtimelev_list is not None:  # If user specified leadtime and/or level list exists
        # If any of the 'leadtimes' or 'levels' queried by the user are not
        # available, then return 'None' and exit.
        if False in leadtimelev_list_flag.values():
            logging.debug(leadtimelev_list_flag)
            failed_list = [leadtimelev for leadtimelev, flag in
                           leadtimelev_list_flag.items() if flag is False]

            logging.debug(failed_list)
            logging.error("Data not available for the following *_leadtime_levels_*: %s" % ",".join(failed_list))
            return None

    logging.debug("The following files have been queried for download:%s" % "\n\t".join(queried_file_list))
    return queried_file_list


def download_forecast_data(model_name, basetime, queried_dataset):
    """
    Function to download a given set of files, for a given model and for
    forecast simulation started at a particular hour using ftp. If any error occurs during download
    of a file, prompt error and delete the local empty file and quit ftp.

    Args:
    :param model_name: Name of the model.
    :param basetime: Datetime object specifying forecast base time
    :param queried_dataset: Dictionary of model diagnostic variable names and
                    corresponding list of files names to download.

    Returns: True if all files are downloaded successfully, else False.
    """
    logging.info("***** Downloading forecast data...")

    if not queried_dataset:
        logging.error("Input dataset contains no files.")
        return False

    local_base_directory = get_local_base_directory()
    base_hour_str = basetime.strftime("%H")
    base_date_str = basetime.strftime("%Y%m%d")
    os.chdir(local_base_directory)
    download_path = os.path.join(local_base_directory, model_name.upper(), base_date_str)
    if not os.path.exists(download_path):
        os.makedirs(download_path)
    os.chdir(download_path)

    model_dir = os.path.join(wxlib.config.dwd_base_dir, model_name.lower(), "grib")
    base_hour_dir = os.path.join(model_dir, base_hour_str)
    repository = connect_to_dwd_opendata_ftpserver()

    for variable, grid_type_file_list_dictionary in queried_dataset.items():
        for grid_type, file_list in grid_type_file_list_dictionary.items():
            variable_dir = os.path.join(base_hour_dir, variable)
            repository.cwd(variable_dir)
            logging.debug(",".join(file_list))
            for single_grb_file in file_list:
                try:
                    logging.info("Downloading file '%s'.." % single_grb_file)
                    with open(single_grb_file, 'wb') as fptr:
                        repository.retrbinary('RETR ' + single_grb_file, fptr.write)
                        fptr.close()
                        logging.info("    .. done")
                except ftplib.error_perm:
                    logging.error("File '%s' not available." % single_grb_file)
                    os.unlink(single_grb_file)
                    repository.quit()
                    return False

    repository.quit()
    logging.info("    .. download of forecast data has finished.")
    return True


def merge_and_convert_downloaded_files(model_name, basetime, queried_dataset):
    """
    Function to process the download set of files, for a given model and for
    forecast simulation started at a particular hour. The processing consists of
    uncompressing the files, concatenating all the files of a given model
    diagnostic for a given

    Args:
    :param model_name: Name of the model
    :param basetime: Datetime object specifying forecast base time
    :param queried_dataset: Dictionary of model diagnostic variable names and
                    corresponding list of files names to download

    Returns: True if successful, else False
    """
    logging.info("***** Uncompressing, merging and converting downloaded data...")

    if not queried_dataset:
        logging.error("Input dataset contains no files.")
        return False

    local_base_directory = get_local_base_directory()
    basetime_str = basetime.strftime("%Y%m%d%H")
    base_date_str = basetime.strftime("%Y%m%d")

    os.chdir(local_base_directory)
    queried_dataset_path = os.path.join(local_base_directory, model_name.upper(), base_date_str)

    if not os.path.exists(queried_dataset_path):
        logging.error("The local dataset path '%s' doesn't exist." % queried_dataset_path)
        return False

    os.chdir(queried_dataset_path)

    for variable, grid_type_file_list_dictionary in queried_dataset.items():
        for grid_type, file_list in grid_type_file_list_dictionary.items():
            logging.info("* %s (%s) .." % (variable, grid_type))

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
                logging.info("Overwriting existing files for variable '%s'." % variable)
                os.remove(grib_file_name)
                os.remove(nc_file_name)
            try:
                # Uncompress the files of each 'variable' and for given 'grid type', using bz2 module.
                with open(unzip_file_name, 'wb') as unzip_file_pointer, bz2.BZ2File(zip_file_name,
                                                                                    'rb') as zip_file_pointer:
                    for data in iter(lambda: zip_file_pointer.read(), b''):
                        unzip_file_pointer.write(data)
                    unzip_file_pointer.close()
                    zip_file_pointer.close()
            except (IOError, EOFError):
                logging.error("Unable to uncompress file '%s'." % variable)
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
            command = "rm " + "*" + grid_type + '_' + basetime_str \
                      + '_*_*_' + variable.upper() + '.grib2*'
            os.system(command)
            logging.info("    .. done")

    return True
