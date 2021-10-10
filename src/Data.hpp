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

#ifndef DATA_HPP_
#define DATA_HPP_

#include <hd.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <memory>
#include <string>
#include <vector>
#include <filesystem>
#include <set>
#include <optional>

#include "Config.hpp"
#include "const.h"
#include "Device.hpp"
#include "vita/string.hpp"

class Data
{
public:
    Data();
    ~Data() = default;

    struct Environment
    {
            std::string PMCachePath {MHWD_PM_CACHE_DIR};
            std::string PMConfigPath {MHWD_PM_CONFIG};
            std::string PMRootPath {MHWD_PM_ROOT};
            bool syncPackageManagerDatabase = true;
    };

    Environment environment;
    std::vector<std::shared_ptr<Device>> USBDevices;
    std::vector<std::shared_ptr<Device>> PCIDevices;
    std::vector<Config> installedUSBConfigs;
    std::vector<Config> installedPCIConfigs;
    std::vector<Config> allUSBConfigs;
    std::vector<Config> allPCIConfigs;
    std::vector<Config> invalidConfigs;

    void updateInstalledConfigData();
    void getAllDevicesOfConfig(const Config& config, std::vector<std::shared_ptr<Device>>& foundDevices);

    std::vector<Config> getAllDependenciesToInstall(const Config&  config);
    void getAllDependenciesToInstall(const Config& config,
            std::vector<Config>& installedConfigs,
            std::vector<Config> *depends);
    std::optional<Config> getDatabaseConfig(const std::string& configName,
            const std::string& configType);
    std::vector<Config> getAllLocalConflicts(const Config&  config);
    std::vector<Config> getAllLocalRequirements(const Config& config);

private:
    void getAllDevicesOfConfig(const std::vector<std::shared_ptr<Device>>& devices,
            const Config& config, std::vector<std::shared_ptr<Device>>& foundDevices);
    void fillInstalledConfigs(std::vector<Config>& configs, const std::set<std::filesystem::path>& configPaths, const std::string& type);
    void fillDevices(hw_item hw, std::vector<std::shared_ptr<Device>>& devices);
    void fillAllConfigs(const std::string& type);
    void setMatchingConfigs(const std::vector<std::shared_ptr<Device>>& devices,
            std::vector<Config>& configs, bool setAsInstalled);
    void setMatchingConfig(const Config& config, const std::vector<std::shared_ptr<Device>>& devices,
            bool setAsInstalled);
    void addConfigSorted(std::vector<Config>& configs, const Config& newConfig);
    std::set<std::filesystem::path> getRecursiveDirectoryFileList(const std::filesystem::path& directoryPath,
            const std::string& onlyFilename = "");

    Vita::string getRightConfigPath(const Vita::string& str, const Vita::string& baseConfigPath);
    void updateConfigData();

    Vita::string from_Hex(uint16_t hexnum, int fill);
    std::string from_CharArray(char* c);
};

#endif /* DATA_HPP_ */
