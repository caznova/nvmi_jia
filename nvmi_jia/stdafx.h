// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

#include "targetver.h"

#include <stdio.h>
#include <tchar.h>

#include <nvml.h>
#include <cuda_runtime_api.h>
// TODO: reference additional headers your program requires here

#define CURL_STATICLIB

#define BOOST_SPIRIT_THREADSAFE
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/ini_parser.hpp>
#include <boost/thread.hpp>
#include <boost/iostreams/stream.hpp>
#include <boost/filesystem.hpp>
#include <boost/iostreams/device/array.hpp>
#include <boost/iostreams/filtering_stream.hpp>
#include <boost/iostreams/device/back_inserter.hpp>
#include <boost/asio.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/thread/sync_queue.hpp>
#include <boost/foreach.hpp>
#include <chrono>
#include <boost/algorithm/string/join.hpp>
#include <boost/range/adaptor/transformed.hpp>

#include <curl/curl.h>
#include <curl/easy.h>

extern boost::asio::io_service * G_IO;
extern std::auto_ptr<boost::asio::io_service::work> * G_WORKER;
extern boost::thread_group * G_TG;

#include "server_http.hpp"
#include "cChttpClient.h"

#pragma comment(lib, "wsock32.lib")
#pragma comment(lib, "wldap32.lib")
#pragma comment(lib, "ws2_32.lib")

#pragma comment(lib, "..\\libcurl\\lib\\static-release-x64\\libcurl_a.lib")
#pragma comment(lib, "..\\openssl\\lib\\libeay32.lib")
#pragma comment(lib, "..\\openssl\\lib\\ssleay32.lib")