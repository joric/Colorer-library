## [1.3.0] - 2021-XX-XX

### Added

- Add work with symlinks. If file is symlink, for example catalog.xml, we work with real file and full path for it ([#10](https://github.com/colorer/Colorer-library/issues/10))
- Add work with system environments in path to files

### Changed

- In base/hrc/auto folder (and same in catalog.xml) search only *.hrc files ([#20](https://github.com/colorer/Colorer-library/issues/20))

## [1.2.1] - 2021-04-03

First auto built release

## [1.2.0] - 2021-09-09

### Changed

- Work with redirect stdin as input file, detect charset if set BOM ([#8](https://github.com/colorer/Colorer-library/issues/8))

### Fixed

- Fix crash on invalid zip file name ([#15](https://github.com/colorer/Colorer-library/issues/15))
- colorertools: fix crash if did not detect file type and did not input filename

## [1.1.1] - 2021-02-28

### Fixed

- Fix output log messages to stdout if log level equal 'off' ([#14](https://github.com/colorer/Colorer-library/issues/14))

### Changed

- Set default log level value to 'off'

## [1.1.0] - 2021-02-28

### Changed

- Change license to MIT
- Change internal Unicode library to ICU
- Get dependencies for build from vcpkg

### Added

- regexp: add support ignorecase for symbol class
- add new property for TextParser - maximum block size of regexp in string line

### Fixed

- Fix error when searching for the missing type
- Fix duplicate entries in outliner https://github.com/colorer/FarColorer/issues/25

### Removed

- Remove dlmalloc library
- Remove working with different charsets. Input files only in Unicode, output only in UTF-8.

## [1.0.4] - 2019-01-29

### Changed

- Update xerces-c
- Update zlib
- Add spdlog
- Remove g3log

## [1.0.3] - 2018-12-09

### Fixed

- Fixed incorrect file type detection

## [1.0.2] - 2016-06-18

### Changed

- optimized

## [1.0.1] - 2016-02-06

### Changed

- logging default off
- add settings for logging

## [1.0.0] - 2016-01-23

### Changed

- add version info
- add asynchronous logging

