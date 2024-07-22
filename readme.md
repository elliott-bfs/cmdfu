## Building

## CMAKE

When linking libraries the sequence can be important for CMAKE. For example this application link configuration `target_link_libraries(mdfu mdfulib toolslib transportlib maclib utilslib)`, where `maclib` might depend on `utilslib`, it is necessary that `utilslib` is listed after `maclib`.


## VSCode C configuration

This project uses some defines that were introduced with the C11 standard and some that take advantage of the glibc library. For the intellisense in VSCode it is necessary to specify these requirements in the `cStandard` setting.
```json
            "cStandard": "gnu11",
```
## Include paths for header files

CMAKE generates a file called `compile_commands.json` in the build directory that includes the individual commands to build each object in the project. If this file is located in the project root (default build directory) VS Code would pick it up automatically but in a better project setup a specific build directory might be used, and then VS Code C/C++ extension must be told where to find this file. This can be done in the `c_cpp_properties.json`.

```json
            "compileCommands": "${workspaceFolder}/build/compile_commands.json",
```

## Linux kernel header files for WSL

On WSL some user space headers are not installed e.g. headers required for the I2C subsystem interface. In a usual Linux environment the headers can be installed through the distribution packaging system but for the WSL kernel there is no such package. A manual way to get the headers created is to first clone the WSL kernel repository https://github.com/microsoft/WSL2-Linux-Kernel.
`git clone --depth 1 https://github.com/microsoft/WSL2-Linux-Kernel.git`
No full history is needed here so to save some space and time downloading we reduce history with `--depth 1`.
Make sure that the branch matches the WSL kernel version.

```bash
$ git branch -l
* linux-msft-wsl-5.15.y
$ uname -r
5.15.146.1-microsoft-standard-WSL2
```

The next step is to configure the kernel to export the headers and for this some tools are required (Complete list of tools required for building the kernel https://www.kernel.org/doc/html/latest/process/changes.html). Most of these tools are probably already installed in WSL so a trial and error could identify the rest. For configuring the kernel with `menuconfig` on Ubuntu the following packages had to be installed.

```bash
$ apt-get install bison flex libncurses-dev
```

With the requird packages installed run
```bash
$ make KCONFIG_CONFIG=Microsoft/config-wsl menuconfig
```
to start the configuration based on the config file from Microsoft. In the configuration menu the config name `CONFIG_HEADERS_INSTALL` nees to be set and this is located under
```
Linux Kernel Configuration
└─>Kernel hacking
    └─>Compile-time checks and compiler options
        └─>Install uapi headers to usr/include
```

With the kernel configured the headers can now be generated.
```bash
$ make headers_install INSTALL_HDR_PATH=<my-linux-header-include-path>
```

The `INSTALL_HDR_DIR_PATH` defines where the headers should be installed and if not provided it will try to install it in the system path, on my Ubuntu WSL this was `/usr/include/`. If installed in the system then VS Code will pick it up automatically in a C/C++ project. Otherwise the path has to be added in the C/C++ properties json file.

After saving your configuration, it's a good idea to reload the VS Code window to ensure the new settings are applied. You can do this by opening the Command Palette (`Ctrl+Shift+P`) and typing "Reload Window".
```json
{
    "configurations": [
        {
            "name": "Linux-WSL",
            "includePath": [
                "${workspaceFolder}/**",
                "/new/location/of/wsl/kernel/headers/**"
            ],
            "defines": [],
            "compilerPath": "/usr/bin/gcc",
            "cStandard": "gnu11",
            "cppStandard": "gnu++14",
            "intelliSenseMode": "linux-gcc-x64"
        }
    ],
    "version": 4
}
```

## Raspberry Pi setup

```bash
$ apt install raspberrypi-kernel raspberrypi-kernel-headers
```

## Test

### Ceedling Installation local
Install ruby - Note that as of this writing, 25.10.2023, ceedling (gem 0.31.1) did not work with ruby 3.2.x or higher.

Windows installer:
https://rubyinstaller.org/

Linux (Debian/Ubuntu etc.)
```bash
sudo apt-get install ruby
```

Install ceedling gem
```bash
sudo gem install ceedling
```

In project directory run
```bash
ceedling new test
```
This will create a test directory containing ceedling test framework for the project.

### Running ceedling locally

To execute all tests run the following command in the test directory
```
ceedling test
```
The 

To a single test use
```
ceedling test:<test name or source file name>
```
e.g. `ceedling test:mdfu_client_info.c` or `ceedling test:mdfu_client_info`

### Running ceedling in docker

```bash
docker pull throwtheswitch/madsciencelab
docker run -it --rm -v /mnt/z/git/projectx:/project throwtheswitch/madsciencelab
```

`/mnt/z/git/projectx` is the directory that should be made available in the docker instance as filesystem.

`project` will be the name for the current directory in the docker image file system. Might as well just set this as root dir e.g. `/`.

### Configuration

project.yml

```
|-- project_root
    |
    |-- test
        |-- test
            |--support
        |-- project.yml
    |-- build
    |-- src
```

```yaml
:module_generator:
:project_root: ./
:source_root: ../
:test_root: ./test/
```