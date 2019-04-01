#!/bin/bash

build_target="x86_64"
build_mode="release"
source_dir=`pwd`"/src"
configure_options=""
# default build_dir = ./build_${build_target}_${build_mode}"
build_dir=""
prefix_dir=""

product_name="cubrid-backup-api"
version=""

function show_usage ()
{
    echo "Usage: $0 [OPTIONS]"
    echo " OPTIONS"
    echo "  -m      Set build mode(release or debug); [default: release]"
    echo ""
    echo " EXAMPLES"
    echo "  $0              # Create 64bit release mode packages"
    echo "  $0 -m debug     # Create 64bit debug mode packages"
    echo ""
}

function get_options ()
{
    while getopts ":m:" opt; do
        case $opt in
            m ) build_mode="$OPTARG" ;;
            * ) show_usage; exit 1 ;;
        esac
    done

    if [ $build_mode != "release" -a $build_mode != "debug" ]; then
        show_usage; exit 1
    fi

    if [ "x$build_dir" = "x" ]; then
        build_dir=`pwd`"/build_${build_target}_${build_mode}"
    fi

    if [ "x$prefix_dir" = "x" ]; then
        prefix_dir="$build_dir/_install/$product_name"
    fi
}

function build_clean ()
{
    git clean -ffdx
}

function build_configure ()
{
    if [ -f VERSION ]; then
        version=$(cat VERSION)
    fi

    if [ ! -d $source_dir ]; then
        exit 1
    fi

    if [ ! -d $build_dir ]; then
        mkdir -p $build_dir
    fi

    configure_prefix="-DCMAKE_INSTALL_PREFIX=$prefix_dir"

    case $build_mode in
        release )
            configure_options="$configure_options -DCMAKE_BUILD_TYPE=RelWithDebInfo" ;;
        debug )
            configure_options="$configure_options -DCMAKE_BUILD_TYPE=Debug" ;;
        * )
            exit 1 ;;
    esac

    cmake -E chdir $build_dir cmake $configure_prefix $configure_options $source_dir
}

function build_compile ()
{
    cmake --build $build_dir
}

function build_install ()
{
    cmake --build $build_dir --target install
}

function build_package ()
{
    package_basename="$product_name-$version-Linux.$build_target"

    if [ ! "$build_mode" = "release" ]; then
        package_basename="$package_basename-$build_mode"
    fi

    package_name="$package_basename.tar.gz"
    
    (cd $build_dir && tar cfz $package_name -C $build_dir/_install $product_name)
}

function build_build ()
{
    build_configure && build_compile && build_install && build_package
}

# main
{
    get_options $@
} &&
{
    build_clean && build_build
}
