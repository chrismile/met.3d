#!/bin/bash

set -euo pipefail

MET3D_BASE_PATH="/home/$USER/met.3d-base"
SHIPPING_DIR="$MET3D_BASE_PATH/Shipping"

is_installed_apt() {
    local pkg_name="$1"
    if [ "$(dpkg -l | awk '/'"$pkg_name"'/ {print }'|wc -l)" -ge 1 ]; then
        return 0
    else
        return 1
    fi
}

if command -v apt &> /dev/null; then
    if ! command -v cmake &> /dev/null || ! command -v git &> /dev/null || ! command -v curl &> /dev/null \
            || ! command -v wget &> /dev/null || ! command -v zip &> /dev/null \
            || ! command -v pkg-config &> /dev/null || ! command -v g++ &> /dev/null \
            || ! command -v gfortran &> /dev/null || ! command -v patchelf &> /dev/null; then
        echo "------------------------"
        echo "Installing build essentials"
        echo "------------------------"
        sudo apt install -y cmake git curl wget zip pkg-config build-essential gfortran patchelf
    fi
    
    # qt5-default replaced by qtbase5-dev qtchooser qt5-qmake qtbase5-dev-tools since Ubuntu 22.04.
    if ! is_installed_apt "qtbase5-dev" || ! is_installed_apt "qtchooser" \
            || ! is_installed_apt "qt5-qmake" || ! is_installed_apt "qtbase5-dev-tools" \
            || ! is_installed_apt "libqt5charts5-dev" || ! is_installed_apt "liblog4cplus-dev" \
            || ! is_installed_apt "libgdal-dev" || ! is_installed_apt "libnetcdf-dev" \
            || ! is_installed_apt "libnetcdf-c\+\+4-dev" || ! is_installed_apt "libeccodes-dev" \
            || ! is_installed_apt "libfreetype6-dev" || ! is_installed_apt "libgsl-dev" \
            || ! is_installed_apt "libglew-dev" || ! is_installed_apt "libproj-dev"; then
        echo "------------------------"
        echo "Installing dependencies "
        echo "------------------------"
        sudo apt install -y qtbase5-dev qtchooser qt5-qmake qtbase5-dev-tools libqt5charts5-dev \
        liblog4cplus-dev libgdal-dev  libnetcdf-dev libnetcdf-c++4-dev libeccodes-dev \
        libfreetype6-dev libgsl-dev libglew-dev libproj-dev
    fi
else
    echo "Error: Unsupported system package manager detected." >&2
    exit 1
fi

if [ ! -d "$MET3D_BASE_PATH" ]; then
    mkdir -p "$MET3D_BASE_PATH"
fi
cd "$MET3D_BASE_PATH"

if [ ! -d "./local" ]; then
    mkdir "./local"
fi
if [ ! -d "./third-party" ]; then
    mkdir "./third-party"
fi
pushd third-party > /dev/null

params=()

if [ ! -d "./glfx" ]; then
    echo "------------------------"
    echo "    Downloading glfx    "
    echo "------------------------"
    git clone https://github.com/chrismile/glfx.git glfx
    pushd glfx >/dev/null
    mkdir build
    pushd build >/dev/null
    cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX:PATH="${MET3D_BASE_PATH}/local" ..
    make -j $(nproc)
    make install
    popd >/dev/null
    popd >/dev/null
fi

#if [ ! -d "./QCustomPlot" ]; then
#    git clone https://gitlab.com/DerManu/QCustomPlot.git
#    pushd QCustomPlot >/dev/null
#    ./run-amalgamate.sh
#    cp qcustomplot.h "${MET3D_BASE_PATH}/local/include/"
#    pushd sharedlib/sharedlib-compilation >/dev/null
#    qmake
#    make -j $(nproc)
#    cp libqcustomplot* "${MET3D_BASE_PATH}/local/lib/"
#    popd >/dev/null
#    popd >/dev/null
#fi

if [ ! -d "./qcustomplot" ]; then
    wget https://www.qcustomplot.com/release/2.1.0fixed/QCustomPlot.tar.gz
    wget https://www.qcustomplot.com/release/2.1.0fixed/QCustomPlot-sharedlib.tar.gz
    tar xvfz QCustomPlot.tar.gz
    tar xvfz QCustomPlot-sharedlib.tar.gz
    mv qcustomplot-sharedlib/ qcustomplot/
    pushd qcustomplot/qcustomplot-sharedlib/sharedlib-compilation >/dev/null
    qmake
    make -j $(nproc)
    cp libqcustomplot* ~/met.3d-base/local/lib/
    popd >/dev/null
    pushd qcustomplot >/dev/null
    cp qcustomplot.h ~/met.3d-base/local/include/
    popd >/dev/null
fi

if [ ! -d "./qt-solutions" ]; then
    git clone https://github.com/qtproject/qt-solutions.git
fi

if [ ! -d "./freefont-20120503" ]; then
    wget http://ftp.gnu.org/gnu/freefont/freefont-ttf-20120503.zip
    unzip freefont-ttf-20120503.zip
fi

