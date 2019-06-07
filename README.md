
# Variability-aware Soufflé

(This project is forked from [Soufflé](https://github.com/souffle-lang/souffle)

Variability-aware Soufflé extends the Soufflé Datalog engine with presence conditions on facts.

## Build Instructions (for Linux)

*   Make sure Soufflé build dependencies are installed

    $ sudo apt-get install autoconf automake bison build-essential doxygen flex g++ git libncurses5-dev libsqlite3-dev libtool make mcpp pkg-config python zlib1g-dev

*   TODO: download CUDD

*   TODO: build CUDD

*   Download Variability-aware Soufflé sources 

    $ git clone git://github.com/ramyshahin/souffle.git

*   Create a build directory, configure and build

    $ cd souffle
    $ sh ./bootstrap
    $ ./configure
    $ make
    $ make install

    Some of the unit tests are currently broken. If you still want to run them:

    $ make check

Detailed information on building CUDD can be found [here]()
Detailed information on building Soufflé can be found [here](http://souffle-lang.org/docs/build)
