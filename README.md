# Shifting Hash Table

### Introduction

Hash tables have been widely used in data storage systems, because of their high query speed. However, the query speed degrades when hash collisions happen. The shifting hash table (SHT) is an implement of hash tables based on the two-stage architecture, and it can achieve high load factor, low hash collision rate, fast query and update speed at the same time.
This repository is the source code of the prototype of SHT. Both stages are implemented on CPU.

### How to build it

There is a little example in `main.c`, which shows the basic usage of SHT. The project is based on [CMake](https://cmake.org/). Run `$ cmake .; make` to build, and the executable binary file will be in `./bin`.