if [ ! -d "./naturalearth" ]; then
    mkdir naturalearth
    pushd naturalearth >/dev/null
    wget http://www.naturalearthdata.com/http//www.naturalearthdata.com/download/50m/physical/ne_50m_coastline.zip
    unzip ne_50m_coastline.zip
    wget http://www.naturalearthdata.com/http//www.naturalearthdata.com/download/50m/cultural/ne_50m_admin_0_boundary_lines_land.zip
    unzip ne_50m_admin_0_boundary_lines_land.zip
    wget http://www.naturalearthdata.com/http//www.naturalearthdata.com/download/50m/raster/HYP_50M_SR_W.zip
    unzip HYP_50M_SR_W.zip
    popd >/dev/null
fi

is_embree_installed=false
os_arch="$(uname -m)"
embree_version="3.13.3"
if ! $is_embree_installed && [ $os_arch = "x86_64" ]; then
    if [ ! -d "./embree-${embree_version}.x86_64.linux" ]; then
        echo "------------------------"
        echo "   downloading Embree   "
        echo "------------------------"
        wget "https://github.com/embree/embree/releases/download/v${embree_version}/embree-${embree_version}.x86_64.linux.tar.gz"
        tar -xvzf "embree-${embree_version}.x86_64.linux.tar.gz"
    fi
    params+=(-Dembree_DIR="${MET3D_BASE_PATH}/third-party/embree-${embree_version}.x86_64.linux/lib/cmake/embree-${embree_version}")
fi

# Go back to the Met.3D base directory.
popd >/dev/null


if [ ! -d "./met.3d" ]; then
    git clone https://gitlab.com/chrismile/met.3d
    pushd met.3d >/dev/null
    git checkout multivar
    popd >/dev/null
fi

if [ ! -d "./build" ]; then
    mkdir build
fi
pushd build >/dev/null
cmake ../met.3d -DCMAKE_BUILD_TYPE=RELEASE -DUSE_STATIC_STD_LIBRARIES=On "${params[@]}"
make -j $(nproc)
popd >/dev/null
BINARY_PATH="$MET3D_BASE_PATH/build/Met3D"


# Copy the application to the destination directory.
mkdir -p $SHIPPING_DIR/bin
rsync -a "$BINARY_PATH" "$SHIPPING_DIR/bin"

# Copy all dependencies of the application to the destination directory.
ldd_output="$(ldd $BINARY_PATH)"
if ! $is_embree_installed && [ $os_arch = "x86_64" ]; then
    libembree3_so="$(readlink -f "${MET3D_BASE_PATH}/third-party/embree-${embree_version}.x86_64.linux/lib/libembree3.so")"
    ldd_output="$ldd_output $libembree3_so"
fi
library_blacklist=(
    "libOpenGL" "libGLdispatch" "libGL.so" "libGLX.so"
    "libwayland" "libX" "libxcb" "libxkbcommon"
    "ld-linux" "libdl." "libutil." "libm." "libc." "libpthread." "libbsd."
    # We build with libstdc++.so and libgcc_s.so statically. If we were to ship them, libraries opened with dlopen will
    # use our, potentially older, versions. Then, we will get errors like "version `GLIBCXX_3.4.29' not found" when
    # the Vulkan loader attempts to load a Vulkan driver that was built with a never version of libstdc++.so.
    # I tried to solve this by using "patchelf --replace-needed" to directly link to the patch version of libstdc++.so,
    # but that made no difference whatsoever for dlopen.
    # OSPRay depends on libstdc++.so, but it is hopefully a very old version available on a wide range of systems.
    "libstdc++.so" "libgcc_s.so"
)
# TODO: "libffi."

for library in $ldd_output
do
    if [[ $library != "/"* ]]; then
        continue
    fi
    is_blacklisted=false
    for blacklisted_library in ${library_blacklist[@]}; do
        if [[ "$library" == *"$blacklisted_library"* ]]; then
            is_blacklisted=true
            break
        fi
    done
    if [ $is_blacklisted = true ]; then
        continue
    fi
    cp "$library" "$SHIPPING_DIR/bin"
    patchelf --set-rpath '$ORIGIN' "$SHIPPING_DIR/bin/$(basename "$library")"
done
patchelf --set-rpath '$ORIGIN' "$SHIPPING_DIR/bin/Met3D"
if ! $is_embree_installed; then
    ln -sf "./$(basename "$libembree3_so")" "$SHIPPING_DIR/bin/libembree3.so"
fi


# /usr/lib/x86_64-linux-gnu/qt5/plugins/platforms/
# Create a run script.
printf "#!/bin/bash\nexport MET3D_HOME=/home/\$USER/met.3d-base/met.3d\nexport MET3D_BASE=/home/\$USER/met.3d-base\n" > "$SHIPPING_DIR/run.sh"
printf "pushd \"\$(dirname \"\$0\")/bin\" >/dev/null\n./Met3D\npopd >/dev/null\n" >> "$SHIPPING_DIR/run.sh"
chmod +x "$SHIPPING_DIR/run.sh"


# Run the program as the last step.
echo ""
echo "All done!"

export MET3D_HOME=/home/christoph/met.3d-base/met.3d
export MET3D_BASE=/home/christoph/met.3d-base
./Shipping/bin/Met3D
#--pipeline=config/pipeline.cfg --frontend=config/frontend.cfg
