#include "stdafx.h"

extern boost::asio::io_service * G_IO = nullptr;
extern std::auto_ptr<boost::asio::io_service::work> * G_WORKER = nullptr;
extern boost::thread_group * G_TG = nullptr;
typedef SimpleWeb::Server<SimpleWeb::HTTP> HttpServer;

boost::posix_time::seconds *G_TickInterval = nullptr; 
boost::asio::deadline_timer * timer = nullptr;

typedef struct _GPUINFOS
{
	size_t Count;
	size_t SumMiliWatt;
	std::vector<std::string> DeviceNames;
	std::vector<unsigned int> MiliWattUsage;
	std::vector<unsigned int> Temperatures;
	std::vector<unsigned int> FanSpeeds;
}GPUINFOS;


int G_IntervalSec = 10;
int G_HttpSrvPort = 8081;
int G_RestartOnGPULost = 0;
std::string G_UrlRequest = "";
std::string G_ContextNameReq = "";
GPUINFOS G_GPUInfo;


void RestartWindows()
{
	HANDLE hToken;
	if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken))
	{
		printf("OpenProcessToken failed!\r\n");
		return;
	}
	TOKEN_PRIVILEGES tkp;
	LookupPrivilegeValue(NULL, SE_SHUTDOWN_NAME, &tkp.Privileges[0].Luid);

	tkp.PrivilegeCount = 1;   
	tkp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

	AdjustTokenPrivileges(hToken, FALSE, &tkp, 0, (PTOKEN_PRIVILEGES)NULL, 0);

	if (GetLastError() != ERROR_SUCCESS)
	{
		printf("AdjustTokenPrivileges failed\r\n");
		return;
	}

	printf("Restarting...\r\n");
	ExitWindowsEx(EWX_REBOOT | EWX_FORCE, SHTDN_REASON_MINOR_MAINTENANCE | SHTDN_REASON_FLAG_PLANNED);
}

bool InitGPULists()
{
	nvmlReturn_t result;
	unsigned int device_count, i;
	nvmlPciInfo_t pci;
	char name[NVML_DEVICE_NAME_BUFFER_SIZE];
	std::string vendor = "";
	result = nvmlInit();
	if (NVML_SUCCESS != result)
	{
		printf("Failed to initialize NVML: %s\r\n", nvmlErrorString(result));
		return false;
	}

	result = nvmlDeviceGetCount(&device_count);
	if (NVML_SUCCESS != result)
	{
		printf("Failed to query device count: %s\r\n", nvmlErrorString(result));
		goto Error;
	}

	G_GPUInfo.Count = device_count;

	for (i = 0; i < device_count; i++)
	{
		nvmlDevice_t device;

		result = nvmlDeviceGetHandleByIndex(i, &device);
		if (NVML_SUCCESS != result)
		{
			printf("Failed to get handle for device %i: %s\r\n", i, nvmlErrorString(result));
			goto Error;
		}

		result = nvmlDeviceGetName(device, name, NVML_DEVICE_NAME_BUFFER_SIZE);
		if (NVML_SUCCESS != result)
		{
			printf("Failed to get name of device %i: %s\r\n", i, nvmlErrorString(result));
			goto Error;
		}

		result = nvmlDeviceGetPciInfo(device, &pci);
		if (NVML_SUCCESS != result)
		{
			printf("Failed to get pci info for device %i: %s\n", i, nvmlErrorString(result));
			goto Error;
		}

		//printf("%d. %s [%04X]\n", i, name, pci.pciSubSystemId & 0xFFFF);
		switch (pci.pciSubSystemId & 0xFFFF)
		{
			case 0x10DE: { vendor = "NVIDIA"; break; }
			case 0x1043: { vendor = "ASUS"; break; }
			case 0x1458: { vendor = "GIGABYTE"; break; }
			case 0x1462: { vendor = "MSI"; break; }
			case 0x19DA: { vendor = "ZOTAC"; break; }
			case 0x3842: { vendor = "EVGA"; break; }
			case 0x1028: { vendor = "DELL"; break; }
			case 0x103C: { vendor = "HP"; break; }
			case 0x10B0: { vendor = "GAINWARD"; break; }
			case 0x196E: { vendor = "PNY"; break; }
			case 0x174B: { vendor = "SAPPIRE"; break; }
			case 0x1019: { vendor = "ELITEGROUP"; break; }
			case 0x1569: { vendor = "PALIT"; break; }
			case 0x1ACC: { vendor = "POV"; break; }
			case 0x1682: { vendor = "XFX"; break; }
			case 0x19F1: { vendor = "BFG"; break; }
			case 0x107D: { vendor = "LEADTEK"; break; }
			case 0x7377: { vendor = "COLORFUL"; break; }
			default: { vendor = "UNKNOW"; break; }
		}

		std::string _name = vendor + " " + name;
		G_GPUInfo.DeviceNames.push_back(_name);
		G_GPUInfo.MiliWattUsage.push_back(0);
		G_GPUInfo.Temperatures.push_back(0);
		G_GPUInfo.FanSpeeds.push_back(0);
	}

	result = nvmlShutdown();
	if (NVML_SUCCESS != result)
		printf("Failed to shutdown NVML: %s\r\n", nvmlErrorString(result));

	return true;

Error:
	result = nvmlShutdown();
	if (NVML_SUCCESS != result)
		printf("Failed to shutdown NVML: %s\r\n", nvmlErrorString(result));
	return false;
}

