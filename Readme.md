<!--
 !-- Copyright (C) 2019  Jimmy Aguilar Mena
 !--
 !-- This program is free software: you can redistribute it and/or modify
 !-- it under the terms of the GNU Lesser General Public License as published by
 !-- the Free Software Foundation, either version 3 of the License, or
 !-- (at your option) any later version.
 !--
 !-- This program is distributed in the hope that it will be useful,
 !-- but WITHOUT ANY WARRANTY; without even the implied warranty of
 !-- MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 !-- GNU General Public License for more details.
 !--
 !-- You should have received a copy of the GNU Lesser General Public License
 !-- along with this program.  If not, see <http://www.gnu.org/licenses/>.
  -->

# Readme

This is a personal fork for Qemacs. A quick and smaller version of
emacs implemented in pure C.

This fork will try to include all the functionalities in the original
[qemacs](https://bellard.org/qemacs/) project as soon as they appear;
but will add some extra functionalities from time to time. But we will
keep the original philosophy of implementing everything only in C and
Keep it simple to use and build as similar to Emacs as possible.

We will try to keep the number of external dependencies as low as
possible and the main supported system to work will be GNU/Linux
distributions; but we actually give support for the MS Windows and
[Haiku](https://www.haiku-os.org/).

## Building from sources

To compile Qemacs from sources you need to use any cmake version
higher than 3.0.

1. Clone this repository:

	```shell
	git clone https://github.com/Ergus/Qemacs.git
	```

2. Create build directory

	```shell
	mkdir Qemacs/build
	cd Qemacs/build
	```

	In this case the build directory is named `build` inside the
	Qemacs cloned repository; but you can name and set it as you
	prefer.

3. Generate the build files.

	```shell
	cmake *path_to_sources*
	```

	If you followed the steps 1 and 2 you can use the **cmake ..**
	because *build* will be inside the sources directory.

	There are some extra build options you can specify in this step using
	the cmake syntax: -DOPTION=value.

    ### Some usefull and important options are:

		* ENABLE_TINY: To build a minimal version without modules and
		external dependencies. (default: off)

		* ENABLE_X11: To try to build a graphical version of qemacs
		using X11 (depends of X11 libraries; default: on)

		* ENABLE_DOC: To build the html documentation (depends of makeinfo).
	
		* CMAKE_INSTALL_PREFIX=*path*: Path prefix to install (default: /usr/local for GNU/Linux and c:/Program Files for Windows)
		
	We use the CMake search system to look for dependencies. When the
    dependencies are not found in any of the usual locations a message
    will be shown and the related option will be disables automatically.

4. Build

	```shell
	make
	```

	After this step you will have the qemacs executable file in the
	current directory.

5. Install (optional)

	If you want to install qemacs in your system or the specified
	**CMAKE_INSTALL_PREFIX** (step 4).

	```shell
	make install
	```

## Documentation

When built with `-DENABLE_DOC=on` (the default) the compilation system will
generate a documentation file in html format in the build directory.

But also original [qemacs](https://bellard.org/qemacs/) project already
provides a useful [online
documentation](https://bellard.org/qemacs/qe-doc.html).

There will be also a manual provided with the current installation
system.

In the future we expect to improve the documentation and provide our
own online version if needed.

## Licensing

Original QEmacs is released under the GNU Lesser General Public
License. This project will keep this license.

Fabrice Bellard.
Charlie Gordon.
Jimmy Aguilar Mena
