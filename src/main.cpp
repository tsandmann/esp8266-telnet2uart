/*
  WiFiTelnetToSerial - Example Transparent UART to Telnet Server for esp8266

  Copyright (c) 2015 Hristo Gochkov. All rights reserved.
  This file is part of the ESP8266WiFi library for Arduino environment.

  Modified by Timo Sandmann for use with ct-Bot software framework,
  ported to C++14.

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include <ESP8266WiFi.h>
#include <algorithm>
#include <cstdint>
#include <cstddef>

#include "config.h" // if config.h is missing, copy config.h.in to config.h and adjust for your wifi!


static WiFiServer server { 23 };
static WiFiClient serverClients[MAX_SRV_CLIENTS];
static uint32_t connection_time[MAX_SRV_CLIENTS];
static uint8_t serial_buf[BUFFER_SIZE];

void setup() {
    WiFi.begin(ssid, password);
    for (uint8_t i { 0 }; WiFi.status() != WL_CONNECTED && i < 20; ++i) {
        ::delay(500);
    }
    if (WiFi.status() != WL_CONNECTED) {
        while (true) {
            ::delay(500);
        }
    }

    /* start UART and server */
    Serial.setRxBufferSize(BUFFER_SIZE);
    Serial.begin(UART_BAUDRATE);

    server.begin();
    server.setNoDelay(true);
}

void loop() {
    /* check if there are any new clients */
    if (server.hasClient()) {
        for (uint8_t i { 0 }; i < MAX_SRV_CLIENTS; ++i) {
            /* find free/disconnected spot */
            if (!serverClients[i].connected()) {
                serverClients[i].stop();

                serverClients[i] = server.available();
                connection_time[i] = ::millis();

                /* suppress local echo */
                serverClients[i].write("\xff\xfb\x01", 3);
                serverClients[i].flush();
                serverClients[i].write("\xff\xfe\x01", 3);
                serverClients[i].flush();
                serverClients[i].write("\xff\xfe\x22", 3);
                serverClients[i].flush();

                serverClients[i].write("ESP8266 telnet-2-serial ready.\r\n");
                serverClients[i].flush();
                return;
            }
        }
        /* no free/disconnected spot so reject */
        WiFiClient serverClient = server.available();
        serverClient.stop();
    }
    /* check clients for data */
    for (uint8_t i { 0 }; i < MAX_SRV_CLIENTS; ++i) {
        if (serverClients[i].connected()) {
            while (serverClients[i].available()) {
                /* get data from the telnet client and push it to the UART */
                const uint32_t dt { ::millis() - connection_time[i] };
                const uint8_t tmp { static_cast<uint8_t>(serverClients[i].read()) };
                if (dt < TELNET_DISCARD_TIME && tmp == 0xff) {
                    /* discard telnet protocl traffic during the first second */
                    while (serverClients[i].available() < 2) {
                        ::delay(0);
                    }
                    serverClients[i].read();
                    serverClients[i].read();
                } else {
                    Serial.write(tmp);
                }
            }
        }
    }
    /* check UART for data */
    if (Serial.available()) {
        const size_t len { std::min(static_cast<size_t>(Serial.available()), BUFFER_SIZE) };
        Serial.readBytes(serial_buf, len);
        /* push UART data to all connected telnet clients */
        for (uint8_t i { 0 }; i < MAX_SRV_CLIENTS; ++i) {
            if (serverClients[i].connected()) {
                serverClients[i].write(serial_buf, len);
                ::delay(0);
            }
        }
    }
}
