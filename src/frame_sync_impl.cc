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
#include "frame_sync_impl.h"

namespace gr {
namespace tutorial {

frame_sync::sptr
frame_sync::make(uint8_t preamble, uint8_t preamble_len,
                 const std::vector<uint8_t> &sync_word,
                 int mod)
{
    return gnuradio::get_initial_sptr
           (new frame_sync_impl(preamble, preamble_len, sync_word, mod));
}


/*
 * The private constructor
 */
frame_sync_impl::frame_sync_impl(uint8_t preamble, uint8_t preamble_len,
                                 const std::vector<uint8_t> &sync_word,
                                 int mod)
    : gr::sync_block("frame_sync",
                    gr::io_signature::make(1, 1, sizeof(uint8_t)),
                    gr::io_signature::make(0, 0, 0)),
                    d_mod((mod_t)mod),
                    d_preamble(preamble),
                    d_preamble_len(preamble_len),
                    d_sync_word(sync_word)
{
    message_port_register_out(pmt::mp("pdu"));

    d_FSD_len = sync_word.size();

    stream = new shift_reg(preamble_len * 4);
    preamble_lookup = new shift_reg(preamble_len * 4); //besicaly (preamble_len / 2) * 8
    FSD_lookup = new shift_reg(d_FSD_len * 8);
    FSD_lookup_90 = new shift_reg(d_FSD_len * 8);
    FSD_lookup_180 = new shift_reg(d_FSD_len * 8);
    FSD_lookup_270 = new shift_reg(d_FSD_len * 8);
    max_size = 3 * 2048;

    byteBuffer = new uint8_t[8];
    buffer = new uint8_t[max_size];

    stream->reset();
    preamble_lookup->reset();
    FSD_lookup->reset();
    FSD_lookup_90->reset();
    FSD_lookup_180->reset();
    FSD_lookup_270->reset();

    for(int i = 0; i < preamble_len / 2; i++){
        for(int j = 0; j < 8; j++){
            *preamble_lookup >>= ((preamble & (1 << j)) >> j);
        }
    }

    for(int i = 0; i < sync_word.size(); i++){
        for(int j = 0; j < 8; j++){
            *FSD_lookup >>= ((sync_word[sync_word.size() - i - 1] & (1 << j)) >> j);
        }
    }

    for(int i = 0; i < sync_word.size(); i++){
        for(int j = 0; j < 8; j += 2){
            if((sync_word[sync_word.size() - i - 1] & (1 << j)) >> j){
                if((sync_word[sync_word.size() - i - 1] & (1 << (j + 1))) >> (j + 1)){
                    *FSD_lookup_90 >>= 0;
                    *FSD_lookup_90 >>= 0;
                }else{
                    *FSD_lookup_90 >>= 0;
                    *FSD_lookup_90 >>= 1;
                }
            }else{
                if((sync_word[sync_word.size() - i - 1] & (1 << (j + 1))) >> (j + 1)){
                    *FSD_lookup_90 >>= 1;
                    *FSD_lookup_90 >>= 1;
                }else{
                    *FSD_lookup_90 >>= 1;
                    *FSD_lookup_90 >>= 0;
                }
            }
        }
    }

    for(int i = 0; i < sync_word.size(); i++){
        for(int j = 0; j < 8; j += 2){
            if((sync_word[sync_word.size() - i - 1] & (1 << j)) >> j){
                if((sync_word[sync_word.size() - i - 1] & (1 << (j + 1))) >> (j + 1)){
                    *FSD_lookup_180 >>= 1;
                    *FSD_lookup_180 >>= 0;
                }else{
                    *FSD_lookup_180 >>= 1;
                    *FSD_lookup_180 >>= 1;
                }
            }else{
                if((sync_word[sync_word.size() - i - 1] & (1 << (j + 1))) >> (j + 1)){
                    *FSD_lookup_180 >>= 0;
                    *FSD_lookup_180 >>= 0;
                }else{
                    *FSD_lookup_180 >>= 0;
                    *FSD_lookup_180 >>= 1;
                }
            }
        }
    }

    for(int i = 0; i < sync_word.size(); i++){
        for(int j = 0; j < 8; j += 2){
            if((sync_word[sync_word.size() - i - 1] & (1 << j)) >> j){
                if((sync_word[sync_word.size() - i - 1] & (1 << (j + 1))) >> (j + 1)){
                    *FSD_lookup_270 >>= 0;
                    *FSD_lookup_270 >>= 1;
                }else{
                    *FSD_lookup_270 >>= 0;
                    *FSD_lookup_270 >>= 0;
                }
            }else{
                if((sync_word[sync_word.size() - i - 1] & (1 << (j + 1))) >> (j + 1)){
                    *FSD_lookup_270 >>= 1;
                    *FSD_lookup_270 >>= 0;
                }else{
                    *FSD_lookup_270 >>= 1;
                    *FSD_lookup_270 >>= 1;
                }
            }
        }
    }

    state = PREAMBLE_SEARCH;
    allowed_mistakes = (d_preamble_len * 4) / 10;
    data_received = 0;
    byteBufferIntex = 0;
    bufferIntex = 0;
    BPSK_inversed = false;
    rotation = R_0;
    saved_bit = 0;
    has_save_bit = false;
}

/*
 * Our virtual destructor.
 */
frame_sync_impl::~frame_sync_impl()
{
    if(stream)
        delete stream;
    delete preamble_lookup;
    delete FSD_lookup;
    delete byteBuffer;
    delete buffer;
}

int
frame_sync_impl::work(int noutput_items,
                      gr_vector_const_void_star &input_items,
                      gr_vector_void_star &output_items)
{
    const uint8_t* in = (const uint8_t *) input_items[0];

    // Do <+signal processing+>
    /*
     * GNU Radio handles PMT messages in a pair structure.
     * The first element corresonds to possible metadata (in our case we do not
     * have assosiated metadata) whereas the second element contains the
     * data in a vector of uint8_t.
     *
     * When the frame is extracted, you will possible hold it in a *uint8_t buffer,
     * To create a PMT pair message you can use:
     * pmt::pmt_t pair = pmt::cons(pmt::PMT_NIL, pmt::init_u8vector(frame_len, buf));
     *
     * Then you can send this message to the port we have registered at the contructpr
     * using:
     *
     * message_port_pub(pmt::mp("pdu"), pair);
     */
    for(int count = 0; count < noutput_items; count++){
        switch(state){
            case PREAMBLE_SEARCH:
                stream->push_back(in[count]);
                if((*stream ^ *preamble_lookup).count() <= allowed_mistakes){
                    state = FSD_SEARCH;
                    allowed_mistakes = d_sync_word.size();
                    data_received = 0;
                    delete stream;
                    stream = new shift_reg(d_FSD_len * 8);
                    stream->reset();
                }
                break;
            case FSD_SEARCH:
                stream->push_back(in[count]);
                data_received++;
                if((*stream ^ *FSD_lookup).count() <= allowed_mistakes){
                    state = SIZE_AQUISITION;
                    byteBufferIntex = 0;
                    bufferIntex = 0;
                    BPSK_inversed = false;
                    rotation = R_0;
                    delete stream;
                    stream = nullptr;
                }else if ((d_mod == BPSK) && ((*stream ^ *FSD_lookup).count() >= (FSD_lookup->size() - allowed_mistakes))){
                    state = SIZE_AQUISITION;
                    byteBufferIntex = 0;
                    bufferIntex = 0;
                    BPSK_inversed = true;
                    delete stream;
                    stream = nullptr;
                }else if ((d_mod == QPSK) && ((*stream ^ *FSD_lookup_90).count() <= allowed_mistakes)){
                    state = SIZE_AQUISITION;
                    byteBufferIntex = 0;
                    bufferIntex = 0;
                    rotation = R_90;
                    delete stream;
                    stream = nullptr;
                }else if ((d_mod == QPSK) && ((*stream ^ *FSD_lookup_180).count() <= allowed_mistakes)){
                    state = SIZE_AQUISITION;
                    byteBufferIntex = 0;
                    bufferIntex = 0;
                    rotation = R_180;
                    delete stream;
                    stream = nullptr;
                }else if ((d_mod == QPSK) && ((*stream ^ *FSD_lookup_270).count() <= allowed_mistakes)){
                    state = SIZE_AQUISITION;
                    byteBufferIntex = 0;
                    bufferIntex = 0;
                    rotation = R_270;
                    delete stream;
                    stream = nullptr;
                }else if (data_received > ((d_preamble_len + d_sync_word.size()) * 8)){
                    state = PREAMBLE_SEARCH;
                    allowed_mistakes = (d_preamble_len * 4) / 10;
                    delete stream;
                    stream = new shift_reg(d_preamble_len * 4);
                    stream->reset();
                }
                break;
            case SIZE_AQUISITION:
                if(d_mod == BPSK)
                    byteBuffer[7 - byteBufferIntex] = BPSK_inversed ? (!in[count]) : in[count];
                else if(d_mod == QPSK){
                    if(has_save_bit){
                        switch(rotation){
                            case R_0:
                                byteBuffer[6 - byteBufferIntex] = saved_bit;
                                byteBuffer[7 - byteBufferIntex] = in[count];
                                break;
                            case R_90: 
                                byteBuffer[6 - byteBufferIntex] = !saved_bit;
                                byteBuffer[7 - byteBufferIntex] = saved_bit ^ in[count];
                                break;
                            case R_180:
                                byteBuffer[6 - byteBufferIntex] = saved_bit;
                                byteBuffer[7 - byteBufferIntex] = !in[count]; 
                                break;
                            case R_270: 
                                byteBuffer[6 - byteBufferIntex] = !saved_bit;
                                byteBuffer[7 - byteBufferIntex] = !(saved_bit ^ in[count]);
                                break;
                        }
                        has_save_bit = false;
                    }else{
                        saved_bit = in[count];
                        has_save_bit = true;
                    }
                }
                byteBufferIntex++;
                if(byteBufferIntex == 8){
                    buffer[bufferIntex] = ((byteBuffer[7] << 7) | (byteBuffer[6] << 6) | (byteBuffer[5] << 5) | (byteBuffer[4] << 4) | (byteBuffer[3] << 3) | (byteBuffer[2] << 2) | (byteBuffer[1] << 1) | (byteBuffer[0]));
                    bufferIntex++;
                    byteBufferIntex = 0;
                }
                if(bufferIntex == 2){
                    state = DATA_AQUISITION;
                    messageSize = (buffer[0] << 8) | buffer[1];
                    byteBufferIntex = 0;
                    bufferIntex = 0;
                    if(messageSize > max_size){
                        state = PREAMBLE_SEARCH;
                        allowed_mistakes = (d_preamble_len * 4) / 10;
                        delete stream;
                        stream = new shift_reg(d_preamble_len * 4);
                        stream->reset();
                    }
                }
                break;
            case DATA_AQUISITION:
                if(d_mod == BPSK)
                    byteBuffer[7 - byteBufferIntex] = BPSK_inversed ? (!in[count]) : in[count];
                else if(d_mod == QPSK){
                    if(has_save_bit){
                        switch(rotation){
                            case R_0:
                                byteBuffer[6 - byteBufferIntex] = saved_bit;
                                byteBuffer[7 - byteBufferIntex] = in[count];
                                break;
                            case R_90: 
                                byteBuffer[6 - byteBufferIntex] = !saved_bit;
                                byteBuffer[7 - byteBufferIntex] = saved_bit ^ in[count];
                                break;
                            case R_180:
                                byteBuffer[6 - byteBufferIntex] = saved_bit;
                                byteBuffer[7 - byteBufferIntex] = !in[count]; 
                                break;
                            case R_270: 
                                byteBuffer[6 - byteBufferIntex] = !saved_bit;
                                byteBuffer[7 - byteBufferIntex] = !(saved_bit ^ in[count]);
                                break;
                        }
                        has_save_bit = false;
                    }else{
                        saved_bit = in[count];
                        has_save_bit = true;
                    }
                }
                byteBufferIntex++;
                if(byteBufferIntex == 8){
                    buffer[bufferIntex] = ((byteBuffer[7] << 7) | (byteBuffer[6] << 6) | (byteBuffer[5] << 5) | (byteBuffer[4] << 4) | (byteBuffer[3] << 3) | (byteBuffer[2] << 2) | (byteBuffer[1] << 1) | (byteBuffer[0]));
                    bufferIntex++;
                    byteBufferIntex = 0;
                }
                if(bufferIntex == messageSize){
                    message_port_pub(pmt::mp("pdu"), pmt::cons(pmt::PMT_NIL, pmt::make_blob(buffer, messageSize)));
                    state = PREAMBLE_SEARCH;
                    allowed_mistakes = (d_preamble_len * 4) / 10;
                    delete stream;
                    stream = new shift_reg(d_preamble_len * 4);
                    stream->reset();
                    byteBufferIntex = 0;
                    bufferIntex = 0;
                    messageSize = 0;
                }
                break;
        }
    }

    // Tell runtime system how many output items we produced.
    return noutput_items;
}

} /* namespace tutorial */
} /* namespace gr */

