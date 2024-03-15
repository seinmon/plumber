#include <cstdint>
#include <iostream>
#include <memory>
#include <vector>

#include "klee/klee.h"
#include "vscale_top.h"
#include "vscale_top__Syms.h"

void clk_forward(const std::unique_ptr<vscale_top> &, uint16_t);

int main(int argc, char** argv)
{
  std::unique_ptr<vscale_top> top {new vscale_top};

  CData ipi_req_ready {};
  CData ipi_resp_valid {};
  CData ipi_resp_data {};
  CData pcr_req_rw {};
  CData pcr_req_valid {};
  CData pcr_resp_ready {};
  SData pcr_req_addr {};
  QData pcr_req_data {};

  EData imem_hrdata {};
  CData imem_hready {};
  CData imem_hresp {};
  EData dmem_hrdata {};
  CData dmem_hready {};
  CData dmem_hresp {};

  CData reset {};
  CData id {};

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
  klee_make_symbolic(&imem_hrdata, sizeof(imem_hrdata), "imem_hrdata");
  klee_make_symbolic(&dmem_hready, sizeof(dmem_hready), "dmem_hready");
  klee_make_symbolic(&dmem_hresp, sizeof(dmem_hresp), "dmem_hresp");
  klee_make_symbolic(&dmem_hrdata, sizeof(dmem_hrdata), "dmem_hrdata");

  klee_make_symbolic(&reset, sizeof(reset), "reset");
  klee_make_symbolic(&id, sizeof(id), "id");

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

  top->htif_id = id;
  top->htif_reset = reset;

  clk_forward(top, 4);

  top.reset();
  return 0;
}

void clk_forward(const std::unique_ptr<vscale_top> &model, uint16_t clks) {
  for (auto i {0}; i < clks; i++) {
    model->clk = 0;
    model->eval();

    model->clk = 1;
    model->eval();

    auto prv = (
        model->rootp->vscale_core__DOT__pipeline__DOT__csr__DOT__priv_stack
        & 0b110);

    klee_assert("Undefined privilege level 2" && (prv != 0b100));

    klee_assert("Supervisor privilege level in not implemented"
        && (prv != 0b010));
  }
}
