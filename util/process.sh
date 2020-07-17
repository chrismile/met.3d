#Polar Stereographic - Northern Hemisphere
dir=`pwd`
original_file="HYP_50M_SR_W"
format="tif"
# Exact proj4 string from  Met3D GUI
proj_options="+proj=stere +a=6378273 +b=6356889.44891 +lat_0=90 +lat_ts=70 +lon_0=0"

#Extract the northern hemisphere by specifying the lat lon extents.
gdal_translate -projwin -180 90 180 0 ${original_file}.${format} ${original_file}_NH.${format}

# Transform the northern hemisphere gif image extracted above and with the options specified in proj4 string format. 
gdalwarp -overwrite -t_srs "${proj_options}" ${original_file}_NH.${format} ${original_file}_NH_polarstereo.${format}
