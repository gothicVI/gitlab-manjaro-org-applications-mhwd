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

#include "Mhwd.hpp"

#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
#include <filesystem>
#include <ranges>

#include "vita/string.hpp"

bool Mhwd::performTransaction(Config& config, MHWD::TransactionType transactionType)
{
    Transaction transaction (data_, &config, transactionType,
            arguments_.FORCE);

    // Print things to do
    if (MHWD::TransactionType::INSTALL == transactionType)
    {
        // Print conflicts
        if (!transaction.conflictedConfigs_.empty())
        {
            consoleWriter_.printError("config '" + config.name_ + "' conflicts with config(s):" +
                    gatherConfigContent(transaction.conflictedConfigs_));
            return false;
        }

        // Print dependencies
        else if (!transaction.dependencyConfigs_.empty())
        {
            consoleWriter_.printStatus("Dependencies to install: " +
                    gatherConfigContent(transaction.dependencyConfigs_));
        }
    }
    else if (MHWD::TransactionType::REMOVE == transactionType)
    {
        // Print requirements
        if (!transaction.configsRequirements_.empty())
        {
            consoleWriter_.printError("config '" + config.name_ + "' is required by config(s):" +
                    gatherConfigContent(transaction.configsRequirements_));
            return false;
        }
    }

    MHWD::Status status = performTransaction(transaction);

    switch (status)
    {
        case MHWD::Status::SUCCESS:
            break;
        case MHWD::Status::ERROR_CONFLICTS:
            consoleWriter_.printError("config '" + config.name_ +
                    "' conflicts with installed config(s)!");
            break;
        case MHWD::Status::ERROR_REQUIREMENTS:
            consoleWriter_.printError("config '" + config.name_ +
                    "' is required by installed config(s)!");
            break;
        case MHWD::Status::ERROR_NOT_INSTALLED:
            consoleWriter_.printError("config '" + config.name_ + "' is not installed!");
            break;
        case MHWD::Status::ERROR_ALREADY_INSTALLED:
            consoleWriter_.printWarning("a version of config '" + config.name_ +
                    "' is already installed!\nUse -f/--force to force installation...");
            break;
        case MHWD::Status::ERROR_NO_MATCH_LOCAL_CONFIG:
            consoleWriter_.printError("passed config does not match with installed config!");
            break;
        case MHWD::Status::ERROR_SCRIPT_FAILED:
            consoleWriter_.printError("script failed!");
            break;
        case MHWD::Status::ERROR_SET_DATABASE:
            consoleWriter_.printError("failed to set database!");
            break;
    }

    data_.updateInstalledConfigData();

    return (MHWD::Status::SUCCESS == status);
}

bool Mhwd::proceedWithInstallation(const std::string& input) const
{
    if ((input.length() == 1) && (('y' == input[0]) || ('Y' == input[0])))
    {
        return true;
    }
    else if (0 == input.length())
    {
        return true;
    }
    else
    {
        return false;
    }
}

bool Mhwd::isUserRoot() const
{
    constexpr unsigned short ROOT_UID = 0;
    if (ROOT_UID != getuid())
    {
        return false;
    }
    return true;
}

std::vector<std::string> Mhwd::checkEnvironment() const
{
    std::vector<std::string> missingDirs;
    if (!std::filesystem::exists(MHWD_USB_CONFIG_DIR))
    {
        missingDirs.emplace_back(MHWD_USB_CONFIG_DIR);
    }
    if (!std::filesystem::exists(MHWD_PCI_CONFIG_DIR))
    {
        missingDirs.emplace_back(MHWD_PCI_CONFIG_DIR);
    }
    if (!std::filesystem::exists(MHWD_USB_DATABASE_DIR))
    {
        missingDirs.emplace_back(MHWD_USB_DATABASE_DIR);
    }
    if (!std::filesystem::exists(MHWD_PCI_DATABASE_DIR))
    {
        missingDirs.emplace_back(MHWD_PCI_DATABASE_DIR);
    }

    return missingDirs;
}

