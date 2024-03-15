
#include "mem.h"

#include <iostream>

Mem::Mem(std::string identifier): id {identifier} {}

void Mem::write(BUS_WIDTH addr, BUS_WIDTH data) {
  memory[addr] = data;
}

BUS_WIDTH Mem::read(BUS_WIDTH addr) {
  BUS_WIDTH data {0};

  if (memory.find(addr) != memory.end()) {
    data = memory[addr];
  }

  return data;
}
