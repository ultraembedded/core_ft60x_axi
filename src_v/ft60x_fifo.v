//-----------------------------------------------------------------
//          FTDI FT601 SuperSpeed USB3.0 to AXI bus master
//                              V0.1
//                        Ultra-Embedded.com
//                         Copyright 2020
//
//                 Email: admin@ultra-embedded.com
//
//                          License: GPL
//-----------------------------------------------------------------
//
// This source code is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This source code is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this source code.  If not, see <https://www.gnu.org/licenses/>.
//
//-----------------------------------------------------------------

module ft60x_fifo
(
    // Inputs
     input           clk_i
    ,input           rst_i
    ,input           ftdi_rxf_i
    ,input           ftdi_txe_i
    ,input  [ 31:0]  ftdi_data_in_i
    ,input  [  3:0]  ftdi_be_in_i
    ,input           inport_valid_i
    ,input  [ 31:0]  inport_data_i
    ,input           outport_accept_i

    // Outputs
    ,output          ftdi_wrn_o
    ,output          ftdi_rdn_o
    ,output          ftdi_oen_o
    ,output [ 31:0]  ftdi_data_out_o
    ,output [  3:0]  ftdi_be_out_o
    ,output          inport_accept_o
    ,output          outport_valid_o
    ,output [ 31:0]  outport_data_o
);



//-----------------------------------------------------------------
// Rx RAM: FT60x -> Target
//-----------------------------------------------------------------
reg  [10:0] rx_wr_ptr_q;
wire [10:0] rx_wr_ptr_next_w = rx_wr_ptr_q + 11'd1;
reg  [10:0] rx_rd_ptr_q;
wire [10:0] rx_rd_ptr_next_w = rx_rd_ptr_q + 11'd1;

// Retime input data
reg  [31:0] rd_data_q;
reg         rd_valid_q;
wire        rx_valid_w;

always @ (posedge clk_i or posedge rst_i)
if (rst_i)
    rd_valid_q  <= 1'b0;
else
    rd_valid_q  <= rx_valid_w;

always @ (posedge clk_i or posedge rst_i)
if (rst_i)
    rd_data_q  <= 32'b0;
else
    rd_data_q  <= ftdi_data_in_i;

// Write pointer
always @ (posedge clk_i or posedge rst_i)
if (rst_i)
    rx_wr_ptr_q  <= 11'b0;
else if (rd_valid_q)
    rx_wr_ptr_q  <= rx_wr_ptr_next_w;

wire [31:0] rx_data_w;