bool CollectGPUInfo()
{
	nvmlReturn_t result;
	unsigned int device_count, i;
	unsigned int pSumMiliWattUsage = 0;
	result = nvmlInit();
	if (NVML_SUCCESS != result)
	{
		printf("Failed to initialize NVML: %s\r\n", nvmlErrorString(result));
		return false;
	}

	result = nvmlDeviceGetCount(&device_count);
	if (NVML_SUCCESS != result)
	{
		printf("Failed to query device count: %s\r\n", nvmlErrorString(result));
		goto Error;
	}

	for (i = 0; i < device_count; i++)
	{
		nvmlDevice_t device;

		result = nvmlDeviceGetHandleByIndex(i, &device);
		if (NVML_SUCCESS != result)
		{
			printf("Failed to get handle for device %i: %s\r\n", i, nvmlErrorString(result));
			if (result == NVML_ERROR_GPU_IS_LOST)
			{
				if (G_RestartOnGPULost == 1)
				{
					G_IO->dispatch(RestartWindows);
				}
			}
			goto Error;
		}

		unsigned int _pUsage;
		result = nvmlDeviceGetPowerUsage(device, &_pUsage);
		if (NVML_SUCCESS != result)
		{
			printf("Failed to get power usage of device %i: %s\r\n", i, nvmlErrorString(result));
			_pUsage = 0;
		}
		else
		{
			G_GPUInfo.MiliWattUsage.at(i) = _pUsage;
		}

		unsigned int _pTemp;
		result = nvmlDeviceGetTemperature(device, nvmlTemperatureSensors_t::NVML_TEMPERATURE_GPU , &_pTemp);
		if (NVML_SUCCESS != result)
		{
			printf("Failed to get temperature of device %i: %s\r\n", i, nvmlErrorString(result));
			_pTemp = 0;
		}
		else
		{
			G_GPUInfo.Temperatures.at(i) = _pTemp;
		}

		unsigned int _pFanSpeed;
		result = nvmlDeviceGetFanSpeed(device, &_pFanSpeed);
		if (NVML_SUCCESS != result)
		{
			printf("Failed to get fan speed of device %i: %s\r\n", i, nvmlErrorString(result));
			_pFanSpeed = 0;
		}
		else
		{
			G_GPUInfo.FanSpeeds.at(i) = _pFanSpeed;
		}

		pSumMiliWattUsage += _pUsage;
	}

	result = nvmlShutdown();
	if (NVML_SUCCESS != result)
		printf("Failed to shutdown NVML: %s\r\n", nvmlErrorString(result));

	G_GPUInfo.SumMiliWatt = pSumMiliWattUsage;
	return true;

Error:
	result = nvmlShutdown();
	if (NVML_SUCCESS != result)
		printf("Failed to shutdown NVML: %s\r\n", nvmlErrorString(result));
	return false;
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
				bool pRet = CollectGPUInfo();
				if (pRet == true && G_GPUInfo.SumMiliWatt > 0)
				{
					int length = (int)(log10(G_GPUInfo.SumMiliWatt) + 1);
					response.clear();
					response << "HTTP/1.1 200 OK\r\nContent-Length: " << length << "\r\n\r\n" << G_GPUInfo.SumMiliWatt;
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
	bool pRet = CollectGPUInfo();
	if (pRet == true)
	{
		//boost::posix_time::ptime tickcache = boost::posix_time::microsec_clock::local_time();
		data_req << G_UrlRequest;
		data_req << "&pc=" << G_ContextNameReq;
		data_req << "&milwatt=" << G_GPUInfo.SumMiliWatt;

		std::string device_name_all = boost::algorithm::join(G_GPUInfo.DeviceNames, ",");
		std::string device_temp_all = boost::algorithm::join(G_GPUInfo.Temperatures | boost::adaptors::transformed(static_cast<std::string(*)(unsigned int)>(std::to_string)), ",");
		std::string device_fs_all = boost::algorithm::join(G_GPUInfo.FanSpeeds | boost::adaptors::transformed(static_cast<std::string(*)(unsigned int)>(std::to_string)), ",");

		CURL *curl = curl_easy_init();
		if (curl) {
			char *output = curl_easy_escape(curl, device_name_all.c_str(), (int)device_name_all.length());
			if (output) 
			{
				data_req << "&devices=" << output;
				curl_free(output);
			}
			curl_easy_cleanup(curl);
		}

		data_req << "&temps=" << device_temp_all;
		data_req << "&fans=" << device_fs_all;
		boost::posix_time::time_duration td = boost::posix_time::milliseconds(GetTickCount64());
		data_req << "&uptime=" << td.total_seconds();
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
	G_RestartOnGPULost = pt.get<int>("config.restartOnGPUlost", 0);
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
	if(G_HttpSrvPort > 0 )
		G_IO->dispatch(start_WebServer);

	InitGPULists();

	timer = new boost::asio::deadline_timer(*G_IO, *G_TickInterval);
	timer->async_wait(tick);

	getchar();
	return 1;
}