std::optional<Config> Mhwd::getInstalledConfig(const std::string& configName,
        const std::string& configType)
{
    std::vector<Config>* installedConfigs;

    // Get the right configs
    if ("USB" == configType)
    {
        installedConfigs = &data_.installedUSBConfigs;
    }
    else
    {
        installedConfigs = &data_.installedPCIConfigs;
    }

    auto installedConfig = std::find_if(installedConfigs->begin(), installedConfigs->end(),
            [configName](const auto& config) {
                return configName == config.name_;
            });

    if (installedConfig != installedConfigs->end())
    {
        return *installedConfig;
    }
    return std::nullopt;
}

std::optional<Config> Mhwd::getDatabaseConfig(const std::string& configName,
        const std::string& configType)
{
    std::vector<Config>* allConfigs;

    // Get the right configs
    if ("USB" == configType)
    {
        allConfigs = &data_.allUSBConfigs;
    }
    else
    {
        allConfigs = &data_.allPCIConfigs;
    }

    auto config = std::find_if(allConfigs->begin(), allConfigs->end(),
            [configName](const auto& config) {
                return config.name_ == configName;
            });
    if (config != allConfigs->end())
    {
        return *config;
    }
    return std::nullopt;
}

std::optional<Config> Mhwd::getAvailableConfig(const std::string& configName,
        const std::string& configType)
{
    std::vector<std::shared_ptr<Device>> *devices;

    // Get the right devices
    if ("USB" == configType)
    {
        devices = &data_.USBDevices;
    }
    else
    {
        devices = &data_.PCIDevices;
    }

    for (auto&& device = devices->begin(); device != devices->end();
            ++device)
    {
        if ((*device)->availableConfigs_.empty())
        {
            continue;
        }
        else
        {
            auto& availableConfigs = (*device)->availableConfigs_;
            auto availableConfig = std::find_if(availableConfigs.begin(), availableConfigs.end(),
                    [configName](const auto& config){
                        return config.name_ == configName;
                    });
            if (availableConfig != availableConfigs.end())
            {
                return *availableConfig;
            }
        }
    }
    return std::nullopt;
}

MHWD::Status Mhwd::performTransaction(const Transaction& transaction)
{
    if ((MHWD::TransactionType::INSTALL == transaction.type_) &&
            !transaction.conflictedConfigs_.empty())
    {
        return MHWD::Status::ERROR_CONFLICTS;
    }
    if ((MHWD::TransactionType::REMOVE == transaction.type_)
            && !transaction.configsRequirements_.empty())
    {
        return MHWD::Status::ERROR_REQUIREMENTS;
    }

    // Check if already installed
    auto installedConfig = getInstalledConfig(transaction.config_->name_,
                                              transaction.config_->type_);
    MHWD::Status status = MHWD::Status::SUCCESS;

    if ((MHWD::TransactionType::REMOVE == transaction.type_)
        || (installedConfig && transaction.isAllowedToReinstall()))
    {
        if (!installedConfig)
        {
            return MHWD::Status::ERROR_NOT_INSTALLED;
        }

        consoleWriter_.printMessage(MHWD::MessageType::REMOVE_START, installedConfig->name_);
        if (MHWD::Status::SUCCESS != (status = uninstallConfig(installedConfig.value())))
        {
            return status;
        }

        consoleWriter_.printMessage(MHWD::MessageType::REMOVE_END, installedConfig->name_);


    }

    if (MHWD::TransactionType::INSTALL == transaction.type_)
    {
        // Check if already installed but not allowed to reinstall
        if (installedConfig && !transaction.isAllowedToReinstall())
        {
            return MHWD::Status::ERROR_ALREADY_INSTALLED;
        }

        // Install all dependencies first
        for (auto&& dependencyConfig = transaction.dependencyConfigs_.end() - 1;
             dependencyConfig != transaction.dependencyConfigs_.begin() - 1;
             --dependencyConfig)
        {

            consoleWriter_.printMessage(MHWD::MessageType::INSTALLDEPENDENCY_START,
                                        (*dependencyConfig).name_);
            if (MHWD::Status::SUCCESS != (status = installConfig((*dependencyConfig))))
            {
                return status;
            }
            else
            {
                consoleWriter_.printMessage(MHWD::MessageType::INSTALLDEPENDENCY_END,
                                            (*dependencyConfig).name_);
            }
        }

        consoleWriter_.printMessage(MHWD::MessageType::INSTALL_START, transaction.config_->name_);
        if (MHWD::Status::SUCCESS != (status = installConfig(*transaction.config_)))
        {
            return status;
        }
        else
        {
            consoleWriter_.printMessage(MHWD::MessageType::INSTALL_END,
                                        transaction.config_->name_);
        }

    }
    return status;

}


