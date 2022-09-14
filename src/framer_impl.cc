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
#include "framer_impl.h"

namespace gr {
namespace tutorial {

framer::sptr
framer::make(uint8_t preamble, size_t preamble_len,
             const std::vector<uint8_t> &sync_word)
{
    return gnuradio::get_initial_sptr(
               new framer_impl(preamble, preamble_len, sync_word));
}

/*
 * The private constructor
 */
framer_impl::framer_impl(uint8_t preamble, size_t preamble_len,
                         const std::vector<uint8_t> &sync_word) :
    gr::block("framer", gr::io_signature::make(0, 0, 0),
              gr::io_signature::make(0, 0, 0))
{
    message_port_register_in(pmt::mp("pdu"));
    message_port_register_out(pmt::mp("frame"));

    /*
     * Register the message handler. For every message received in the input
     * message port it will be called automatically.
     */
    set_msg_handler(pmt::mp("pdu"),
    [this](pmt::pmt_t msg) {
        this->framer_impl::construct(msg);
    });

    max_size = 3 * 2048;

    buffer = new uint8_t[max_size + 2 + sync_word.size() + preamble_len];

    std::fill_n(buffer, preamble_len, preamble);

    for (int i = 0; i < sync_word.size(); i++){
        buffer[preamble_len + i] = sync_word[i];
    }

    bufferStart = preamble_len + sync_word.size();
}

void
framer_impl::construct(pmt::pmt_t m)
{
    /* Extract the bytes of the PDU. GNU Radio handles PDUs in pairs.
     * The first element of the pair contains metadata associated with  the
     * PDU, whereas the second element of the pair is a pmt with a u8 vector
     * containing the raw bytes. Below there is an example how to get the
     * PDU raw pointer and the length of the frame in bytes that points to
     */
    pmt::pmt_t meta(pmt::car(m));
    pmt::pmt_t bytes(pmt::cdr(m));
    /* Access the raw bytes of the PDU */
    size_t pdu_len;
    const uint8_t *bytes_in = pmt::u8vector_elements(bytes, pdu_len);
    size_t bufferElement = bufferStart;

    /*
     * TODO: Do processing
     */

    if(pdu_len > max_size) 
        return;

    buffer[bufferElement] = (pdu_len >> 8);
    buffer[bufferElement + 1] = pdu_len;
    bufferElement += 2;

    for(int i = 0; i < pdu_len; i++){
        buffer[bufferElement] = bytes_in[i];
        bufferElement++;
    }

    /*
     * FIXME: This just copies the input to the output. It is just for testing
     * purposes.
     * NOTE: Obey the pair scheme that GNU Radio uses otherwise following
     * blocks will not work. In your case if you do not have any associated
     * metadata, place just pmt::PMT_NIL on the first element of the pair
     */
    message_port_pub(pmt::mp("frame"), pmt::cons(pmt::PMT_NIL, pmt::make_blob(buffer, bufferElement)));
}

/*
 * Our virtual destructor.
 */
framer_impl::~framer_impl()
{
    delete buffer;
}

} /* namespace tutorial */
} /* namespace gr */

