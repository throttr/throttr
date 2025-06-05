// Copyright (C) 2025 Ian Torres
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU Affero General Public License for more details.
//
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.

#ifndef THROTTR_TRANSPORT_HPP
#define THROTTR_TRANSPORT_HPP

#ifdef ENABLED_FEATURE_UNIX_SOCKETS
#include <boost/asio/local/stream_protocol.hpp>
using transport_acceptor = boost::asio::local::stream_protocol::acceptor;
using transport_socket = boost::asio::local::stream_protocol::socket;
using transport_endpoint = boost::asio::local::stream_protocol::endpoint;
#else
#include <boost/asio/ip/tcp.hpp>
using transport_acceptor = boost::asio::ip::tcp::acceptor;
using transport_socket = boost::asio::ip::tcp::socket;
using transport_endpoint = boost::asio::ip::tcp::endpoint;
#endif

#endif // THROTTR_TRANSPORT_HPP
