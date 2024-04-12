# Columbo
--------
TODO

## Building from Source
Initially you need to ensure that the submodules required for building Columbo are initialized. 
To do so run `git submodule update --init --recursive`. Besides, does Columbo require a C++20 compatible compiler. Currently 
only Clang++ and G++ are supported.

From there one you can invoke cmake to generate build-files for the project. To do so run
`cmake -DCMAKE_BUILD_TYPE=<build_type> -DWITH_OTLP_GRPC=OFF -DWITH_EXAMPLES_HTTP=ON -DWITH_OTLP_HTTP=ON -DBUILD_TESTING=OFF -G Ninja -S . -B /build/<build_subdir>`.

In case you want to build Columbo in debug mode choose `<build_type> = Debug` or for release mode `<build_type> = Release`.
To define the subdirectory to place the build-files in choose e.g. `<build_subdir> = debug` or `<build_subdir> = release`.

As `<generator>` you can pass any generator cmake supports and that is installed on the system your building on.

By default, Columbo makes use of ccache if installed. You can disable that by passing `-ENABLE_CCACHE=OFF` to cmake. To 
make use of the `-march=native` compiler flag one can also additionally pass `-DNATIVE=ON` to cmake when generating the 
build-files.

Once the build files are generated one can simply run `cmake --build /build/<build_subdir> --target all -j <number-cores>` 
to build all Columbo targets. Replace `<number-cores>` by the number of cores to use. 

## Docker (Compose)
TODO
