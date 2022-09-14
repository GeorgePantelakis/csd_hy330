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

#ifndef INCLUDED_TUTORIAL_FRAME_SYNC_IMPL_H
#define INCLUDED_TUTORIAL_FRAME_SYNC_IMPL_H

#include <tutorial/frame_sync.h>
#include <tutorial/shift_reg.h>

namespace gr {
namespace tutorial {

class frame_sync_impl : public frame_sync {
private:
    typedef enum {
        BPSK,
        QPSK
    } mod_t;

    typedef enum {
        PREAMBLE_SEARCH,
        FSD_SEARCH,
        SIZE_AQUISITION,
        DATA_AQUISITION
    } state_t;

    typedef enum {
        R_0,
        R_90,
        R_180,
        R_270
    } rotation_t;

    const mod_t d_mod;
    shift_reg* stream;
    shift_reg* preamble_lookup;
    shift_reg* FSD_lookup;
    shift_reg* FSD_lookup_90;
    shift_reg* FSD_lookup_180;
    shift_reg* FSD_lookup_270;
    uint8_t allowed_mistakes;
    state_t state;
    const uint8_t d_preamble_len;
    uint8_t d_FSD_len;
    const uint8_t d_preamble;
    const std::vector<uint8_t> d_sync_word;
    uint8_t data_received;
    uint8_t *byteBuffer;
    uint8_t byteBufferIntex;
    uint8_t *buffer;
    size_t bufferIntex;
    uint16_t messageSize;
    bool BPSK_inversed;
    rotation_t rotation;
    uint8_t saved_bit;
    bool has_save_bit;
    size_t max_size;

public:
    frame_sync_impl(uint8_t preamble, uint8_t preamble_len,
                    const std::vector<uint8_t> &sync_word,
                    int mod);
    ~frame_sync_impl();

    // Where all the action really happens
    int work(
        int noutput_items,
        gr_vector_const_void_star &input_items,
        gr_vector_void_star &output_items
    );
};

} // namespace tutorial
} // namespace gr

#endif /* INCLUDED_TUTORIAL_FRAME_SYNC_IMPL_H */