MHWD::Status Mhwd::installConfig(const Config& config)
{
    std::filesystem::path databaseDir;
    if ("USB" == config.type_)
    {
        databaseDir = MHWD_USB_DATABASE_DIR;
    }
    else
    {
        databaseDir = MHWD_PCI_DATABASE_DIR;
    }

    if (!runScript(config, MHWD::TransactionType::INSTALL))
    {
        return MHWD::Status::ERROR_SCRIPT_FAILED;
    }

    std::error_code ec;
    std::filesystem::copy(config.basePath_,
                          databaseDir / config.name_,
                          std::filesystem::copy_options::recursive, ec);

    if (ec)
    {
        return MHWD::Status::ERROR_SET_DATABASE;
    }

    // Installed config vectors have to be updated manual with updateInstalledConfigData(Data*)

    return MHWD::Status::SUCCESS;
}

MHWD::Status Mhwd::uninstallConfig(const Config& config)
{
    auto installedConfig = getInstalledConfig(config.name_, config.type_);
    
    // Check if installed
    if (!installedConfig)
    {
        return MHWD::Status::ERROR_NOT_INSTALLED;
    }
    if (installedConfig->basePath_ != config.basePath_)
    {
        return MHWD::Status::ERROR_NO_MATCH_LOCAL_CONFIG;
    }

    // Run script
    if (!runScript(installedConfig.value(), MHWD::TransactionType::REMOVE))
    {
        return MHWD::Status::ERROR_SCRIPT_FAILED;
    }

    std::error_code ec;
    std::filesystem::remove_all(installedConfig->basePath_,ec);

    if (ec)
    {
        return MHWD::Status::ERROR_SET_DATABASE;
    }

    // Installed config vectors have to be updated manual with updateInstalledConfigData(Data*)

    data_.updateInstalledConfigData();

    return MHWD::Status::SUCCESS;

}

