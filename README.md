# llvm-brainfuck

Having good performance is crucial and brainfuck is not an exception for that.  
Compile brainfuck to an executable and get rid of days being wasted just for interpreting.

## How to build

### Prerequisite

* LLVM 10
* Clang (preferably 10?) or GCC

### Build command

```bash
g++ llvm-brainfuck.cc -o llvm-brainfuck `llvm-config --cxxflags --ldflags --libs`
```


## Usage

### Compile brainfuck script

```bash
# Translate brainfuck to LLVM IR
./llvm-brainfuck helloworld.bf > helloworld.ll
# Compile LLVM IR to assembly
llc helloworld.ll -o helloworld.s
# Generate executable using clang or gcc
clang helloworld.s -o helloworld
gcc helloworld.s -o helloworld
# Run
./helloworld
```

### Interpret brainfuck (If you are lazy running all the commands)

```bash
# Translate brainfuck to LLVM IR and pass to LLVM interpreter
./llvm-brainfuck helloworld.bf | lli
```

## Note

### Error when using GCC

If GCC complains about PIE stuff, use ```-relocation-model=pic``` option in ```llc```:
```bash
llc helloworld.ll -relocation-model=pic -o helloworld.s
```
### Vulnerability

This brainfuck compiler has a vulnerability that probably can lead to ACE if you run malcrafted brainfuck script.  
Be careful about running exotic brainfuck script.

### Improve performance

The performance can be improved by using LLVM mem2reg pass:

```bash
opt -mem2reg helloworld.ll -o helloworld.bc
```

Then do the ```llc``` command and rest.
