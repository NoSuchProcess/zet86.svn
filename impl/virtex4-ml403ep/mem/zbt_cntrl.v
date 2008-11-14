`timescale 1ns/10ps

`include "defines.v"

module zbt_cntrl (
`ifdef DEBUG
    output reg [4:0] cnt,
    output           op,
`endif

    // Wishbone slave interface
    input             wb_clk_i,
    input             wb_rst_i,
    input      [15:0] wb_dat_i,
    output reg [15:0] wb_dat_o,
    input      [19:1] wb_adr_i,
    input             wb_we_i,
    input      [ 1:0] wb_sel_i,
    input             wb_stb_i,
    input             wb_cyc_i,
    output reg        wb_ack_o,

    // Pad signals
    output            sram_clk_,
    output reg [20:0] sram_addr_,
    inout      [31:0] sram_data_,
    output reg        sram_we_n_,
    output reg [ 3:0] sram_bw_,
    output reg        sram_cen_,
    output            sram_adv_ld_n_
  );

  // Registers and nets
  reg  [31:0] wr;
  wire        nload;

`ifndef DEBUG
  reg [ 4:0] cnt;
  wire       op;
`endif

  // Continuous assignments
  assign op   = wb_stb_i & wb_cyc_i;
  assign nload = (|cnt || wb_ack_o);

  assign sram_clk_      = !wb_clk_i;
  assign sram_adv_ld_n_ = 1'b0;
  assign sram_data_     = (op && wb_we_i) ? wr : 32'hzzzzzzzz;

  // Behaviour
  // cnt
  always @(posedge wb_clk_i)
    cnt <= wb_rst_i ? 3'b0
         : { cnt[3:0], nload ? 1'b0 : op };

  // wb_ack_o
  always @(posedge wb_clk_i)
    wb_ack_o <= wb_rst_i ? 1'b0 : (wb_ack_o ? op : cnt[3]);

  // wb_dat_o
  always @(posedge wb_clk_i)
    wb_dat_o <= cnt[3] ? (wb_adr_i[1] ? sram_data_[31:16]
                                      : sram_data_[15:0]) : wb_dat_o;

  // sram_addr_
  always @(posedge wb_clk_i)
    sram_addr_ <= op ? { 3'b0, wb_adr_i[19:2] } : sram_addr_;

  // sram_we_n_
  always @(posedge wb_clk_i)
    sram_we_n_ <= wb_we_i ? (cnt[1] ? !op : 1'b1) : 1'b1;

  // sram_bw_
  always @(posedge wb_clk_i)
    sram_bw_ <= wb_adr_i[1] ? { ~wb_sel_i, 2'b11 }
                            : { 2'b11, ~wb_sel_i };

  // sram_cen_
  always @(posedge wb_clk_i)
    sram_cen_ <= wb_rst_i ? 1'b1
      : (cnt[0] ? 1'b0 : (cnt[4] ? 1'b1 : sram_cen_));

  // wr
  always @(posedge wb_clk_i)
    wr <= op ? (wb_adr_i[1] ? { wb_dat_i, 16'h0 }
                            : { 16'h0, wb_dat_i }) : wr;
endmodule