bool Mhwd::runScript(const Config& config, MHWD::TransactionType operationType)
{
    std::string cmd = "exec " + std::string(MHWD_SCRIPT_PATH);

    if (MHWD::TransactionType::REMOVE == operationType)
    {
        cmd += " --remove";
    }
    else
    {
        cmd += " --install";
    }

    if (data_.environment.syncPackageManagerDatabase)
    {
        cmd += " --sync";
    }

    cmd += " --cachedir \"" + data_.environment.PMCachePath + "\"";
    cmd += " --pmconfig \"" + data_.environment.PMConfigPath + "\"";
    cmd += " --pmroot \"" + data_.environment.PMRootPath + "\"";
    cmd += " --config \"" + config.configPath_ + "\"";

    // Set all config devices as argument
    std::vector<std::shared_ptr<Device>> foundDevices;
    std::vector<std::shared_ptr<Device>> devices;
    data_.getAllDevicesOfConfig(config, foundDevices);

    for (auto&& foundDevice = foundDevices.begin();
            foundDevice != foundDevices.end(); ++foundDevice)
    {
        bool found = false;

        // Check if already in list
        for (auto&& dev = devices.begin(); dev != devices.end(); ++dev)
        {
            if ((*foundDevice)->sysfsBusID_ == (*dev)->sysfsBusID_
                    && (*foundDevice)->sysfsID_ == (*dev)->sysfsID_)
            {
                found = true;
                break;
            }
        }

        if (!found)
        {
            devices.push_back(std::shared_ptr<Device>{*foundDevice});
        }
    }

    for (auto&& dev = devices.begin(); dev != devices.end(); ++dev)
    {
        Vita::string busID = (*dev)->sysfsBusID_;

        if ("PCI" == config.type_)
        {
            std::vector<Vita::string> split = Vita::string(busID).replace(".", ":").explode(":");
            const unsigned long size = split.size();

            if (size >= 3)
            {
                // Convert to int to remove leading 0
                busID = Vita::string::toStr<int>(std::stoi(split[size - 3], nullptr, 16));
                busID += ":" + Vita::string::toStr<int>(std::stoi(split[size - 2], nullptr, 16));
                busID += ":" + Vita::string::toStr<int>(std::stoi(split[size - 1], nullptr, 16));
            }
        }

        cmd += " --device \"" + (*dev)->classID_ + "|" + (*dev)->vendorID_ + "|" + (*dev)->deviceID_
                + "|" + busID + "\"";
    }

    cmd += " 2>&1";

    FILE *in;

    if (!(in = popen(cmd.c_str(), "r")))
    {
        return false;
    }
    else
    {
        char buff[512];
        while (fgets(buff, sizeof(buff), in) != nullptr)
        {
            consoleWriter_.printMessage(MHWD::MessageType::CONSOLE_OUTPUT, buff);
        }

        int stat = pclose(in);

        if (WEXITSTATUS(stat) != 0)
        {
            return false;
        }
        else
        {
            // Only one database sync is required
            if (MHWD::TransactionType::INSTALL == operationType)
            {
                data_.environment.syncPackageManagerDatabase = false;
            }
            return true;
        }
    }
}

void Mhwd::setVersionMhwd(std::string versionOfSoftware, std::string yearCopyright)
{
    version_ = versionOfSoftware;
    year_ = yearCopyright;
}

