#include <cstdint>
#include <iostream>
#include <iterator>
#include <memory>
#include <vector>

#include "vscale_top.h"
#include "vscale_top__Syms.h"

#include "binary_conc.h"
#include "mem.h"

void clk_forward(const std::unique_ptr<vscale_top> &, uint16_t,
    std::unique_ptr<Mem> &, std::unique_ptr<Mem> &);
void update_data_memory(const std::unique_ptr<vscale_top> &,
    std::unique_ptr<Mem> &);

int main(int argc, char** argv)
{
  std::unique_ptr<Mem> imem {new Mem {"imem"}};
  std::unique_ptr<Mem> dmem {new Mem {"dmem"}};
  std::unique_ptr<vscale_top> top {new vscale_top};

  // Write the program to imem
  for (auto i = 0; i < testcase.size(); i++) {
    std::cout << testcase[i] << std::endl;
    auto imem_addr = (i * 4) + 4;
    imem->write(imem_addr, testcase[i]);
  }

  top->htif_id = 0;
  top->htif_reset = 0;

  top->htif_ipi_req_ready = 0;
  top->htif_ipi_resp_valid = 0;
  top->htif_ipi_resp_data = 0;
  top->htif_pcr_req_rw = 0;
  top->htif_pcr_req_valid = 0;
  top->htif_pcr_resp_ready = 0;
  top->htif_pcr_req_addr = 0;
  top->htif_pcr_req_data = 0;

  top->imem_hrdata = 0;
  top->imem_hready = 1;
  top->imem_hresp = 0;
  top->dmem_hrdata = 0;
  top->dmem_hready = 1;
  top->dmem_hresp = 0;

  clk_forward(top, 4, dmem, imem);

  std::cout << "mepc: " <<
    std::bitset<32>(top->rootp->vscale_core__DOT__pipeline__DOT__csr__DOT__mepc) << std::endl;

  std::cout << "priv_stack: " <<
    std::bitset<32>(top->rootp->vscale_core__DOT__pipeline__DOT__csr__DOT__priv_stack) << std::endl;

  std::cout << "x1: " <<
    std::bitset<32>(top->rootp->vscale_core__DOT__pipeline__DOT__regfile__DOT__data[1]) << std::endl;

  std::cout << "x2: " <<
    std::bitset<32>(top->rootp->vscale_core__DOT__pipeline__DOT__regfile__DOT__data[2]) << std::endl;

  top.reset();
  return 0;
}

void clk_forward(const std::unique_ptr<vscale_top> &model,
    uint16_t clks, std::unique_ptr<Mem> &dmem, std::unique_ptr<Mem> &imem) {
  for (auto i {0}; i < clks; i++) {
    model->clk = 0;
    model->eval();

    auto instruction = imem->read(model->imem_haddr);
    if (instruction) {
      model->imem_hrdata = instruction;
      model->dmem_hrdata = dmem->read(model->dmem_haddr);
    }
    update_data_memory(model, dmem);

    model->clk = 1;
    model->eval();

    instruction = imem->read(model->imem_haddr);
    if (instruction) {
      model->imem_hrdata = instruction;
      model->dmem_hrdata = dmem->read(model->dmem_haddr);
    }
    update_data_memory(model, dmem);

    std::cout << "(m)mepc: " <<
      std::bitset<32>(model->rootp->vscale_core__DOT__pipeline__DOT__csr__DOT__mepc) << std::endl;

    std::cout << "(m)priv_stack: " <<
      std::bitset<32>(model->rootp->vscale_core__DOT__pipeline__DOT__csr__DOT__priv_stack) << std::endl;

    std::cout << "(m)x1: " <<
      std::bitset<32>(model->rootp->vscale_core__DOT__pipeline__DOT__regfile__DOT__data[1]) << std::endl;

    std::cout << "(m)x2: " <<
      std::bitset<32>(model->rootp->vscale_core__DOT__pipeline__DOT__regfile__DOT__data[2]) << std::endl;
  }
}

void update_data_memory(const std::unique_ptr<vscale_top> &model,
    std::unique_ptr<Mem> &dmem) {
  if (model->dmem_hwrite) {
    dmem->write(model->dmem_haddr, model->dmem_hwdata);
  }
}
