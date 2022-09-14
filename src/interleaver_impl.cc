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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gnuradio/io_signature.h>
#include "interleaver_impl.h"

namespace gr {
namespace tutorial {

interleaver::sptr
interleaver::make(size_t block_size)
{
    return gnuradio::get_initial_sptr
           (new interleaver_impl(block_size));
}


/*
 * The private constructor
 */
interleaver_impl::interleaver_impl(size_t block_size)
    : gr::block("interleaver",
                gr::io_signature::make(0, 0, 0),
                gr::io_signature::make(0, 0, 0)),
                d_block_size(block_size)
{
    message_port_register_in(pmt::mp("pdu_in"));
    message_port_register_out(pmt::mp("pdu_out"));

    /* Register the message handler. For every message received in the input
     * message port it will be called automatically.
     */
    set_msg_handler(pmt::mp("pdu_in"),
    [this](pmt::pmt_t msg) {
        this->interleaver_impl::interleave(msg);
    });

    interleave_table = new uint8_t*[block_size];
    for(int i = 0; i < block_size; i++)
        interleave_table[i] = new uint8_t[block_size];
}

/*
 * Our virtual destructor.
 */
interleaver_impl::~interleaver_impl()
{
    for(int i = 0; i < d_block_size; i++)
        delete[] interleave_table[i];
    delete[] interleave_table;
}

void
interleaver_impl::interleave(pmt::pmt_t m)
{
    pmt::pmt_t meta(pmt::car(m));
    pmt::pmt_t bytes(pmt::cdr(m));

    size_t pdu_len;
    const uint8_t *bytes_in = pmt::u8vector_elements(bytes, pdu_len);

    size_t row = 0;
    size_t col = 0;

    uint8_t bit_buffer[pdu_len * 8];
    size_t bitBufferIntex = 0;
    uint8_t buffer[pdu_len];

    /* TODO: Add your code here */

    if(((pdu_len * 8) % d_block_size) != 0){
        return;
    }

    for(int i = 0; i < pdu_len; i++){
        for(int j = 7; j >= 0; j--){
            interleave_table[row][col] = ((bytes_in[i] >> j) & 1);
            col++;
            if(col == d_block_size){
                row++;
                col = 0;
            }
            if(row == d_block_size){
                std::cout << "Warning at Interleaver: Too small block size for all the data!" << std::endl;
                break;
            }
        }
    }

    for(int _col = 0; _col < d_block_size; _col++){
        for(int _row = 0; _row < row; _row++){
            bit_buffer[bitBufferIntex] = interleave_table[_row][_col];
            bitBufferIntex++;
        }
    }

    for(int i = 0; i < pdu_len; i++){
            buffer[i] = (bit_buffer[8 * i] << 7) | (bit_buffer[(8 * i) + 1] << 6) | (bit_buffer[(8 * i) + 2] << 5) | (bit_buffer[(8 * i) + 3] << 4) | (bit_buffer[(8 * i) + 4] << 3) | (bit_buffer[(8 * i) + 5] << 2) | (bit_buffer[(8 * i) + 6] << 1) | (bit_buffer[(8 * i) + 7]);
    }

    /*
     * FIXME: This just copies the input to the output. Even you do not
     * implement the interleaver, it will forward the input message to the next
     * block and everything should work fine
     */
    message_port_pub(pmt::mp("pdu_out"), pmt::cons(pmt::PMT_NIL, pmt::make_blob(buffer, pdu_len)));
}


} /* namespace tutorial */
} /* namespace gr */

