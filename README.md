# df
this program reports disk information in terms of sizes; it is a simple 
reimplementation of well known standart unix command df.

- reports only physical disks
- all static memory (no dynamic memory allocation)
- only integer arithmetic (no floating point arithmetic)

## Usage
Currently, there is only one argument is accepted by the program.
```
df [-h|--human-readable]
```

## Installation
```sh
git clone https://github.com/gokberkgunes/df.git
make
./df
```
