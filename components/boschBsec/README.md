# Bosch BSEC Component

[BSEC](https://www.bosch-sensortec.com/bst/products/all_products/bsec) (Bosch Sensortec
Environmental Cluster) is a proprietary library which is used to process the data provided by a
BME680 environmental sensor to derive more useful results.

The library published at the URL above cannot be used in a Legato component because the code is not
relocatable and Legato components are built as shared libraries.  This issue was raised with Bosch
in a [forum post](https://community.bosch-sensortec.com/t5/MEMS-sensors-forum/BSEC-integration-problem-on-gcc-Cortex-A7/m-p/5995)
in which Bosch provides an alternative download compiled with the options necessary to make the
library relocatable.

To build this component, download and extract the zip file from the forum post and then create an
enironment variable `$BSEC_DIR` which points to the directory containing the `.a` file.  For
example: `export BSEC_DIR=~/BSEC_1.4.7.2_GCC_CortexA7_20190225/algo/bin/Normal_version/Cortex_A7`
