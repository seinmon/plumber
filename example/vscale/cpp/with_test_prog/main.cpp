#include <cstdint>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <vector>
#include <chrono>

#include "klee/klee.h"
#include "vscale_top.h"
#include "vscale_top__Syms.h"

#include "binary.h"
#include "mem.h"

void clk_forward(const std::unique_ptr<vscale_top> &, uint16_t,
    std::unique_ptr<Mem> &, std::unique_ptr<Mem> &,
    unsigned int &subscription);
void update_data_memory(const std::unique_ptr<vscale_top> &,
    std::unique_ptr<Mem> &);

int main(int argc, char** argv)
{
  std::unique_ptr<Mem> imem {new Mem {"imem"}};
  std::unique_ptr<Mem> dmem {new Mem {"dmem"}};
  std::unique_ptr<vscale_top> top {new vscale_top};

  IData subscription {0};

  CData ipi_req_ready {};
  CData ipi_resp_valid {};
  CData ipi_resp_data {};
  CData pcr_req_rw {};
  CData pcr_req_valid {};
  CData pcr_resp_ready {};
  SData pcr_req_addr {};
  QData pcr_req_data {};

  CData reset {};
  CData id {};

  EData imem_hrdata {};
  CData imem_hready {};
  CData imem_hresp {};
  EData dmem_hrdata {};
  CData dmem_hready {};
  CData dmem_hresp {};


  for (auto i = 0; i < testcase.size(); i++) {
    std::cout << testcase[i] << std::endl;
    auto imem_addr = (i * 4) + 4;
    imem->write(imem_addr, testcase[i]);
  }

  klee_make_symbolic(&ipi_req_ready, sizeof(ipi_req_ready), "ipi_req_ready");
  klee_make_symbolic(&ipi_resp_valid, sizeof(ipi_resp_valid),
      "ipi_resp_valid");
  klee_make_symbolic(&ipi_resp_data, sizeof(ipi_resp_data), "ipi_resp_data");
  klee_make_symbolic(&pcr_req_rw, sizeof(pcr_req_rw), "pcr_req_rw");
  klee_make_symbolic(&pcr_req_valid, sizeof(pcr_req_valid), "pcr_req_valid");
  klee_make_symbolic(&pcr_resp_ready, sizeof(pcr_resp_ready),
      "pcr_resp_ready");
  klee_make_symbolic(&pcr_req_addr, sizeof(pcr_req_addr), "pcr_req_addr");
  klee_make_symbolic(&pcr_req_data, sizeof(pcr_req_data), "pcr_req_data");

  klee_make_symbolic(&imem_hready, sizeof(imem_hready), "imem_hready");
  klee_make_symbolic(&imem_hresp, sizeof(imem_hresp), "imem_hresp");
  klee_make_symbolic(&dmem_hready, sizeof(dmem_hready), "dmem_hready");
  klee_make_symbolic(&dmem_hresp, sizeof(dmem_hresp), "dmem_hresp");

  klee_make_symbolic(&reset, sizeof(reset), "reset");
  klee_make_symbolic(&id, sizeof(id), "id");

  klee_subscribe(&subscription , "Subscription", Change);
  subscription = 0;

  top->htif_id = id;
  top->htif_reset = reset;

  top->htif_ipi_req_ready = ipi_req_ready;
  top->htif_ipi_resp_valid = ipi_resp_valid;
  top->htif_ipi_resp_data = ipi_resp_data;
  top->htif_pcr_req_rw = pcr_req_rw;
  top->htif_pcr_req_valid = pcr_req_valid;
  top->htif_pcr_resp_ready = pcr_resp_ready;
  top->htif_pcr_req_addr = pcr_req_addr;
  top->htif_pcr_req_data = pcr_req_data;

  top->imem_hrdata = imem_hrdata;
  top->imem_hready = imem_hready;
  top->imem_hresp = imem_hresp;
  top->dmem_hrdata = dmem_hrdata;
  top->dmem_hready = dmem_hready;
  top->dmem_hresp = dmem_hresp;

  clk_forward(top, 4, dmem, imem, subscription);

  top.reset();
  return 0;
}

void clk_forward(const std::unique_ptr<vscale_top> &model,
    uint16_t clks, std::unique_ptr<Mem> &dmem, std::unique_ptr<Mem> &imem,
    unsigned int &subscription) {
  for (auto i {0}; i < clks; i++) {
    model->clk = 0;
    model->eval();

    auto instruction = imem->read(model->imem_haddr);
    if (instruction) {
      model->imem_hrdata = instruction;
      model->dmem_hrdata = dmem->read(model->dmem_haddr);
    }
    update_data_memory(model, dmem);

    subscription = model->rootp->vscale_core__DOT__pipeline__DOT__csr__DOT__mepc;
      /* & 0b110; */

    model->clk = 1;
    model->eval();

    if (instruction) {
      model->imem_hrdata = instruction;
      model->dmem_hrdata = dmem->read(model->dmem_haddr);
    }
    update_data_memory(model, dmem);

    subscription = model->rootp->vscale_core__DOT__pipeline__DOT__csr__DOT__mepc;
      /* & 0b110; */

    klee_assert("Undefined privilege level 2"
        && ((model->rootp->vscale_core__DOT__pipeline__DOT__csr__DOT__priv_stack & 0b110)
          != 0b100));
  }
}

void update_data_memory(const std::unique_ptr<vscale_top> &model,
    std::unique_ptr<Mem> &dmem) {
  if (model->dmem_hwrite) {
    dmem->write(model->dmem_haddr, model->dmem_hwdata);
  }
}
