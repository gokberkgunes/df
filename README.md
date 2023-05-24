# df
this program reports disk information in terms of sizes; it is a simple and
small reimplementation of well known sstandart unix command df.

df tries to only report sata and nvme disks to the user

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
