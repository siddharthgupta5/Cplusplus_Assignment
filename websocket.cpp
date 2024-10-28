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
            return fail(ec, "read");

        std::string data = beast::buffers_to_string(buffer_.data());
        
        Json::Value root;
        Json::Reader reader;
        if (reader.parse(data, root))
        {
            std::cout << "Received message:" << std::endl;
            std::cout << root.toStyledString() << std::endl;
        }
        else
        {
            std::cerr << "Failed to parse JSON: " << data << std::endl;
        }

        buffer_.consume(buffer_.size());
        read();
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
