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