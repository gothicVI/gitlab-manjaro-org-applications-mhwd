/*
 *  This file is part of the mhwd - Manjaro Hardware Detection project
 *
 *  mhwd - Manjaro Hardware Detection
 *  Roland Singer <roland@manjaro.org>
 *  Łukasz Matysiak <december0123@gmail.com>
 *  Filipe Marques <eagle.software3@gmail.com>
 *
 *  Copyright (C) 2012 - 2016 Manjaro (http://manjaro.org)
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef PRINTER_HPP_
#define PRINTER_HPP_

#include <hd.h>

#include <string>
#include <vector>

#include "Config.hpp"
#include "Device.hpp"
#include "Enums.hpp"

class ConsoleWriter
{
public:
    void printStatus(std::string statusMsg) const;
    void printError(std::string errorMsg) const;
    void printWarning(std::string warningMsg) const;
    void printMessage(MHWD::MESSAGETYPE type, std::string str) const;
    void printHelp() const;
    void printVersion(std::string& versionMhwd, std::string& yearCopy) const;
    void listDevices(const std::vector<std::shared_ptr<Device>>& devices,
            std::string typeOfDevice) const;
    void listConfigs(const std::vector<std::shared_ptr<Config>>& configs,
            std::string header) const;
    void printAvailableConfigsInDetail(const std::string& deviceType,
            const std::vector<std::shared_ptr<Device>>& devices) const;
    void printInstalledConfigs(const std::string& deviceType,
            const std::vector<std::shared_ptr<Config>>& installedConfigs) const;
    void printConfigDetails(const Config& config) const;
    void printDeviceDetails(hw_item hw, FILE *f = stdout) const;
private:
    void printLine() const;

    const char* CONSOLE_COLOR_RESET {"\033[m"};
    const char* CONSOLE_RED_MESSAGE_COLOR {"\033[1m\033[31m"};
    const char* CONSOLE_TEXT_OUTPUT_COLOR {"\033[0;32m"};
};

#endif /* PRINTER_HPP_ */
