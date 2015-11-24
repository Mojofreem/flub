#!/bin/bash
# TODO: Obtain working path from CMake, instead of assuming an in
#       tree build.

GENERATED_BINS=("flubtest" "themer" "stbfont")
CMAKE_REVERTS=()
CMAKE_ARTIFACTS=("CMakeCache.txt" "Makefile" "cmake_install.cmake" "flub/flubconfig.h")
CMAKE_WORKDIRS=("CMakeFiles" "resources")

STASH_EXT=".stash"

function stash_bins {
	for file in "${GENERATED_BINS[@]}"; do
		if [ -f "${file}" ]; then
			cp -f ${file} ${file}${STASH_EXT}
		fi
	done	
}

function unstash_bins {
	for file in "${GENERATED_BINS[@]}"; do
		if [ ! -f "${file}${STASH_EXT}" ]; then
			continue
		fi
		cp -f ${file}${STASH_EXT} ${file}
		rm -f ${file}${STASH_EXT}
	done	
}

function clear_cmake_artifacts {
	for file in "${CMAKE_ARTIFACTS[@]}"; do
		rm -f ${file}
	done
	for path in "${CMAKE_WORKDIRS[@]}"; do
		rm -rf ${path}
	done
	for file in "${CMAKE_REVERTS[@]}"; do
		git checkout -- ${file}
	done
}

function clear_bins {
	for file in "${GENERATED_BINS[@]}"; do
		if [ -f "${file}" ]; then
			rm -f ${file}
		fi
	done	
}

if [ "$1" == "help" ]; then
	echo "clean.sh <option>"
	echo
	echo "    <none> - remove cmake artifacts, build artifacts, and generated"
	echo "             binaries."
	echo "    build - remove the build artifacts and generated binaries"
	echo "    keepbin - remove only the cmake and build artifacts"
	echo "    help - show this message"
	return
elif [ "$1" == "stash" ]; then
	stash_bins
	echo "-= Stashed =-"
elif [ "$1" == "unstash" ]; then
	unstash_bins
	echo "-= Unstashed =-"
elif [ "$1" == "build" ]; then
	if [ -f "Makefile" ]; then
		make clean
	fi
	clear_bins
elif [ "$1" == "keepbin" ]; then
	if [ -f "Makefile" ]; then
		stash_bins
		make clean
		unstash_bins
		clear_cmake_artifacts
	fi
elif [ -z "$1" ]; then
	if [ -f "Makefile" ]; then
		make clean
	fi
	clear_bins
	clear_cmake_artifacts
else
	echo "ERROR: Unknown command \"${1}\""
fi
