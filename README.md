# Columbo
--------

Columbo is a library that allows users to easily write small C++ programs to create Low-Level End-to-End System-Traces through 
SimBricks simulations. As such it's intended to be used alongside SimBricks. For more details see https://simbricks.github.io/.

## Building from Source
Initially, you need to ensure that the submodules required for building Columbo are initialized. 
To do so run `git submodule update --init --recursive`. Besides that, Columbo requires a C++20 compatible compiler. Currently, 
only Clang++ and G++ are supported.

From here, you can invoke CMake to generate build files for the project. To do so run the following from the repository's root.
```shell
cmake -DCMAKE_BUILD_TYPE=<build_type> -DWITH_OTLP_GRPC=OFF -DWITH_EXAMPLES_HTTP=ON -DWITH_OTLP_HTTP=ON -DBUILD_TESTING=OFF -G Ninja -S . -B build/<build_subdir>
```

In case you want to build Columbo in debug mode choose `<build_type> = Debug` or for release mode `<build_type> = Release`.
To define the subdirectory to place the build-files in, choose e.g. `<build_subdir> = debug` or `<build_subdir> = release`.

As `<generator>` you can pass any generator CMake supports and that is installed on your system.

By default, Columbo makes use of ccache if installed. You can disable that by passing `-ENABLE_CCACHE=OFF` to CMake. To 
make use of the `-march=native` compiler flag one can also additionally pass `-DNATIVE=ON` to CMake when generating the build-files.

Once the build files are generated, one can simply run `cmake --build build/<build_subdir> --target all -j <number-cores>` to build all Columbo targets.
Replace `<number-cores>` by the number of cores to use. 

## Docker (Compose)
TODO

## Folder structure
- `configs/`: Contains configuration files for tools that are intended to be used alongside Columbo as well as for Columbo itself.
- `docker-compose`: Contains a Dockerfile for compiling Columbo and Docker Compose files to allow easily running SimBricks, Columbo, as well as external tools like Jaeger simultaneously.
- `include`: Columbos header files.
- `libs`: Libraries i.e. submodules Columbo depends on.
- `source`: Columbo's source/implementation files.
- `tests`: Unittests.
