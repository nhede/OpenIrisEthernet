#include "streamServerEth.hpp"
#include <esp_wifi.h>

#define ETH_PART_BOUNDARY "123456789000000000000987654321"
constexpr static const char *ETH_STREAM_HTTPOK = "HTTP/1.1 200 OK\r\nContent-Type: multipart/x-mixed-replace;boundary=" ETH_PART_BOUNDARY "\r\nTransfer-Encoding: chunked\r\n";
constexpr static const char *ETH_STREAM_CONTENT_TYPE = "Access-Control-Allow-Origin; Content-Type: multipart/x-mixed-replace; boundary=" ETH_PART_BOUNDARY "\r\n: *\r\nX-Framerate: 60\r\n";
constexpr static const char *ETH_STREAM_BOUNDARY = "\r\n24\r\n\r\n--" ETH_PART_BOUNDARY "\r\n";
constexpr static const char *ETH_STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\nX-Timestamp: %d.%06d\r\n\r\n";

esp_err_t StreamHelpersEth::stream(EthernetClient *ec)
{
    camera_fb_t * fb = NULL;
    struct timeval _timestamp;
    size_t _jpg_buf_len = 0;
    uint8_t * _jpg_buf = NULL;
    char * part_buf[128];
    char * hex_buf[32];
    boolean error_sending = false;
    int bytes_sent;
    int jpg_od, jpg_jos;
    size_t hlen;
    size_t clen;

    //ec->println("HTTP/1.1 200 OK");
    //ec->print("Content-Type: ");
    //ec->println(ETH_STREAM_CONTENT_TYPE);
    //ec->println("Access-Control-Allow-Origin: *");
    //ec->println();
    ec->print(ETH_STREAM_HTTPOK);
    ec->print(ETH_STREAM_CONTENT_TYPE);

    while(true){
        error_sending = false;
        fb = esp_camera_fb_get();
        if (!fb)
        {
            Serial.println("Camera capture failed");
            return ESP_FAIL;
        }
        else
        {
            _timestamp.tv_sec = fb->timestamp.tv_sec;
            _timestamp.tv_usec = fb->timestamp.tv_usec;
            _jpg_buf_len = fb->len;
            _jpg_buf = fb->buf;
        }

        bytes_sent = ec->write(ETH_STREAM_BOUNDARY, strlen(ETH_STREAM_BOUNDARY));
        if (bytes_sent != strlen(ETH_STREAM_BOUNDARY)) error_sending = true;

        hlen = snprintf((char *)part_buf, 128, ETH_STREAM_PART, _jpg_buf_len, _timestamp.tv_sec, _timestamp.tv_usec);
        clen = snprintf((char *)hex_buf, 32, "\r\n%x\r\n", hlen);

        bytes_sent = ec->write((const char *)hex_buf, clen);
        if (bytes_sent != clen) error_sending = true;

        bytes_sent = ec->write((const char *)part_buf, hlen);
        if (bytes_sent != hlen) error_sending = true;

        clen = snprintf((char *)hex_buf, 32, "\r\n%x\r\n", _jpg_buf_len);

        bytes_sent = ec->write((const char *)hex_buf, clen);
        if (bytes_sent != clen) error_sending = true;

        // Send JPG data in 2kb chunks
        jpg_od  = 0;
        jpg_jos = _jpg_buf_len;
        while (jpg_jos > 0)
        {
            if (jpg_jos > 2048)
            {
                bytes_sent = ec->write( (const char *)&_jpg_buf[jpg_od] , 2048);
                jpg_jos -= 2048;
                jpg_od += 2048;
            }
            else
            {
                bytes_sent = ec->write( (const char *)&_jpg_buf[jpg_od] , jpg_jos);
                jpg_jos = 0;
                jpg_od = 0;
            }
        }

        // release buffers
        if (fb)
        {
            esp_camera_fb_return(fb);
            fb = NULL;
            _jpg_buf = NULL;
        }
        else if(_jpg_buf)
        {
            free(_jpg_buf);
            _jpg_buf = NULL;
        }

        if (error_sending) break; // if client disconnected do not send any more JPG's
    }
    return ESP_OK;
}

