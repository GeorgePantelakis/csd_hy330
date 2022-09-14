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
#include "fec_decoder_impl.h"

namespace gr {
namespace tutorial {

fec_decoder::sptr
fec_decoder::make(int type)
{
    return gnuradio::get_initial_sptr
           (new fec_decoder_impl(type));
}


/*
 * The private constructor
 */
fec_decoder_impl::fec_decoder_impl(int type)
    : gr::block("fec_encoder",
                gr::io_signature::make(0, 0, 0),
                gr::io_signature::make(0, 0, 0)),
      d_type(type)
{
    message_port_register_in(pmt::mp("pdu_in"));
    message_port_register_out(pmt::mp("pdu_out"));

    /* Register the message handler. For every message received in the input
     * message port it will be called automatically.
     */
    set_msg_handler(pmt::mp("pdu_in"),
    [this](pmt::pmt_t msg) {
        this->fec_decoder_impl::decode(msg);
    });

    hamming_lookup = new uint8_t[8]{0, 0, 0, 1, 0, 1, 1, 1};

    if(type == 2){
        G = new int*[12];
        HT = new int*[24];

        for (int i = 0; i < 12; ++i) {
            G[i] = new int[24];
        }

        for (int i = 0; i < 24; ++i) {
            HT[i] = new int[12];
        }

        int I[12][12] = {};
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
fec_decoder_impl::~fec_decoder_impl()
{
    delete hamming_lookup;

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

int weight(int *v) {
    int weight = 0;
    for (int i = 0; i < 12; i++) {
        weight += v[i];
    }
    return weight;
}

void
fec_decoder_impl::decode(pmt::pmt_t m)
{
    pmt::pmt_t meta(pmt::car(m));
    pmt::pmt_t bytes(pmt::cdr(m));

    size_t pdu_len;
    const uint8_t *bytes_in = pmt::u8vector_elements(bytes, pdu_len);
    uint8_t bit_in_0;
    uint8_t bit_in_1;
    uint8_t bit_in_2;
    uint8_t bits_taken = 0;
    size_t bitBufferIntex = 0;

    uint8_t *bits_to_decode;
    size_t bits_to_decode_index = 0;
    size_t bit_buffer_index = 0;
    int e[24];
    bool decodable = true;
    int sp[12] = {};

    switch (d_type) {
    /* No FEC just copy the input message to the output */
    case 0:
        message_port_pub(pmt::mp("pdu_out"), m);
        return;
    case 1:
        /* Do Hamming decoding */
        buffer = new uint8_t[pdu_len / 3];
        bit_buffer = new uint8_t[(pdu_len / 3) * 8];

        for(int i = 0; i < pdu_len; i++){
            for(int j = 7; j >= 0; j--){
                switch(bits_taken){
                    case 0:
                        bit_in_0 = (bytes_in[i] >> j) & 1;
                        bits_taken++;
                        break;
                    case 1:
                        bit_in_1 = (bytes_in[i] >> j) & 1;
                        bits_taken++;
                        break;
                    case 2:
                        bit_in_2 = (bytes_in[i] >> j) & 1;
                        bit_buffer[bitBufferIntex] = hamming_lookup[(bit_in_0 << 2) | (bit_in_1 << 1) | (bit_in_2)];
                        bitBufferIntex++;
                        bits_taken = 0;
                        break;
                }
            }
        }

        for(int i = 0; i < pdu_len / 3; i++){
            buffer[i] = (bit_buffer[8 * i] << 7) | (bit_buffer[(8 * i) + 1] << 6) | (bit_buffer[(8 * i) + 2] << 5) | (bit_buffer[(8 * i) + 3] << 4) | (bit_buffer[(8 * i) + 4] << 3) | (bit_buffer[(8 * i) + 5] << 2) | (bit_buffer[(8 * i) + 6] << 1) | (bit_buffer[(8 * i) + 7]);
        }

        message_port_pub(pmt::mp("pdu_out"), pmt::cons(pmt::PMT_NIL, pmt::make_blob(buffer, pdu_len / 3)));

        delete buffer;
        delete bit_buffer;
        return;
    case 2:
        /* Do Golay decoding */

        buffer = new uint8_t[2 * pdu_len];
        bit_buffer = new uint8_t[2 * pdu_len * 8];
        bits_to_decode = new uint8_t[12];

        for(int k = 0; k < pdu_len; k++){
            for(int l = 7; l >= 0; l--){
                bits_to_decode[bits_to_decode_index] = ((bytes_in[k] >> l) & 1);
                bits_to_decode_index++;
                if(bits_to_decode_index == 12){
                    for (int m = 0; m < 24; m++) {
                        e[m] = 0;
                        //s[m] = 0;
                        if(m < 12)
                            sp[m] = 0;
                    }

                    for (int i = 0; i < 24; i++) {
                        for (int j = 0; j < 12; j++) {
                            s[j] = (s[j] + ((bits_to_decode[i] * HT[i][j]) % 2)) % 2;
                        }
                    }

                    if (weight(s) <= 3) {
                        for (int i = 0; i < 24; i++) {
                            if (i < 12) {
                                e[i] = s[i];
                            } else {
                                e[i] = 0;
                            }
                        }
                        goto result;
                    }

                    // if w(s + pi) <= 2 for some pi then set e = (s + pi, u(i))
                    else {
                        for (int i = 0; i < 12; i++) {
                            int spi[12] = {};
                            for (int j = 0; j < 12; j++) {
                                spi[j] = s[j];
                            }
                            for (int j = 0; j < 12; j++) {
                                spi[j] = (spi[j] + P[i][j]) % 2;
                            }
                            if (weight(spi) <= 2) {
                                for (int k = 0; k < 24; k++) {
                                    if (k < 12) {
                                        e[k] = spi[k];
                                    } else {
                                        e[k] = (i == (k - 12));
                                    }
                                }
                                goto result;
                            }
                        }
                    }

                    for (int i = 0; i < 12; i++) {
                        for (int j = 0; j < 12; j++) {
                            sp[j] = (sp[j] + ((s[i] * P[i][j]) % 2)) % 2;
                        }
                    }

                    // if w(s*P) = 2 or w(s*P) = 3 then set e = (0, s*P)
                    if (weight(sp) == 2 || weight(sp) == 3) {
                        for (int i = 0; i < 24; i++) {
                            if (i < 12) {
                                e[i] = 0;
                            } else {
                                e[i] = sp[i - 12];
                            }
                        }
                        goto result;
                    }

                    // if w(s*P + pi) = 2 for some pi then set e = (u(i), s*P + pi)
                    else {
                        for (int i = 0; i < 12; i++) {
                            int sppi[12] = {};
                            for (int j = 0; j < 12; j++) {
                                sppi[j] = sp[j];
                            }
                            for (int j = 0; j < 12; j++) {
                                sppi[j] = (sppi[j] + P[i][j]) % 2;
                            }
                            if (weight(sppi) == 2) {
                                for (int k = 0; k < 24; k++) {
                                    if (k < 12) {
                                        e[k] = (i == k);
                                    } else {
                                        e[k] = sppi[k - 12];
                                    }
                                }
                                goto result;
                            }
                        }
                    }

                    // if pattern is not correctable, request retransmission
                    decodable = false;
result:
                    /*
                    std::cout << "Error pattern: ";
                    for (int i = 0; i < 24; i++) {
                        std::cout << e[i];
                    }
                    std::cout << std::endl;
                    */

                    if (decodable) {
                        for (int i = 0; i < 12; i++) {
                            bit_buffer[bit_buffer_index] = (bits_to_decode[i + 12] + e[i]) % 2;
                            bit_buffer_index++;
                        }
                    } else {
                        for (int i = 0; i < 12; i++) {
                            bit_buffer[bit_buffer_index] = 0;
                            bit_buffer_index++;
                        }
                        decodable = true;
                    }
                    bits_to_decode_index = 0;
                }
            }
        }

        for(int i = 0; i < 2 * pdu_len; i++){
            buffer[i] = (bit_buffer[8 * i] << 7) | (bit_buffer[(8 * i) + 1] << 6) | (bit_buffer[(8 * i) + 2] << 5) | (bit_buffer[(8 * i) + 3] << 4) | (bit_buffer[(8 * i) + 4] << 3) | (bit_buffer[(8 * i) + 5] << 2) | (bit_buffer[(8 * i) + 6] << 1) | (bit_buffer[(8 * i) + 7]);
        }

        message_port_pub(pmt::mp("pdu_out"), pmt::cons(pmt::PMT_NIL, pmt::make_blob(buffer, 2 * pdu_len)));
        
        delete buffer;
        delete bit_buffer;
        delete bits_to_decode;

        return;
    default:
        throw std::runtime_error("fec_decoder: Invalid FEC");
        return;
    }
}

} /* namespace tutorial */
} /* namespace gr */

