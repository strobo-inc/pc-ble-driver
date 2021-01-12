#include "h4_transport.h"
#include <algorithm>
#include <functional>
#include <iomanip>
#include <iostream>

H4Transport::H4Transport(Transport *nextTransportLayer)
    : is_open(false)
    , next_transport_layer(nextTransportLayer)
    , rx_state(RX_STATE_WAIT_HEADER)
{
    std::cout << "h4 transport layer created" << std::endl;
}
uint32_t H4Transport::open(const status_cb_t &status_callback, const data_cb_t &data_callback,
                           const log_cb_t &log_callback) noexcept
{
    if (is_open)
    {
        return NRF_ERROR_SD_RPC_SERIAL_PORT_ALREADY_OPEN;
    }
    auto retcode = Transport::open(status_callback, data_callback, log_callback);
    if (retcode != NRF_SUCCESS)
    {
        std::cout << "h4 trasnport open failed:" << retcode << std::endl;
        return retcode;
    }
    data_cb =
        std::bind(&H4Transport::data_handler, this, std::placeholders::_1, std::placeholders::_2);
    retcode = next_transport_layer->open(upperStatusCallback, data_cb, upperLogCallback);
    std::cout << "h4 transport opened:" << retcode << std::endl;
    return retcode;
}
uint32_t H4Transport::close() noexcept
{
    if (!is_open)
    {
        return NRF_ERROR_SD_RPC_SERIAL_PORT_ALREADY_CLOSED;
    }
    return next_transport_layer->close();
}

void print_vec(const std::vector<uint8_t> v)
{
    std::ios::fmtflags cur_flags = std::cout.flags();
    for (auto d : v)
    {
        std::cout << std::setw(2) << std::setfill('0') << std::hex << (int)d << " ";
    }
    std::cout << std::endl;
    std::cout.flags(cur_flags);
}

uint32_t H4Transport::send(const std::vector<uint8_t> &data) noexcept
{
    uint16_t size = data.size();
    std::vector<uint8_t> txpkt(size + 2);
    txpkt[0] = size & 0xff;
    txpkt[1] = (size >> 8) & 0xff;
    std::copy(data.begin(), data.end(), txpkt.begin() + 2);
    std::cout << "sending" << std::endl;
    print_vec(data);
    return next_transport_layer->send(txpkt);
}
H4Transport::~H4Transport() noexcept
{
    delete next_transport_layer;
}

void H4Transport::data_handler(const uint8_t *data, const size_t length) noexcept
{
    // std::lock_guard<std::mutex>lk(rx_state_mutex);//一箇所(受信スレッド)からしかこのメソッドは呼ばれないので，mutexかけなくても良い
    size_t data_remain = length;
    size_t read_ptr    = 0;
    std::cout << "h4 uart rx handler" << std::endl;
    if (length == 0)
    {
        std::cout << "data empty!" << std::endl;
        return;
    }
    //下位レイヤから複数のパケットを含むデータ塊が渡されることがあるので，可能な限りパースする
    while (data_remain > 0)
    {
        if (rx_state == RX_STATE_WAIT_HEADER)
        {
            rx_header = data[read_ptr++];
            data_remain--;
            rx_state = RX_STATE_RECEIVING_HEADER;
        }
        if (rx_state == RX_STATE_RECEIVING_HEADER)
        {
            if (read_ptr < length)
            {
                rx_header |= data[read_ptr++] << 8;
                data_remain--;
                std::cout << "header rx complete:" << std::setw(4) << std::setfill('0') << std::hex
                          << rx_header << std::endl;
                rx_counter = 0;
                rx_packet.clear();
                rx_packet.reserve(rx_header);
                rx_state = RX_STATE_RECEIVING_PAYLOAD;
            }
            else
            {
                return;
            }
        }
        if (rx_state == RX_STATE_RECEIVING_PAYLOAD)
        {
            int to_copy = std::min((int)(length - read_ptr), (int)(rx_header - rx_counter));
            for (int i = 0; i < to_copy; i++)
            {
                rx_packet.push_back(data[read_ptr++]);
            }
            rx_counter += to_copy;
            data_remain -= to_copy;
            if (rx_counter == rx_header)
            {
                // payload complete
                std::cout << "rx payload complete" << std::endl;
                print_vec(rx_packet);
                rx_state = RX_STATE_WAIT_HEADER;
                upperDataCallback(rx_packet.data(), rx_packet.size());
                std::cout << "rx callback finished" << std::endl;
            }
        }
    }
}
