#include "stdafx.h"

extern boost::asio::io_service * G_IO = nullptr;
extern std::auto_ptr<boost::asio::io_service::work> * G_WORKER = nullptr;
extern boost::thread_group * G_TG = nullptr;
typedef SimpleWeb::Server<SimpleWeb::HTTP> HttpServer;

boost::posix_time::seconds *G_TickInterval = nullptr; 
boost::asio::deadline_timer * timer = nullptr;

int G_IntervalSec = 10;
int G_HttpSrvPort = 8081;
std::string G_UrlRequest = "";
std::string G_ContextNameReq = "";

int sumUsage()
{
	nvmlReturn_t result;
	unsigned int device_count, i;
	unsigned int pAllUsage = 0;
	result = nvmlInit();
	if (NVML_SUCCESS != result)
	{
		printf("Failed to initialize NVML: %s\n", nvmlErrorString(result));
		return 0;
	}

	result = nvmlDeviceGetCount(&device_count);
	if (NVML_SUCCESS != result)
	{
		printf("Failed to query device count: %s\n", nvmlErrorString(result));
		goto Error;
	}

	for (i = 0; i < device_count; i++)
	{
		nvmlDevice_t device;

		result = nvmlDeviceGetHandleByIndex(i, &device);
		if (NVML_SUCCESS != result)
		{
			printf("Failed to get handle for device %i: %s\n", i, nvmlErrorString(result));
			goto Error;
		}

		unsigned int _pUsage;
		result = nvmlDeviceGetPowerUsage(device, &_pUsage);
		if (NVML_SUCCESS != result)
		{
			printf("Failed to get power usage of device %i: %s\n", i, nvmlErrorString(result));
			//goto Error;
		}

		pAllUsage += _pUsage;
	}

	result = nvmlShutdown();
	if (NVML_SUCCESS != result)
		printf("Failed to shutdown NVML: %s\n", nvmlErrorString(result));

	return pAllUsage;

Error:
	result = nvmlShutdown();
	if (NVML_SUCCESS != result)
		printf("Failed to shutdown NVML: %s\n", nvmlErrorString(result));
	return 0;
}

inline  int start_WebServer()
{
	HttpServer server(G_HttpSrvPort, 4);
	server.default_resource["GET"] = [](HttpServer::Response& response, std::shared_ptr<HttpServer::Request> request)
	{
		size_t sp = request->path.find_first_of('/', 1);
		if (sp != std::string::npos)
		{
			std::string cmd(request->path.begin(), request->path.begin() + sp);
			if (cmd.compare("/pwusage") == 0)
			{
				unsigned int pAllUsage = sumUsage();
				if (pAllUsage > 0)
				{
					int length = (int)(log10(pAllUsage) + 1);
					response.clear();
					response << "HTTP/1.1 200 OK\r\nContent-Length: " << length << "\r\n\r\n" << pAllUsage;
				}
				else
				{
					response.clear();
					response << "HTTP/1.1 200 OK\r\nContent-Length: 1\r\n\r\n0";
				}

				return;
			}
		}
		std::string content = "Could not open path " + request->path;
		response.clear();
		response << "HTTP/1.1 400 Bad Request\r\nContent-Length: " << content.length() << "\r\n\r\n" << content;
	};

	std::thread server_thread([&server]() {
		server.start();
	});

	printf("HTTP server started port %d\r\n", G_HttpSrvPort);
	server_thread.join();

	return 0;
}

void tick(const boost::system::error_code& /*e*/) {
	std::vector<BYTE> _buffer_content;
	std::vector<BYTE> _buffer_header;
	DLStream  _content(_buffer_content);
	DLStream  _headers(_buffer_header);
	DOWNLOAD_DATA _dl_data = { _headers,_content };
	std::stringstream data_req;
	unsigned int pAllUsage = sumUsage();
	boost::posix_time::ptime tickcache = boost::posix_time::microsec_clock::local_time();
	data_req << G_UrlRequest;
	data_req << "&pc=" << G_ContextNameReq;
	data_req << "&milwatt=" << pAllUsage;
	data_req << "&t=" << tickcache.time_of_day().ticks();
	std::cout << data_req.str() << std::endl;
	if (DownloadURLContent(data_req.str(), _dl_data))
	{
		_content.flush();
		_headers.flush();
		if (_buffer_content.size() >= 3)
		{
			if (_buffer_content[0] == 239)
			{
				_buffer_content.erase(_buffer_content.begin(), _buffer_content.begin() + 3);
			}
			if (_buffer_content.size() > 0)
			{
				try
				{
					std::string s(_buffer_content.begin(), _buffer_content.end());
					std::cout << s << std::endl;
				
				}
				catch (std::exception& e)
				{
					printf("ERROR  : %s \r\n", e.what());
				}
			}
		}
	}

	timer->expires_at(timer->expires_at() + *G_TickInterval);
	timer->async_wait(tick);
}


int main()
{
	boost::property_tree::ptree pt;
	boost::property_tree::ini_parser::read_ini("fuckjia.ini", pt);
	G_UrlRequest = pt.get<std::string>("config.url", "https://dev.xxx.in.th/test/watt.php?t=1");
	G_ContextNameReq = pt.get<std::string>("config.context","RIG1");
	G_IntervalSec = pt.get<int>("config.intervalsec",10);
	G_HttpSrvPort = pt.get<int>("config.httpsrvport", 8081);
	G_TickInterval = new boost::posix_time::seconds(G_IntervalSec);

	SetConsoleTitleA((LPCSTR)G_ContextNameReq.c_str());

	G_IO = new boost::asio::io_service();
	G_WORKER = new std::auto_ptr<boost::asio::io_service::work>(new boost::asio::io_service::work(*G_IO));
	G_TG = new boost::thread_group();
	for (int i = 0; i < 4; ++i)
	{
		G_TG->create_thread(
			[&]()
		{
			G_IO->run();
		});
	}
	G_IO->dispatch(start_WebServer);

	timer = new boost::asio::deadline_timer(*G_IO, *G_TickInterval);
	timer->async_wait(tick);

	getchar();
	return 1;
}
