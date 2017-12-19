#pragma once
#include "stdafx.h"

typedef boost::iostreams::stream< boost::iostreams::back_insert_device<std::vector<BYTE>>> DLStream;
typedef struct _DATA
{
	DLStream & DataHeader;
	DLStream & DataContent;
} DOWNLOAD_DATA;

static size_t write_data(void *ptr, size_t size, size_t nmemb, void *stream)
{
	DLStream * pStr = ((DLStream*)stream);
	if (size * nmemb)
	{
		pStr->write((BYTE*)ptr, size * nmemb);
	}
	return nmemb * size;
}

static bool DownloadURLContent(std::string pUrl, DOWNLOAD_DATA & pDownloadData)
{
	CURL *curl_handle;
	if (curl_global_init(CURL_GLOBAL_ALL) != CURLE_OK)
		return false;
	if ((curl_handle = curl_easy_init()) == NULL)
		return false;

	char stdError[CURL_ERROR_SIZE] = { '\0' };
	if (curl_easy_setopt(curl_handle, CURLOPT_TIMEOUT, 5) != CURLE_OK)
		goto clean_up;
	if (curl_easy_setopt(curl_handle, CURLOPT_ERRORBUFFER, stdError) != CURLE_OK)
		goto clean_up;
	if (curl_easy_setopt(curl_handle, CURLOPT_URL, pUrl.c_str()) != CURLE_OK)
		goto clean_up;
	if (curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, write_data) != CURLE_OK)
		goto clean_up;
	if (curl_easy_setopt(curl_handle, CURLOPT_HEADERFUNCTION, write_data) != CURLE_OK)
		goto clean_up;
	if (curl_easy_setopt(curl_handle, CURLOPT_WRITEHEADER, (void *)&(pDownloadData.DataHeader)) != CURLE_OK)
		goto clean_up;
	if (curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)&(pDownloadData.DataContent)) != CURLE_OK)
		goto clean_up;
	if (curl_easy_setopt(curl_handle, CURLOPT_NOPROGRESS, 1L) != CURLE_OK)
		goto clean_up;
	if (curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYHOST, 0L) != CURLE_OK)
		goto clean_up;
	if (curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYPEER, 0L) != CURLE_OK)
		goto clean_up;
	//if (curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYSTATUS, 0L) != CURLE_OK)
	//	goto clean_up;
	int cr = curl_easy_perform(curl_handle);
	if (cr != CURLE_OK)
		goto clean_up;

	curl_easy_cleanup(curl_handle);
	curl_global_cleanup();
	return true;
clean_up:
	curl_easy_cleanup(curl_handle);
	curl_global_cleanup();
	return false;
}
