
#include <iostream>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast/ssl.hpp>
#include <cstdlib>

#include <string>
#include <json/json.h>


namespace beast = boost::beast;
namespace http = beast::http;
namespace websocket = beast::websocket;
namespace net = boost::asio;
namespace ssl = boost::asio::ssl;
using tcp = boost::asio::ip::tcp;

class WebSocketClient : public std::enable_shared_from_this<WebSocketClient>
{
public:

    explicit WebSocketClient(net::io_context& ioc, ssl::context& ctx)
        : resolver_(ioc), ws_(ioc, ctx) {}

    void run(const std::string& host, const std::string& port, const std::string& instrument)
    {
        host_ = host;
        instrument_ = instrument;

        resolver_.async_resolve(host, port,
            beast::bind_front_handler(&WebSocketClient::on_resolve, shared_from_this()));
    }

private:
    tcp::resolver resolver_;
    websocket::stream<beast::ssl_stream<beast::tcp_stream>> ws_;
    beast::flat_buffer buffer_;
    std::string host_;
    std::string instrument_;

    void on_resolve(beast::error_code ec, tcp::resolver::results_type results)
    {
        if (ec)
            return fail(ec, "resolve");


        beast::get_lowest_layer(ws_).expires_after(std::chrono::seconds(30));

        beast::get_lowest_layer(ws_).async_connect(results,
            beast::bind_front_handler(&WebSocketClient::on_connect, shared_from_this()));
    }

    void on_connect(beast::error_code ec, tcp::resolver::results_type::endpoint_type ep)
    {
        if (ec)
            return fail(ec, "connect");

        host_ += ':' + std::to_string(ep.port());

        beast::get_lowest_layer(ws_).expires_after(std::chrono::seconds(30));

        ws_.next_layer().async_handshake(ssl::stream_base::client,
            beast::bind_front_handler(&WebSocketClient::on_ssl_handshake, shared_from_this()));
    }

    void on_ssl_handshake(beast::error_code ec)
    {
        if (ec)
            return fail(ec, "ssl_handshake");

        beast::get_lowest_layer(ws_).expires_never();


        ws_.set_option(websocket::stream_base::timeout::suggested(beast::role_type::client));


        ws_.async_handshake(host_, "/ws/api/v2",
            beast::bind_front_handler(&WebSocketClient::on_handshake, shared_from_this()));
    }

    void on_handshake(beast::error_code ec)
    {
        if (ec)
            return fail(ec, "handshake");

        subscribe_to_orderbook();
        start_heartbeat();
    }

    void subscribe_to_orderbook()
    {
        Json::Value subscription;
        subscription["jsonrpc"] = "2.0";
        subscription["id"] = 1;
        subscription["method"] = "public/subscribe";
        subscription["params"]["channels"].append("book." + instrument_ + ".100ms");

        Json::FastWriter writer;
        std::string message = writer.write(subscription);

        ws_.async_write(net::buffer(message),
            beast::bind_front_handler(&WebSocketClient::on_write, shared_from_this()));
    }

    void on_write(beast::error_code ec, std::size_t bytes_transferred)
    {
        boost::ignore_unused(bytes_transferred);

        if (ec)
            return fail(ec, "write");
        read();
    }

    void read()
    {
        ws_.async_read(buffer_,
            beast::bind_front_handler(&WebSocketClient::on_read, shared_from_this()));
    }

    void on_read(beast::error_code ec, std::size_t bytes_transferred)
    {
        boost::ignore_unused(bytes_transferred);

        if (ec)
        {
            if (ec == websocket::error::closed)
            {
                std::cout << "WebSocket connection closed. Reconnecting..." << std::endl;
                reconnect();
            }
            else
            {
                return fail(ec, "read");
            }
        }

        std::string data = beast::buffers_to_string(buffer_.data());

        Json::Value root;
        Json::Reader reader;
        if (reader.parse(data, root))
        {
            process_orderbook_data(root);
        }
        else
        {
            std::cerr << "Failed to parse JSON: " << data << std::endl;
        }

        buffer_.consume(buffer_.size());
        read();
    }

    void process_orderbook_data(const Json::Value& root)
    {
        // Extract the relevant order book data from the JSON object
        // and update your application's state accordingly
        std::cout << "Received order book data:" << std::endl;
        std::cout << root.toStyledString() << std::endl;
    }

    void reconnect()
    {
        // Implement logic to reconnect to the WebSocket server
        // and resubscribe to the order book channel
        std::cout << "Reconnecting to the WebSocket server..." << std::endl;
        run(host_, "443", instrument_);
    }

    void send_heartbeat()
    {
        // Implement logic to send a heartbeat message to the WebSocket server
        // to keep the connection alive
        Json::Value heartbeat;
        heartbeat["jsonrpc"] = "2.0";
        heartbeat["id"] = 1;
        heartbeat["method"] = "public/ping";

        Json::FastWriter writer;
        std::string message = writer.write(heartbeat);

        ws_.async_write(net::buffer(message),
            beast::bind_front_handler(&WebSocketClient::on_heartbeat, shared_from_this()));
    }

    void on_heartbeat(beast::error_code ec, std::size_t bytes_transferred)
    {
        boost::ignore_unused(bytes_transferred);

        if (ec)
            return fail(ec, "heartbeat");

        // Schedule the next heartbeat
        std::chrono::seconds heartbeat_interval(30);
        ws_.get_executor().context().post(
            [self = shared_from_this(), heartbeat_interval]() {
                self->send_heartbeat();
            }, heartbeat_interval);
    }

    void start_heartbeat()
    {
        // Start the heartbeat mechanism
        send_heartbeat();
    }

    void fail(beast::error_code ec, char const* what)
    {
        std::cerr << what << ": " << ec.message() << "\n";
    }
};

int main(int argc, char** argv)
{

    if (argc != 2)
    {
        std::cerr << "Usage: websocket_client <instrument>\n";
        std::cerr << "Example: websocket_client BTC-PERPETUAL\n";
        return EXIT_FAILURE;
    }
    const std::string instrument = argv[1];

    try
    {

        net::io_context ioc;
        ssl::context ctx{ssl::context::tlsv12_client};
        ctx.set_verify_mode(ssl::verify_peer);
        ctx.set_default_verify_paths();
        std::make_shared<WebSocketClient>(ioc, ctx)->run("www.deribit.com", "443", instrument);
        ioc.run();
    }
    catch (std::exception const& e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}