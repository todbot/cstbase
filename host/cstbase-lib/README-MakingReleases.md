Making Releases of cstbase-tool
==============================

[this is mostly notes to Tod and other maintainers on how to do new releases]

Steps when a new release of cstbase-tool (and cstbase-lib) is to be made

Assumptions
------------

- Target platforms are primarily:
  - Mac OS X 10.6+
  - Windows XP+
  - Linux 32-bit - Ubuntu (currently 12.10)
  - FreeBSD 8.2
  - 
- Build primarily 32-bit, not 64-bit
- Unix command-line build tools available for each platform
- Shared mounted checkout of github.com/todbot/ cstbase repo,
   or at least same revision checkout

General Process
---------------

1. cd to cstbase/commmandline
2. Build code with "make clean && make"
3. Package up zipfile with "make package"
4. Copy zip package to and test on separate test systems
5. Publish zip package to github release and thingm.com/cstbase/downloads


Example
-------

% cd cstbase/commandline
% make clean && make