void Mhwd::tryToParseCmdLineOptions(int argc, char* argv[], bool& autoConfigureNonFreeDriver,
        std::string& operationType, std::string& autoConfigureClassID)
{
    if (argc <= 1)
    {
        arguments_.LIST_AVAILABLE = true;
    }
    for (int nArg = 1; nArg < argc; ++nArg)
    {
        const std::string option{ argv[nArg] };
        if (("-h" == option) || ("--help" == option))
        {
            consoleWriter_.printHelp();
        }
        else if (("-v" == option) || ("--version" == option))
        {
            consoleWriter_.printVersion(version_, year_);
        }
        else if (("-f" == option) || ("--force" == option))
        {
            arguments_.FORCE = true;
        }
        else if (("-d" == option) || ("--detail" == option))
        {
            arguments_.DETAIL = true;
        }
        else if (("-la" == option) || ("--listall" == option))
        {
            arguments_.LIST_ALL = true;
        }
        else if (("-li" == option) || ("--listinstalled" == option))
        {
            arguments_.LIST_INSTALLED = true;
        }
        else if (("-l" == option) || ("--list" == option))
        {
            arguments_.LIST_AVAILABLE = true;
        }
        else if (("-lh" == option) || ("--listhardware" == option))
        {
            arguments_.LIST_HARDWARE = true;
        }
        else if ("--pci" == option)
        {
            arguments_.SHOW_PCI = true;
        }
        else if ("--usb" == option)
        {
            arguments_.SHOW_USB = true;
        }
        else if (("-a" == option) || ("--auto" == option))
        {
            if ((nArg + 3) >= argc)
            {
                throw std::runtime_error{"invalid use of option: -a/--auto\n"};
            }
            else
            {
                const std::string deviceType{ argv[nArg + 1] };
                const std::string driverType{ argv[nArg + 2] };
                const std::string classID{ argv[nArg + 3] };
                if ((("pci" != deviceType) && ("usb" != deviceType))
                        || (("free" != driverType) && ("nonfree" != driverType)))
                {
                    throw std::runtime_error{"invalid use of option: -a/--auto\n"};
                }
                else
                {
                    operationType = Vita::string{ deviceType }.toUpper();
                    autoConfigureNonFreeDriver = ("nonfree" == driverType);
                    autoConfigureClassID = Vita::string(classID).toLower().trim();
                    arguments_.AUTOCONFIGURE = true;
                    nArg += 3;
                }
            }
        }
        else if (("-ic" == option) || ("--installcustom" == option))
        {
            if ((nArg + 1) >= argc)
            {
                throw std::runtime_error{"invalid use of option: -ic/--installcustom\n"};
            }
            else
            {
                const std::string deviceType{ argv[++nArg] };
                if (("pci" != deviceType) && ("usb" != deviceType))
                {
                    throw std::runtime_error{"invalid use of option: -ic/--installcustom\n"};
                }
                else
                {
                    operationType = Vita::string{ deviceType }.toUpper();
                    arguments_.CUSTOM_INSTALL = true;
                }
            }
        }
        else if (("-i" == option) || ("--install" == option))
        {
            if ((nArg + 1) >= argc)
            {
                throw std::runtime_error{"invalid use of option: -i/--install\n"};
            }
            else
            {
                const std::string deviceType{ argv[++nArg] };
                if (("pci" != deviceType) && ("usb" != deviceType))
                {
                    throw std::runtime_error{"invalid use of option: -i/--install\n"};
                }
                else
                {
                    operationType = Vita::string{ deviceType }.toUpper();
                    arguments_.INSTALL = true;
                }
            }
        }
        else if (("-r" == option) || ("--remove" == option))
        {
            if ((nArg + 1) >= argc)
            {
                throw std::runtime_error{"invalid use of option: -r/--remove\n"};
            }
            else
            {
                const std::string deviceType{ argv[++nArg] };
                if (("pci" != deviceType) && ("usb" != deviceType))
                {
                    throw std::runtime_error{"invalid use of option: -r/--remove\n"};
                }
                else
                {
                    operationType = Vita::string{ deviceType }.toUpper();
                    arguments_.REMOVE = true;
                }
            }
        }
        else if ("--pmcachedir" == option)
        {
            if (nArg + 1 >= argc)
            {
                throw std::runtime_error{"invalid use of option: --pmcachedir\n"};
            }
            else
            {
                data_.environment.PMCachePath = Vita::string(argv[++nArg]).trim("\"").trim();
            }
        }
        else if ("--pmconfig" == option)
        {
            if (nArg + 1 >= argc)
            {
                throw std::runtime_error{"invalid use of option: --pmconfig\n"};
            }
            else
            {
                data_.environment.PMConfigPath = Vita::string(argv[++nArg]).trim("\"").trim();
            }
        }
        else if ("--pmroot" == option)
        {
            if (nArg + 1 >= argc)
            {
                throw std::runtime_error{"invalid use of option: --pmroot\n"};
            }
            else
            {
                data_.environment.PMRootPath = Vita::string(argv[++nArg]).trim("\"").trim();
            }
        }
        else if (arguments_.INSTALL || arguments_.REMOVE)
        {
            bool found = false;
            std::string name;
            if (arguments_.CUSTOM_INSTALL)
            {
                name = std::string{ argv[nArg] };
            }
            else
            {
                name = Vita::string(argv[nArg]).toLower();
            }
            for (const auto& config : configs_)
            {
                if (config == name)
                {
                    found = true;
                    break;
                }
            }
            if (!found)
            {
                configs_.push_back(name);
            }
        }
        else
        {
            throw std::runtime_error{"invalid option: " + std::string(argv[nArg]) + "\n"};
        }
    }
    if (!arguments_.SHOW_PCI && !arguments_.SHOW_USB)
    {
        arguments_.SHOW_USB = true;
        arguments_.SHOW_PCI = true;
    }
}