ft60x_ram_dp
u_rx_ram
(
     .clk0_i(clk_i)
    ,.rst0_i(rst_i)
    ,.clk1_i(clk_i)
    ,.rst1_i(rst_i)

    // Write Port
    ,.wr0_i(rd_valid_q)
    ,.addr0_i(rx_wr_ptr_q)
    ,.data0_i(rd_data_q)
    ,.data0_o()

    // Read Port
    ,.wr1_i(1'b0)
    ,.addr1_i(rx_rd_ptr_q)
    ,.data1_i(32'b0)
    ,.data1_o(rx_data_w)
);

wire [11:0] rx_level_w = (rx_rd_ptr_q <= rx_wr_ptr_q) ? (rx_wr_ptr_q - rx_rd_ptr_q) : (12'd2048 - rx_rd_ptr_q) + rx_wr_ptr_q;
wire        rx_space_w = (12'd2048 - rx_level_w) > 12'd1024;

// Delayed write pointer
reg [10:0] rx_wr_ptr2_q;
always @ (posedge clk_i or posedge rst_i)
if (rst_i)
    rx_wr_ptr2_q  <= 11'b0;
else
    rx_wr_ptr2_q  <= rx_wr_ptr_q;

// Read Side
wire read_ok_w = (rx_wr_ptr2_q != rx_rd_ptr_q);
reg  rd_q;

always @ (posedge clk_i or posedge rst_i)
if (rst_i)
    rd_q <= 1'b0;
else
    rd_q <= read_ok_w;

always @ (posedge clk_i or posedge rst_i)
if (rst_i)
    rx_rd_ptr_q <= 11'b0;
// Read address increment
else if (read_ok_w && ((!outport_valid_o) || (outport_valid_o && outport_accept_i)))
    rx_rd_ptr_q <= rx_rd_ptr_next_w;

// Read Skid Buffer
reg        rd_skid_q;
reg [31:0] rd_skid_data_q;

always @ (posedge clk_i or posedge rst_i)
if (rst_i)
begin
    rd_skid_q      <= 1'b0;
    rd_skid_data_q <= 32'b0;
end
else if (outport_valid_o && !outport_accept_i)
begin
    rd_skid_q      <= 1'b1;
    rd_skid_data_q <= outport_data_o;
end
else
begin
    rd_skid_q      <= 1'b0;
    rd_skid_data_q <= 32'b0;
end

assign outport_valid_o = rd_skid_q | rd_q;
assign outport_data_o  = rd_skid_q ? rd_skid_data_q : rx_data_w;

//-----------------------------------------------------------------
// Tx RAM: Target -> FT60x
//-----------------------------------------------------------------
reg  [10:0] tx_wr_ptr_q;
wire [10:0] tx_wr_ptr_next_w = tx_wr_ptr_q + 11'd1;
reg  [10:0] tx_rd_ptr_q;
wire [10:0] tx_rd_ptr_next_w = tx_rd_ptr_q + 11'd1;
wire [10:0] tx_rd_ptr_prev_w = tx_rd_ptr_q - 11'd2;

wire [31:0] tx_data_w;

ft60x_ram_dp
u_tx_ram
(
     .clk0_i(clk_i)
    ,.rst0_i(rst_i)
    ,.clk1_i(clk_i)
    ,.rst1_i(rst_i)

    // Write Port
    ,.wr0_i(inport_valid_i & inport_accept_o)
    ,.addr0_i(tx_wr_ptr_q)
    ,.data0_i(inport_data_i)
    ,.data0_o()

    // Read Port
    ,.wr1_i(1'b0)
    ,.addr1_i(tx_rd_ptr_q)
    ,.data1_i(32'b0)
    ,.data1_o(tx_data_w)
);

// Write pointer
always @ (posedge clk_i or posedge rst_i)
if (rst_i)
    tx_wr_ptr_q  <= 11'b0;
else if (inport_valid_i & inport_accept_o)
    tx_wr_ptr_q  <= tx_wr_ptr_next_w;

// Delayed write pointer (pesamistic)
reg  [10:0] tx_wr_ptr2_q;
always @ (posedge clk_i or posedge rst_i)
if (rst_i)
    tx_wr_ptr2_q  <= 11'b0;
else
    tx_wr_ptr2_q  <= tx_wr_ptr_q;

// Write push timeout
localparam TX_BACKOFF_THRESH = 16'h00FF;

reg [15:0] tx_idle_cycles_q;
always @ (posedge clk_i or posedge rst_i)
if (rst_i)
    tx_idle_cycles_q  <= 16'b0;
else if (inport_valid_i)
    tx_idle_cycles_q  <= 16'b0;
else if (tx_idle_cycles_q != TX_BACKOFF_THRESH)
    tx_idle_cycles_q <= tx_idle_cycles_q + 16'd1;

wire tx_timeout_w = (tx_idle_cycles_q == TX_BACKOFF_THRESH);

wire [11:0] tx_level_w = (tx_rd_ptr_q <= tx_wr_ptr2_q) ? (tx_wr_ptr2_q - tx_rd_ptr_q) : (12'd2048 - tx_rd_ptr_q) + tx_wr_ptr2_q;
wire        tx_ready_w = (tx_level_w >= (1024 / 4)) || (tx_timeout_w && tx_level_w != 12'd0);

assign inport_accept_o = (tx_level_w < 12'd2000);

reg [11:0] tx_level_q;

always @ (posedge clk_i or posedge rst_i)
if (rst_i)
    tx_level_q  <= 12'b0;
else
    tx_level_q  <= tx_level_w;

//-----------------------------------------------------------------
// Defines / Local params
//-----------------------------------------------------------------
localparam STATE_W           = 3;
localparam STATE_IDLE        = 3'd0;
localparam STATE_TX_START    = 3'd1;
localparam STATE_TX          = 3'd2;
localparam STATE_TURNAROUND  = 3'd3;
localparam STATE_RX_START    = 3'd4;
localparam STATE_RX          = 3'd5;

reg [STATE_W-1:0] state_q;

wire rx_ready_w = !ftdi_rxf_i;
wire tx_space_w = !ftdi_txe_i;

assign rx_valid_w = rx_ready_w & (state_q == STATE_RX);

//-----------------------------------------------------------------
// Next State Logic
//-----------------------------------------------------------------
reg [STATE_W-1:0] next_state_r;
reg [2:0]         state_count_q;
always @ *
begin
    next_state_r = state_q;

    case (state_q)
    //-----------------------------------------
    // STATE_IDLE
    //-----------------------------------------
    STATE_IDLE :
    begin
        if (rx_ready_w && rx_space_w)
            next_state_r = STATE_RX_START;
        else if (tx_space_w && tx_ready_w)
            next_state_r = STATE_TX_START; 
    end
    //-----------------------------------------
    // STATE_TX_START
    //-----------------------------------------
    STATE_TX_START :
    begin
        next_state_r  = STATE_TX;
    end
    //-----------------------------------------
    // STATE_TX
    //-----------------------------------------
    STATE_TX :
    begin
        if (!tx_space_w || tx_level_q == 12'd0)
            next_state_r  = STATE_TURNAROUND;
    end
    //-----------------------------------------
    // STATE_TURNAROUND
    //-----------------------------------------
    STATE_TURNAROUND :
    begin
        if (state_count_q == 3'b0)
            next_state_r  = STATE_IDLE;
    end
    //-----------------------------------------
    // STATE_RX_START
    //-----------------------------------------
    STATE_RX_START :
    begin
        next_state_r  = STATE_RX;
    end
    //-----------------------------------------
    // STATE_RX
    //-----------------------------------------
    STATE_RX :
    begin
        if (!rx_ready_w /*|| rx_full_next_w*/)
            next_state_r  = STATE_TURNAROUND;
    end
    default:
        ;
   endcase
end

// Update state
always @ (posedge clk_i or posedge rst_i)
if (rst_i)
    state_q   <= STATE_IDLE;
else
    state_q   <= next_state_r;

always @ (posedge clk_i or posedge rst_i)
if (rst_i)
    state_count_q   <= 3'b0;
else if (state_q == STATE_TX && next_state_r == STATE_TURNAROUND)
    state_count_q   <= 3'd7;
else if (state_q == STATE_RX && next_state_r == STATE_TURNAROUND)
    state_count_q   <= 3'd7;
else if (state_count_q != 3'b0)
    state_count_q   <= state_count_q - 3'd1;

// Read pointer
always @ (posedge clk_i or posedge rst_i)
if (rst_i)
    tx_rd_ptr_q <= 11'b0;
else if (((state_q == STATE_IDLE && next_state_r == STATE_TX_START) || (state_q == STATE_TX_START) || (state_q == STATE_TX && tx_space_w)) && (tx_rd_ptr_q != tx_wr_ptr2_q) && (tx_level_q != 12'b0))
    tx_rd_ptr_q <= tx_rd_ptr_next_w;
else if ((state_q == STATE_TX) && !tx_space_w && tx_level_q == 12'b0)
    tx_rd_ptr_q <= tx_rd_ptr_q - 11'd1;
else if ((state_q == STATE_TX) && !tx_space_w)
    tx_rd_ptr_q <= tx_rd_ptr_prev_w;

//-----------------------------------------------------------------
// RD/WR/OE
//-----------------------------------------------------------------
// Xilinx placement pragmas:
//synthesis attribute IOB of rdn_q is "TRUE"
//synthesis attribute IOB of wrn_q is "TRUE"
//synthesis attribute IOB of oen_q is "TRUE"
//synthesis attribute IOB of data_q is "TRUE"

reg        rdn_q;
reg        wrn_q;
reg        oen_q;
reg [35:0] data_q;

always @ (posedge clk_i or posedge rst_i)
if (rst_i)
    oen_q   <= 1'b1;
else if (state_q == STATE_IDLE && next_state_r == STATE_RX_START)
    oen_q   <= 1'b0;
else if (state_q == STATE_RX && next_state_r == STATE_TURNAROUND)
    oen_q   <= 1'b1;

always @ (posedge clk_i or posedge rst_i)
if (rst_i)
    rdn_q   <= 1'b1;
else if (state_q == STATE_RX_START && next_state_r == STATE_RX)
    rdn_q   <= 1'b0;
else if (state_q == STATE_RX && next_state_r == STATE_TURNAROUND)
    rdn_q   <= 1'b1;

always @ (posedge clk_i or posedge rst_i)
if (rst_i)
    wrn_q   <= 1'b1;
else if (state_q == STATE_TX_START && next_state_r == STATE_TX)
    wrn_q   <= 1'b0;
else if (state_q == STATE_TX && next_state_r == STATE_TURNAROUND)
    wrn_q   <= 1'b1;

always @ (posedge clk_i or posedge rst_i)
if (rst_i)
    data_q  <= 36'b0;
else
    data_q  <= {4'b1111, tx_data_w};

assign ftdi_wrn_o      = wrn_q;
assign ftdi_rdn_o      = rdn_q;
assign ftdi_oen_o      = oen_q;
assign ftdi_data_out_o = data_q[31:0];
assign ftdi_be_out_o   = data_q[35:32];

endmodule


module ft60x_ram_dp
(
    // Inputs
     input           clk0_i
    ,input           rst0_i
    ,input  [ 10:0]  addr0_i
    ,input  [ 31:0]  data0_i
    ,input           wr0_i
    ,input           clk1_i
    ,input           rst1_i
    ,input  [ 10:0]  addr1_i
    ,input  [ 31:0]  data1_i
    ,input           wr1_i

    // Outputs
    ,output [ 31:0]  data0_o
    ,output [ 31:0]  data1_o
);

//-----------------------------------------------------------------
// Dual Port RAM 8KB
// Mode: Read First
//-----------------------------------------------------------------
/* verilator lint_off MULTIDRIVEN */
reg [31:0]   ram [2047:0] /*verilator public*/;
/* verilator lint_on MULTIDRIVEN */

reg [31:0] ram_read0_q;
reg [31:0] ram_read1_q;


// Synchronous write
always @ (posedge clk0_i)
begin
    if (wr0_i)
        ram[addr0_i][31:0] <= data0_i[31:0];

    ram_read0_q <= ram[addr0_i];
end

always @ (posedge clk1_i)
begin
    if (wr1_i)
        ram[addr1_i][31:0] <= data1_i[31:0];

    ram_read1_q <= ram[addr1_i];
end

assign data0_o = ram_read0_q;
assign data1_o = ram_read1_q;


endmodule
