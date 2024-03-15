#ifndef _SYDEX_SRAM_H
#include <map>
#include <string>

#ifdef B64
#define BUS_WIDTH int64_t
#else
#define BUS_WIDTH int32_t
#endif

class Mem {
  public:
    Mem(std::string identifier);
    void write(BUS_WIDTH addr, BUS_WIDTH data);
    BUS_WIDTH read(BUS_WIDTH addr);

  private:
    const std::string id;
    std::map<BUS_WIDTH, BUS_WIDTH> memory {};
};

#endif // _SYDEX_SRAM_H