bool Mhwd::optionsDontInterfereWithEachOther() const
{
    if (arguments_.INSTALL && arguments_.REMOVE)
    {
        consoleWriter_.printError("install and remove options can only be used separately!\n");
        consoleWriter_.printHelp();
        return false;
    }
    else if ((arguments_.INSTALL || arguments_.REMOVE) && arguments_.AUTOCONFIGURE)
    {
        consoleWriter_.printError("auto option can't be combined with install and remove options!\n");
        consoleWriter_.printHelp();
        return false;
    }
    else if ((arguments_.REMOVE || arguments_.INSTALL) && configs_.empty())
    {
        consoleWriter_.printError("nothing to do?!\n");
        consoleWriter_.printHelp();
        return false;
    }

    return true;
}

int Mhwd::launch(int argc, char *argv[])
{
    std::vector<std::string> missingDirs { checkEnvironment() };
    if (!missingDirs.empty())
    {
        consoleWriter_.printError("Following directories do not exist:");
        for (const auto& dir : missingDirs)
        {
            consoleWriter_.printStatus(dir);
        }
        return 1;
    }

    std::string operationType;
    bool autoConfigureNonFreeDriver = false;
    std::string autoConfigureClassID;

    try
    {
        tryToParseCmdLineOptions(argc, argv, autoConfigureNonFreeDriver, operationType,
                autoConfigureClassID);
    }
    catch(const std::runtime_error& e)
    {
        consoleWriter_.printError(e.what());
        consoleWriter_.printHelp();
        return 1;
    }

    if (!optionsDontInterfereWithEachOther())
    {
        return 1;
    }

    // Check for invalid configs
    for (auto&& invalidConfig : data_.invalidConfigs)
    {
        consoleWriter_.printWarning("config '" + invalidConfig.configPath_ + "' is invalid!");
    }

    // > Perform operations:

    // List all configs
    if (arguments_.LIST_ALL && arguments_.SHOW_PCI)
    {
        if (!data_.allPCIConfigs.empty())
        {
            consoleWriter_.listConfigs(data_.allPCIConfigs, "All PCI configs:");
        }
        else
        {
            consoleWriter_.printWarning("No PCI configs found!");
        }
    }
    if (arguments_.LIST_ALL && arguments_.SHOW_USB)
    {
        if (!data_.allUSBConfigs.empty())
        {
            consoleWriter_.listConfigs(data_.allUSBConfigs, "All USB configs:");
        }
        else
        {
            consoleWriter_.printWarning("No USB configs found!");
        }
    }

    // List installed configs
    if (arguments_.LIST_INSTALLED && arguments_.SHOW_PCI)
    {
        if (arguments_.DETAIL)
        {
            consoleWriter_.printInstalledConfigs("PCI", data_.installedPCIConfigs);
        }
        else
        {
            if (!data_.installedPCIConfigs.empty())
            {
                consoleWriter_.listConfigs(data_.installedPCIConfigs, "Installed PCI configs:");
            }
            else
            {
                consoleWriter_.printWarning("No installed PCI configs!");
            }
        }
    }
    if (arguments_.LIST_INSTALLED && arguments_.SHOW_USB)
    {
        if (arguments_.DETAIL)
        {
            consoleWriter_.printInstalledConfigs("USB", data_.installedUSBConfigs);
        }
        else
        {
            if (!data_.installedUSBConfigs.empty())
            {
                consoleWriter_.listConfigs(data_.installedUSBConfigs, "Installed USB configs:");
            }
            else
            {
                consoleWriter_.printWarning("No installed USB configs!");
            }
        }
    }

    // List available configs
    if (arguments_.LIST_AVAILABLE && arguments_.SHOW_PCI)
    {
        if (arguments_.DETAIL)
        {
            consoleWriter_.printAvailableConfigsInDetail("PCI", data_.PCIDevices);
        }
        else
        {
            for (auto&& PCIDevice : data_.PCIDevices)
            {
                if (!PCIDevice->availableConfigs_.empty())
                {
                    consoleWriter_.listConfigs(PCIDevice->availableConfigs_,
                            PCIDevice->sysfsBusID_ + " (" + PCIDevice->classID_ + ":"
                                    + PCIDevice->vendorID_ + ":" + PCIDevice->deviceID_ + ") "
                                    + PCIDevice->className_ + " " + PCIDevice->vendorName_ + ":");
                }
            }
        }
    }

    if (arguments_.LIST_AVAILABLE && arguments_.SHOW_USB)
    {
        if (arguments_.DETAIL)
        {
            consoleWriter_.printAvailableConfigsInDetail("USB", data_.USBDevices);
        }

        else
        {
            for (auto&& USBdevice : data_.USBDevices)
            {
                if (!USBdevice->availableConfigs_.empty())
                {
                    consoleWriter_.listConfigs(USBdevice->availableConfigs_,
                            USBdevice->sysfsBusID_ + " (" + USBdevice->classID_ + ":"
                            + USBdevice->vendorID_ + ":" + USBdevice->deviceID_ + ") "
                            + USBdevice->className_ + " " + USBdevice->vendorName_ + ":");
                }
            }
        }
    }

    // List hardware information
    if (arguments_.LIST_HARDWARE && arguments_.SHOW_PCI)
    {
        if (arguments_.DETAIL)
        {
            consoleWriter_.printDeviceDetails(hw_pci);
        }
        else
        {
            consoleWriter_.listDevices(data_.PCIDevices, "PCI");
        }
    }
    if (arguments_.LIST_HARDWARE && arguments_.SHOW_USB)
    {
        if (arguments_.DETAIL)
        {
            consoleWriter_.printDeviceDetails(hw_usb);
        }
        else
        {
            consoleWriter_.listDevices(data_.USBDevices, "USB");
        }
    }

    // Auto configuration
    if (arguments_.AUTOCONFIGURE)
    {
        std::vector<std::shared_ptr<Device>> *devices;
        std::vector<Config> *installedConfigs;

        if ("USB" == operationType)
        {
            devices = &data_.USBDevices;
            installedConfigs = &data_.installedUSBConfigs;
        }
        else
        {
            devices = &data_.PCIDevices;
            installedConfigs = &data_.installedPCIConfigs;
        }
        bool foundDevice = false;
        for (auto&& device : *devices)
        {
            if (device->classID_ != autoConfigureClassID)
            {
                continue;
            }
            else
            {
                foundDevice = true;
                std::optional<Config> config;

                for (auto&& availableConfig : device->availableConfigs_)
                {
                    if (autoConfigureNonFreeDriver || availableConfig.freedriver_)
                    {
                        config = availableConfig;
                        break;
                    }
                }

                if (!config)
                {
                    consoleWriter_.printWarning(
                            "No config found for device: " + device->sysfsBusID_ + " ("
                                    + device->classID_ + ":" + device->vendorID_ + ":"
                                    + device->deviceID_ + ") " + device->className_ + " "
                                    + device->vendorName_ + " " + device->deviceName_);
                    continue;
                }
                else
                {
                    // If force is not set then skip found config
                    bool skip = false;
                    if (!arguments_.FORCE)
                    {
                        skip = std::find_if(installedConfigs->begin(), installedConfigs->end(),
                                [&config](const auto& conf) -> bool {
                                                return conf.name_ == config->name_;
                                }) != installedConfigs->end();
                    }
                    // Print found config
                    if (skip)
                    {
                        consoleWriter_.printStatus(
                                "Skipping already installed config '" + config->name_ +
                                "' for device: " + device->sysfsBusID_ + " (" +
                                device->classID_ + ":" + device->vendorID_ + ":" +
                                device->deviceID_ + ") " + device->className_ + " " +
                                device->vendorName_ + " " + device->deviceName_);
                    }
                    else
                    {
                        consoleWriter_.printStatus(
                                "Using config '" + config->name_ + "' for device: " +
                                device->sysfsBusID_ + " (" + device->classID_ + ":" +
                                device->vendorID_ + ":" + device->deviceID_ + ") " +
                                device->className_ + " " + device->vendorName_ + " " +
                                device->deviceName_);
                    }

                    bool alreadyInList = std::find(configs_.begin(), configs_.end(), config->name_) != configs_.end();
                    if (!alreadyInList && !skip)
                    {
                        configs_.push_back(config->name_);
                    }
                }
            }
        }

        if (!foundDevice)
        {
            consoleWriter_.printWarning("No device of class " + autoConfigureClassID + " found!");
        }
        else if (!configs_.empty())
        {
            arguments_.INSTALL = true;
        }
    }

    // Transaction
    if (arguments_.INSTALL || arguments_.REMOVE)
    {
        if (!isUserRoot())
        {
            consoleWriter_.printError("You cannot perform this operation unless you are root!");
        }
        else
        {
            for (auto&& configName = configs_.begin();
                    configName != configs_.end(); configName++)
            {
                if (arguments_.CUSTOM_INSTALL)
                {
                    // Custom install -> get configs
                    struct stat filestatus;
                    std::string filepath = (*configName) + "/MHWDCONFIG";

                    if (0 != stat(filepath.c_str(), &filestatus))
                    {
                        consoleWriter_.printError("custom config '" + filepath + "' does not exist!");
                        return 1;
                    }
                    else if (!S_ISREG(filestatus.st_mode))
                    {
                        consoleWriter_.printError("custom config '" + filepath + "' is invalid!");
                        return 1;
                    }
                    else
                    {
                        config_.reset(new Config(filepath, operationType));
                        if (!config_->readConfigFile(filepath))
                        {
                            consoleWriter_.printError("failed to read custom config '" + filepath + "'!");
                            return 1;
                        }

                        else if (!performTransaction(*config_, MHWD::TransactionType::INSTALL))
                        {
                            return 1;
                        }
                    }
                }
                else if (arguments_.INSTALL)
                {
                    auto configOptional = getAvailableConfig(*configName, operationType);

                    if(!configOptional){

                        configOptional = getDatabaseConfig(*configName, operationType);
                        consoleWriter_.printWarning(
                            "no matching device for config '" + (*configName) + "' found!");

                    }

                    config_ = configOptional?std::make_unique<Config>(configOptional.value()):nullptr;
                    if (!config_)
                    {
                        consoleWriter_.printError("config '" + (*configName) + "' does not exist!");
                        return 1;
                    }


                    if (!performTransaction(*config_, MHWD::TransactionType::INSTALL))
                    {
                        return 1;
                    }
                }
                else if (arguments_.REMOVE)
                {
                    auto configOptional = getInstalledConfig((*configName), operationType);

                    config_ = configOptional?std::make_unique<Config>(configOptional.value()):nullptr;

                    if (!config_)
                    {
                        consoleWriter_.printError("config '" + (*configName) + "' is not installed!");
                        return 1;
                    }

                    else if (!performTransaction(*config_, MHWD::TransactionType::REMOVE))
                    {
                        return 1;
                    }
                }
            }
        }
    }
    return 0;
}

std::string Mhwd::gatherConfigContent(const std::vector<Config> & configuration) const
{
    std::string config;
    for (auto&& c : configuration)
    {
        config += " " + c.name_;
    }
    return config;
}
