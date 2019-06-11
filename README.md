
# Variability-aware Soufflé

(This project is forked from [Soufflé](https://github.com/souffle-lang/souffle))

Variability-aware Soufflé extends the Soufflé Datalog engine with presence conditions on facts.

## Build Instructions (for Linux)

1.   Make sure Soufflé build dependencies are installed
```
$ sudo apt-get install autoconf automake bison build-essential doxygen flex g++ git libncurses5-dev libsqlite3-dev libtool make mcpp pkg-config python zlib1g-dev
```
2.   Download CUDD from [here](http://davidkebo.com/source/cudd_versions/cudd-3.0.0.tar.gz) and uncompress it.

3.   Build and Install CUDD:
```
$ cd cudd-3.0.0
$ ./configure --enable-shared --enable-dddmp --enable-obj
$ make
$ make check
$ sudo make install
```
4.   Download Variability-aware Soufflé sources 
```
$ git clone git://github.com/ramyshahin/souffle.git
```
5.   Create a build directory, configure and build
```
$ cd souffle
$ sh ./bootstrap
$ ./configure
$ make
$ sudo make install
```

6.  Some of the unit tests are currently broken. If you still want to run them:
```
$ make check
````

Detailed information on building CUDD can be found [here](http://davidkebo.com/cudd)

Detailed information on building Soufflé can be found [here](http://souffle-lang.org/docs/build)
