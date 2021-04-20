#ifndef H4_TRANSPORT_H
#define H4_TRANSPORT_H
#include "transport.h"
#include "uart_transport.h"
#include <atomic>
#include <mutex>
#include <vector>

typedef enum{
    RX_STATE_WAIT_HEADER,
    RX_STATE_RECEIVING_HEADER,
    RX_STATE_RECEIVING_PAYLOAD,
}h4_rx_state_t;
class H4Transport :public Transport
{
private:
    Transport*next_transport_layer;
    status_cb_t status_cb;
    data_cb_t data_cb;
    static const int H4_HEADER_LENGTH=2;//header is 2byte
    std::atomic_bool is_open;
    void data_handler(const uint8_t *data, const size_t length) noexcept;
    std::vector<uint8_t>rx_packet;
    uint16_t rx_header;
    uint16_t rx_counter;
    h4_rx_state_t rx_state;
    std::mutex rx_state_mutex;
    int tx_pkt_count;
    int rx_pkt_count;

public:
    H4Transport(/* args */)=delete;
    H4Transport(Transport *nextTransportLayer);
    virtual uint32_t open(const status_cb_t &status_callback, const data_cb_t &data_callback,
                          const log_cb_t &log_callback) noexcept override;
    virtual uint32_t close() noexcept override;

    virtual uint32_t send(const std::vector<uint8_t> &data) noexcept override;
    ~H4Transport()noexcept override;
};





#endif