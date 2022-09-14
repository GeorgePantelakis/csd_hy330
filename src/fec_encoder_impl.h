/* -*- c++ -*- */
/*
 * gr-tutorial: Useful blocks for SDR and GNU Radio learning
 *
 *  Copyright (C) 2019, 2020 Manolis Surligas <surligas@gmail.com>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef INCLUDED_TUTORIAL_FEC_ENCODER_IMPL_H
#define INCLUDED_TUTORIAL_FEC_ENCODER_IMPL_H

#include <tutorial/fec_encoder.h>

namespace gr {
namespace tutorial {

class fec_encoder_impl : public fec_encoder {
private:
    const int d_type;
    uint8_t* buffer;
    uint8_t* bit_buffer;

    int u[24];
    int P[12][12] = {{1, 0, 0, 0, 1, 1, 1, 0, 1, 1, 0, 1},
                     {0, 0, 0, 1, 1, 1, 0, 1, 1, 0, 1, 1},
                     {0, 0, 1, 1, 1, 0, 1, 1, 0, 1, 0, 1},
                     {0, 1, 1, 1, 0, 1, 1, 0, 1, 0, 0, 1},
                     {1, 1, 1, 0, 1, 1, 0, 1, 0, 0, 0, 1},
                     {1, 1, 0, 1, 1, 0, 1, 0, 0, 0, 1, 1},
                     {1, 0, 1, 1, 0, 1, 0, 0, 0, 1, 1, 1},
                     {0, 1, 1, 0, 1, 0, 0, 0, 1, 1, 1, 1},
                     {1, 1, 0, 1, 0, 0, 0, 1, 1, 1, 0, 1},
                     {1, 0, 1, 0, 0, 0, 1, 1, 1, 0, 1, 1},
                     {0, 1, 0, 0, 0, 1, 1, 1, 0, 1, 1, 1},
                     {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0}};
    int** G;
    int** HT;

    void encode(pmt::pmt_t m);

public:
    fec_encoder_impl(int type);
    ~fec_encoder_impl();

};

} // namespace tutorial
} // namespace gr

#endif /* INCLUDED_TUTORIAL_FEC_ENCODER_IMPL_H */