StreamServerEth::StreamServerEth(int STREAM_PORT,
                           StateManager<WiFiState_e> *stateManager, EthernetServer *server, ProjectConfig *configManager, const std::string &_ip, bool _dhcp) : STREAM_SERVER_PORT(STREAM_PORT),
                                                                      stateManager(stateManager),
                                                                      server(server),
                                                                      configManager(configManager),
                                                                      dhcp(_dhcp) 
{
    ip.fromString(_ip.c_str());
}

int StreamServerEth::startStreamServerEth(bool wifi_enabled)
{
    uint8_t wifi_mac[6];
    esp_err_t err;
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();

    //ProjectConfig::EthConfig_t* ethconfig = configManager->getEthConfig();

    //init wifi to get mac address, so its unique for ethernet too

    if(!wifi_enabled) {
        err = esp_wifi_init(&cfg);
        if(err){
            return -1;
        }
    }

    err = esp_wifi_get_mac((wifi_interface_t)WIFI_IF_STA, wifi_mac);
    if(err){
        return -1;
    }

    mac[0] = 0x02;
    mac[1] = 0xAB;
    mac[2] = 0xCD;
    mac[3] = wifi_mac[3];
    mac[4] = wifi_mac[4];
    mac[5] = wifi_mac[5];

    if(!wifi_enabled) {
        err = esp_wifi_deinit();
        if(err){
            return -1;
        }
    }

    Serial.printf("Ethernet mac: %02x:%02x:%02x:%02x:%02x:%02x\r\n", mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);

    // start the Ethernet connection and the server:
    Ethernet.init(2);
    
    if(dhcp)
    {
        Ethernet.begin(mac);
    }
    else
    {
        Ethernet.begin(mac, ip); 
    }

    // Check for Ethernet hardware present
    if (Ethernet.hardwareStatus() == EthernetNoHardware)
    {
        Serial.println("Ethernet shield was not found.  Sorry, can't run without hardware. :(");
        return 0;
    }
    while (Ethernet.linkStatus() == LinkOFF)
    {
        Serial.println("Ethernet cable is NOT connected.");
        return 0;   
    }

    Serial.println("Ethernet cable is now connected.");
  
    server->begin(STREAM_SERVER_PORT);
    delay(500);
    Serial.print("Ethernet server is at: ");
    Serial.print(Ethernet.localIP());
    Serial.printf(":%d\r\n", STREAM_SERVER_PORT);

    return 0;
}

void StreamServerEth::loopStreamServerEth() 
{
    boolean biocr;
    boolean biolf;
    boolean doubleblank;
    boolean singleblank;

    // listen for incoming clients
    EthernetClient client = server->available();
    if (client) {
        //Serial.println("new client");
        // an http request ends with a blank line
        doubleblank = false;
        singleblank = true;
        biocr = false;
        biolf = false;

        while (client.connected())
        {
            if (client.available())
            {
                char c = client.read();
                //Serial.write(c);

                if (c == '\n') //LF
                {
                    if (biolf)
                    {
                        doubleblank = true; // \n\n
                        singleblank = true;
                    }
                    if (biocr)
                    {
                        if (singleblank) doubleblank = true; // \n\r\n
                        singleblank = true;
                    }
                    biolf = true;
                    biocr = false;
                }
                else if (c == '\r') //CR
                {
                    if (biocr)
                    {
                        doubleblank = true; //\r\r
                        singleblank = true;
                    }
                    if (biolf)
                    {
                        // \n\r
                    }
          
                    biolf = false;
                    biocr = true;          
                }
                else
                {
                    biolf = false;
                    biocr = false;
                    singleblank = false;
                    doubleblank = false;
                }

                if (doubleblank)
                {
                    StreamHelpersEth::stream(&client); // stream the jpeg's
                    singleblank = false;
                    doubleblank = false;          
                    break;
                }
            }
        }
        delay(10);
        client.stop();
        //Serial.println("client disconnected");
    }
    delay(10);
}
