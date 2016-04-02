# automatond
Heart of the arduimaton network, and the only TCP/IP connected node.

- Adds functionality which leverages RF24Network fragmented payload feature that allows for 144 byte payloads sent as fragments
- Uses [Blake2s](https://blake2.net/) keyed hash algorithm to sign all messages with a symmetric key
- Broadcasts signed heartbeats to RF24Network nodes to maintain a time reference
- Configured `SECRET_KEY` and `DIGEST_SIZE` must match on all nodes to effectively validates hashes.
- All payloads comprised of three base64 encoded strings delimeted by periods
- heartbeat.payload.hash `MTQ1OTU3MDUzNA==.MTQ1OTU3MDUzNA==.MzNkZWNlYTFkOTRm` = `1459570534.1459570534.33decea1d94f`
- Payloads currently expire within `1` second of being sent, to protect against replay attacks

## Dependencies
- [RapidJSON](https://github.com/miloyip/rapidjson)
- [CryptoC++](https://www.cryptopp.com/) for base64 encoding/decoding
- [BLAKE2](https://blake2.net/) via [libb2](https://github.com/BLAKE2/libb2)
- [libmosquitto](http://mosquitto.org/) (mqtt client)

## Optional
- [Mini OLED Display Driver](https://github.com/hallard/ArduiPi_OLED)

## Installing Deps on Pi running Debian Jessie derivative
```
sudo apt-get update
sudo apt-get upgrade

#rapid json
git clone https://github.com/miloyip/rapidjson
cp -R rapidjson/include/rapidjson /usr/local/include

# cryptocpp 
sudo apt-get install libcrypto++9 \
libcrypto++9-dbg libcrypto++-dev \
libcrypto++-doc libcrypto++-utils

# libb2
git clone https://github.com/BLAKE2/libb2
cd libb2/
./autogen.sh
./configure
make
sudo make install

# libmosquitto
# add repo for latest version of mosquitto
wget http://repo.mosquitto.org/debian/mosquitto-repo.gpg.key
sudo apt-key add mosquitto-repo.gpg.key
cd /etc/apt/sources.list.d/
sudo wget http://repo.mosquitto.org/debian/mosquitto-jessie.list
# sudo wget http://repo.mosquitto.org/debian/mosquitto-wheezy.list
apt-get update
#check to see packages are there.
apt-cache search mosquitto 
sudo apt-get install libmosquitto-dev \
libmosquitto1 libmosquitto1-dbg \
libmosquittopp1 libmosquittopp1-dbg

# RF24 & RF24Network
# ensure SPI is enabled
#create dir for RF24 Libs
mkdir ~/rf24libs && cd ~/rf24libs
git clone https://github.com/tmrh20/RF24.git RF24 
cd RF24
sudo make install
cd ~/rf24libs  
git clone https://github.com/tmrh20/RF24Network.git RF24Network
cd RF24Network
sudo make install

# For OLED Output, not required.
# Adafruit OLED Driver for Pi - https://hallard.me/adafruit-oled-display-driver-for-pi/
# ensure I2c is enabled.
sudo apt-get install build-essential git-core libi2c-dev i2c-tools lm-sensors
git clone https://github.com/hallard/ArduiPi_OLED
cd ArduiPi_OLED
sudo make
```