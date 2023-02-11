#pragma once
#ifndef STREAM_SERVER_ETH_HPP
#define STREAM_SERVER_ETH_HPP
#include <Arduino.h>
#include <WiFi.h>
#include <ethernet/EthernetSPI2.h>
#include "data/StateManager/StateManager.hpp"
#include <data/config/project_config.hpp>

// Camera includes
#include "esp_camera.h"
#include "esp_http_server.h"
#include "esp_timer.h"
#include "fb_gfx.h"
#include "img_converters.h"

namespace StreamHelpersEth
{
	esp_err_t stream(EthernetClient *ec, bool &hdrIsSent);
}
class StreamServerEth
{

private:
	httpd_handle_t camera_stream = nullptr;
	int STREAM_SERVER_PORT;
	StateManager<WiFiState_e> *stateManager;
	EthernetServer *server;
	uint8_t mac[6];
	IPAddress ip;
	bool dhcp;
	ProjectConfig *configManager;
	bool isStreaming;
	bool hdrIsSent;
	EthernetClient client;

public:
	StreamServerEth(int STREAM_PORT,
				 StateManager<WiFiState_e> *stateManager, EthernetServer *server, ProjectConfig *configManager, const std::string &_ip, bool _dhcp);
	int startStreamServerEth(bool wifi_enabled); //if wifi disabled, init it to get the mac
	void loopStreamServerEth();
};

#endif // STREAM_SERVER_ETH_HPP
