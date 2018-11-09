/*
Copyright (c) 2015-2017, Raspberry Pi Foundation
Copyright (c) 2015, Dave Stevenson
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the copyright holder nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/


#ifndef AR0144MODES_H_
#define AR0144MODES_H_

#include <stddef.h>
struct sensor_regs ar0144_regs[] =
        {
                // start streaming
                //{0x3100, 1},
                {0x301A, 0x205C}, // TODO fix ME!!!!
        };

struct mode_def ar0144_modes[] = {
   {
      .regs          = ar0144_regs,
      .num_regs      = 1,
      .width         = 1280,
      .height        = 800,
      .encoding      = 0,
      .order         = BAYER_ORDER_GBRG,
      .native_bit_depth = 12,
      .image_id      = 0x2C,
      .data_lanes    = 2,
      .min_vts       = 832,
      .line_time_ns  = 23077,
      .timing        = {0, 0, 0, 0, 0},
      .term          = {0, 0},
      .black_level   = 0,
   }
};

#undef addreg

struct sensor_regs ar0144_stop[] = {
   { 0x301A, 0x2058 },
};

struct sensor_def ar0144 = {
   .name =                 "ar0144",
   .modes =                ar0144_modes,
   .num_modes =            1,
   .stop =                 ar0144_stop,
   .num_stop_regs =        1,

   .i2c_addr =             0x10,
   .i2c_addressing =       2,
   .i2c_data_size =        2,
   .i2c_ident_length =     2,
   .i2c_ident_reg =        0x301A,
   .i2c_ident_value =      0x5820,  // TODO: Fix ME!!!!

   .vflip_reg =            0,
   .vflip_reg_bit =        0,
   .hflip_reg =            0,
   .hflip_reg_bit =        0,

   .exposure_reg =         0,
   .exposure_reg_num_bits = 20,

   .vts_reg =              0,
   .vts_reg_num_bits =     16,

   .gain_reg =             0,
   .gain_reg_num_bits =    10,
};

#endif
