# Usage of XTU-SA (Cross Translation Unit Static Analysation) scripts

## Requirements
* You need to have your to be analysed project correctly set up for the build.
* You need to create a compilation database json of your build process as this is the source of information used by all XTU scripts here.

## Process
These are the steps of XTU analysation:

1. `xtu-build.py` script uses your compilation database and extracts all necessary information from files compiled.
  It puts all its generated data into a folder (.xtu by default).
2. `xtu-analyze.py` script uses all previously generated data and executes the analysation.
  It needs the clang binary and scan-build-py's analyze-cc in order to do that.
  The output is put into a folder (.xtu-out by default)
  where both the analysation reports and the called commands' generated output is stored.

## Usage example
0. You have generated your compilation database into build.json and you are in your projects build directory
1. `xtu-build.py -b build.json -v --clang-path <path-to-folder-of-clang-binary>`
2. `xtu-analyze.py -b build.json -v --clang-path <path-to-folder-of-clang-binary> --analyze-cc-path <path-to-folder-of-analyze-cc-script>`
