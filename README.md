#Embedded toit module for ESP IDF
This project enables using toit alongside a standard ESP IDF project. 
The project is still rough around the edges. The features
* An ESP IDF component
* Interoperability via RCP messages
* A simplified C++ Api for setting up the interoperability 
* An example ping pong app

###Setup instructions
This has only been tested on MacOS 12
1. Clone this repo.
2. Run `git submodule update --init --recursive`
3. Change to the toit directory and run `make`, please consult 
https://github.com/toitlang/toit for details of prerequisites for you platform. 
4. Run `sh toit/third_party/esp-idf/install.sh`
5. Copy the example directory out of the repo, we will call it *project_dir* 
from now on, as an example `cp -r esp-idf/example .`
6. Run `mkdir $project_dir/components`
7. Move this repo inside `mv esp-toit $project_dir/components/` 
8. Source the esp-idf shell setup: `. $project_dir/components/esp-toit/toit/third_party/esp-idf/export.sh`
9. In the *project_dir* run `idf.py build`
10. And lastly flash to you device `idf.py -p <port> flash monitor`


minimum needed changes to the esp32 config (More is needed, havent figured it out yet)

* enable nimble ble
* enable ethernet w5500
* Thread local sotrage pointers set to 3
* Custom partition table
* Serial Flash Config: flash size 4mb