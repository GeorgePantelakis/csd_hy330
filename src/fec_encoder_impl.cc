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
#include "fec_encoder_impl.h"

namespace gr {
namespace tutorial {

fec_encoder::sptr
fec_encoder::make(int type)
{
    return gnuradio::get_initial_sptr
           (new fec_encoder_impl(type));
}


/*
 * The private constructor
 */
fec_encoder_impl::fec_encoder_impl(int type)
    : gr::block("fec_encoder",
                gr::io_signature::make(0, 0, 0),
                gr::io_signature::make(0, 0, 0)),
      d_type(type)
{
    message_port_register_in(pmt::mp("pdu_in"));
    message_port_register_out(pmt::mp("pdu_out"));

    /*
     * Register the message handler. For every message received in the input
     * message port it will be called automatically.
     */
    set_msg_handler(pmt::mp("pdu_in"),
    [this](pmt::pmt_t msg) {
        this->fec_encoder_impl::encode(msg);
    });

    if(type == 2){
        G = new int*[12];
        HT = new int*[24];

        for (int i = 0; i < 12; ++i) {
            G[i] = new int[24];
        }

        for (int i = 0; i < 24; ++i) {
            HT[i] = new int[12];
        }

        for (int i = 0; i < 24; ++i) {
            u[i] = 0;
        }

        int I[12][12];
        for (int i = 0; i < 12; i++) {
            for (int j = 0; j < 12; j++) {
                if (i == j) {
                    I[i][j] = 1;
                } else {
                    I[i][j] = 0;
                }
            }
        }

        for (int i = 0; i < 12; i++) {
            for (int j = 0; j < 24; j++) {
                if (j < 12) {
                    HT[j][i] = I[i][j];
                    G[i][j] = P[i][j];
                } else {
                    G[i][j] = I[i][j - 12];
                    HT[j][i] = P[i][j - 12];
                }
            }
        }
    }
}

/*
 * Our virtual destructor.
 */
fec_encoder_impl::~fec_encoder_impl()
{
    if(d_type == 2){
        for (int i = 0; i < 12; ++i) {
            delete G[i];
        }

        delete G;

        for (int i = 0; i < 24; ++i) {
            delete HT[i];
        }

        delete HT;
    }
}

void
fec_encoder_impl::encode(pmt::pmt_t m)
{

    pmt::pmt_t meta(pmt::car(m));
    pmt::pmt_t bytes(pmt::cdr(m));

    size_t pdu_len;
    const uint8_t *bytes_in = pmt::u8vector_elements(bytes, pdu_len);
    uint8_t bit_in;
    uint8_t *bits_to_encode;
    size_t bits_to_encode_index = 0;
    size_t bit_buffer_index = 0;

    switch (d_type) {
    /* No FEC just copy the input message to the output */
    case 0:
        message_port_pub(pmt::mp("pdu_out"), m);
        return;
    case 1:
        /* Do Hamming encoding */
        buffer = new uint8_t[3 * pdu_len];
        bit_buffer = new uint8_t[3 * pdu_len * 8];

        for(int i = 0; i < pdu_len; i++){
            for(int j = 7; j >= 0; j--){
                bit_in = ((bytes_in[i] >> j) & 1);
                bit_buffer[(24 * i) + (3 * j)] = bit_in;
                bit_buffer[(24 * i) + (3 * j) + 1] = bit_in;
                bit_buffer[(24 * i) + (3 * j) + 2] = bit_in;
            }
        }
        
        for(int i = 0; i < 3 * pdu_len; i++){
            buffer[i] = (bit_buffer[8 * i] << 7) | (bit_buffer[(8 * i) + 1] << 6) | (bit_buffer[(8 * i) + 2] << 5) | (bit_buffer[(8 * i) + 3] << 4) | (bit_buffer[(8 * i) + 4] << 3) | (bit_buffer[(8 * i) + 5] << 2) | (bit_buffer[(8 * i) + 6] << 1) | (bit_buffer[(8 * i) + 7]);
        }

        message_port_pub(pmt::mp("pdu_out"), pmt::cons(pmt::PMT_NIL, pmt::make_blob(buffer, 3 * pdu_len)));

        delete buffer;
        delete bit_buffer;
        return;
    case 2:
        /* Do Golay encoding */
        if(pdu_len % 3 != 0){
            std::cout << "Warning: fec_encoder dropped a message because it's not a mul of 3!" << std::endl;
            return;
        }

        buffer = new uint8_t[2 * pdu_len];
        bit_buffer = new uint8_t[2 * pdu_len * 8];
        bits_to_encode = new uint8_t[12];

        for(int i = 0; i < pdu_len; i++){
            for(int j = 0; j < 8; j++){
                bits_to_encode[bits_to_encode_index] = (bytes_in[i] & (1 << (7 - j))) >> (7 - j);
                bits_to_encode_index++;
                if(bits_to_encode_index == 12){
                    for (int k = 0; k < 24; k++) {
                        for (int l = 0; l < 12; l++) {
                            u[k] = (u[k] + (bits_to_encode[l] * G[l][k])) % 2;
                        }
                        bit_buffer[bit_buffer_index] = u[k];
                        bit_buffer_index++;
                    }
                    bits_to_encode_index = 0;
                }
            }
        }

        for(int i = 0; i < 2 * pdu_len; i++){
            buffer[i] = (bit_buffer[8 * i] << 7) | (bit_buffer[(8 * i) + 1] << 6) | (bit_buffer[(8 * i) + 2] << 5) | (bit_buffer[(8 * i) + 3] << 4) | (bit_buffer[(8 * i) + 4] << 3) | (bit_buffer[(8 * i) + 5] << 2) | (bit_buffer[(8 * i) + 6] << 1) | (bit_buffer[(8 * i) + 7]);
        }

        message_port_pub(pmt::mp("pdu_out"), pmt::cons(pmt::PMT_NIL, pmt::make_blob(buffer, 2 * pdu_len)));
        
        delete buffer;
        delete bit_buffer;
        delete bits_to_encode;
        return;
    default:
        throw std::runtime_error("fec_encoder: Invalid FEC");
        return;
    }
}

} /* namespace tutorial */
} /* namespace gr */

