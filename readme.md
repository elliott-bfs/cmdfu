# MDFU host application for Linux (cmdfu)

## Configuration

Creating the build tree.
```bash
cmake -B build
```

### Configuration options

- MDFU_MAX_COMMAND_DATA_LENGTH: Defines the maximum MDFU command data length that is supported. This must be at least the same size as the MDFU client reported size.
- MDFU_MAX_RESPONSE_DATA_LENGTH: Defines the maximumd MDFU response data length that is supported.

Creating the build tree and configuring maximum MDFU command data size.
```bash
cmake -B build -D MDFU_MAX_COMMAND_DATA_LENGTH=1024
```

## Building

Building by invocing native build tools through cmake
```bash
cmake --build build
```
or by using native tools e.g. for make project
```bash
cd build
make
```

## Running the application from the build tree

```bash
cd .\build\apps\cmdfu\cmdfu
./cmdfu
```

## Installation from build tree

The install script will put the application into the system folders which are included in the search path.
```bash
cd ./build
make install
```

## Creating source and binary package

Create a package after building as a separate step.
```bash
cd build
cpack
```
The source and debian package will be available in the ./build directory.

Build and packaging in one step.
```bash
cmake --build build --target package
```