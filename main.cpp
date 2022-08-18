

#include <iostream>
#include <iomanip>
#include <fstream>
#include <filesystem>
#include <memory>
#include <chrono>
#include <array>
#include <vector>
#include <map>
#include <string>
#include <sstream>
#include <fstream>
#include <variant>

#include <typeinfo>

#include <boost/asio/io_service.hpp>
#include <sdbusplus/asio/object_server.hpp>

#include "args.hxx"

namespace fs = std::filesystem;

void directory_walk()
{
    std::cout << "\nshaj tester for BMC purposes. Ver. 0.0\n";
    fs::path hwmon0 {"/sys/bus/platform/devices/1e786000.pwm-tacho-controller/hwmon/hwmon0/"};
    for(auto const &entry : fs::directory_iterator {hwmon0})
    {
        if(entry.is_regular_file())
        {
            std::cout << entry.path().stem().string() << "\n";
        }
        else
        {
            std::cout << "~~~\n";
        }
    }
    std::cout << std::endl;
}

const std::array<std::string, 16> tach_name_list {
    "/sys/bus/platform/devices/1e786000.pwm-tacho-controller/hwmon/hwmon0/fan1_input",
    "/sys/bus/platform/devices/1e786000.pwm-tacho-controller/hwmon/hwmon0/fan2_input",
    "/sys/bus/platform/devices/1e786000.pwm-tacho-controller/hwmon/hwmon0/fan3_input",
    "/sys/bus/platform/devices/1e786000.pwm-tacho-controller/hwmon/hwmon0/fan4_input",
    "/sys/bus/platform/devices/1e786000.pwm-tacho-controller/hwmon/hwmon0/fan5_input",
    "/sys/bus/platform/devices/1e786000.pwm-tacho-controller/hwmon/hwmon0/fan6_input",
    "/sys/bus/platform/devices/1e786000.pwm-tacho-controller/hwmon/hwmon0/fan7_input",
    "/sys/bus/platform/devices/1e786000.pwm-tacho-controller/hwmon/hwmon0/fan8_input",
    "/sys/bus/platform/devices/1e786000.pwm-tacho-controller/hwmon/hwmon0/fan9_input",
    "/sys/bus/platform/devices/1e786000.pwm-tacho-controller/hwmon/hwmon0/fan10_input",
    "/sys/bus/platform/devices/1e786000.pwm-tacho-controller/hwmon/hwmon0/fan11_input",
    "/sys/bus/platform/devices/1e786000.pwm-tacho-controller/hwmon/hwmon0/fan12_input",
    "/sys/bus/platform/devices/1e786000.pwm-tacho-controller/hwmon/hwmon0/fan13_input",
    "/sys/bus/platform/devices/1e786000.pwm-tacho-controller/hwmon/hwmon0/fan14_input",
    "/sys/bus/platform/devices/1e786000.pwm-tacho-controller/hwmon/hwmon0/fan15_input",
    "/sys/bus/platform/devices/1e786000.pwm-tacho-controller/hwmon/hwmon0/fan16_input"
};

const std::array<std::string, 16> fan_dbuspath_list {
    "/xyz/openbmc_project/sensors/fan_tach/Fan_CPU1",
    "/xyz/openbmc_project/sensors/fan_tach/Fan_CPU2",
    "/xyz/openbmc_project/sensors/fan_tach/Fan_chassis_1",
    "/xyz/openbmc_project/sensors/fan_tach/Fan_chassis_2",
    "/xyz/openbmc_project/sensors/fan_tach/Fan_chassis_3",
    "/xyz/openbmc_project/sensors/fan_tach/Fan_chassis_4",
    "/xyz/openbmc_project/sensors/fan_tach/Fan_chassis_5",
    "/xyz/openbmc_project/sensors/fan_tach/Fan_chassis_6",
    "/xyz/openbmc_project/sensors/fan_tach/TEST_1",
    "/xyz/openbmc_project/sensors/fan_tach/TEST_3",
    "/xyz/openbmc_project/sensors/fan_tach/TEST_5",
    "/xyz/openbmc_project/sensors/fan_tach/TEST_7",
    "/xyz/openbmc_project/sensors/fan_tach/TEST_9",
    "/xyz/openbmc_project/sensors/fan_tach/TEST_11",
    "/xyz/openbmc_project/sensors/fan_tach/TEST_12",
    "/xyz/openbmc_project/sensors/fan_tach/TEST_13",
};

// Отсюда https://stackoverflow.com/a/21995693
template<typename TimeT = std::chrono::milliseconds>
struct measure
{
    template<typename F, typename ...Args>
    static typename TimeT::rep execution(F&& func, Args&&... args)
    {
        auto start = std::chrono::steady_clock::now();
        std::forward<decltype(func)>(func)(std::forward<Args>(args)...);
        auto duration = std::chrono::duration_cast<TimeT>(std::chrono::steady_clock::now() - start);
        return duration.count();
    }
};


class ItemStat
{
public:
    std::string name;
    fs::path file;
    int good_cnt = 0;
    int flt_cnt = 0;
    int val_max = 0;
    int val_min = 0;
    int last_value = 0;
    int nan_count = 0;
    int max_nonan = 0;
    std::map<int, int> values;

    ItemStat(const std::string &s) : file(s)
    {
        name = file.stem();
    }
};

std::ostream& operator<<(std::ostream& os, ItemStat& it)
{
    os << std::setw(12) << it.name
       << std::setw(7)  << it.good_cnt
       << std::setw(7)  << it.flt_cnt
       << std::setw(7)  << it.val_min
       << std::setw(7)  << it.val_max
       << std::setw(9) << it.last_value
       << " | " << std::setw(5) << it.values.size()
       << std::setw(7) << it.max_nonan;
    return os;
}

struct Property
{
    std::string interface;
    std::string property;
    std::string value;

    int good_cnt = 0;
    int flt_cnt = 0;

    unsigned long timer = 0;
    unsigned long timer_min = 0;
    unsigned long timer_max = 0;

    Property(const std::string &i, const std::string &p) :
            interface(i), property(p)
    {
    }
};

class DbusItemStat : public ItemStat
{
public:
    std::string service;
    std::vector<Property> requests;

    DbusItemStat(const std::string &s) : ItemStat(s),
            service("xyz.openbmc_project.FanSensor")
    {
        requests.reserve(3);
        requests.emplace_back("xyz.openbmc_project.Sensor.Value", "Value");
        requests.emplace_back("xyz.openbmc_project.State.Decorator.Availability", "Available");
        requests.emplace_back("xyz.openbmc_project.State.Decorator.OperationalStatus", "Functional");
    }
};

std::ostream& operator<<(std::ostream& os, DbusItemStat& it)
{
    os << std::setw(15) << it.name
       << std::setw(7)  << it.good_cnt
       << std::setw(7)  << it.flt_cnt
       << " | " << std::setw(5) << it.values.size()
       << std::setw(7) << it.max_nonan;
       // << std::setw(7)  << it.val_min
       // << std::setw(7)  << it.val_max
       // << std::setw(15) << it.last_value
       // << "\n";
    for(const auto &req : it.requests)
    {
        os << std::setw(12) << req.property
           << std::setw(8) << req.value
           << std::setw(7) << req.good_cnt
           << std::setw(7) << req.flt_cnt
           << std::setw(7) << req.timer_min
           << std::setw(7) << req.timer_max;
    }

    return os;
}


void report(const std::vector<std::unique_ptr<ItemStat>> &tach_list)
{
    std::cout << "\n\nReport <ItemStat>:\n";
    for(const auto &tach : tach_list)
    {
        std::cout << *tach << "\n";
    }
}

void report(const std::vector<std::unique_ptr<DbusItemStat>> &tach_list)
{
    std::cout << "\n\nReport <DbusItemStat>:\n";
    for(const auto &tach : tach_list)
    {
        std::cout << *tach << "\n";
    }
}


void store_value(ItemStat &it, int val)
{
    auto f = it.values.find(val);
    if(f != it.values.end())
    {
        it.values[val] += 1;
    }
    else
    {
        it.values[val] = 1;
    }
}

namespace std {
std::ostream& operator<<(std::ostream &os, const std::pair<int, int> &p)
{
    os << p.first << ": " << p.second;
    return os;
}
}

void values_to_file(const std::vector<std::unique_ptr<ItemStat>> &tach_list)
{
    const auto p1 = std::chrono::system_clock::now();
    std::string fn = std::to_string(std::chrono::duration_cast<std::chrono::seconds>(p1.time_since_epoch()).count());
    std::ofstream os {std::string("/tmp/") + fn + "_1.txt"};

    for(const auto &it : tach_list)
    {
        os << "\n=== " << it->name << "\n";
        std::copy(it->values.cbegin(), it->values.cend(), std::ostream_iterator<std::pair<int, int>>(os, "\n"));
    }
    // for(const auto &val : it.values)
    // {
    //     os << val.first << ": " << val.second << "\n";
    // }
}

void values_to_file(const std::vector<std::unique_ptr<DbusItemStat>> &tach_list)
{
    const auto p1 = std::chrono::system_clock::now();
    std::string fn = std::to_string(std::chrono::duration_cast<std::chrono::seconds>(p1.time_since_epoch()).count());
    std::ofstream os {std::string("/tmp/") + fn + "_2.txt"};

    for(const auto &it : tach_list)
    {
        os << "\n=== " << it->name << "\n";
        std::copy(it->values.cbegin(), it->values.cend(), std::ostream_iterator<std::pair<int, int>>(os, "\n"));
    }
}


std::vector<std::unique_ptr<ItemStat>> create_sysfs_list(std::vector<int> &crop)
{
    std::vector<std::unique_ptr<ItemStat>> tach_list;
    if(crop.size() != 0)
    {
        tach_list.reserve(sizeof(crop));
        for(const int idx : crop)
        {
            if(idx < tach_name_list.size())
            {
                tach_list.push_back(std::make_unique<ItemStat>(tach_name_list[idx]));
            }
        }
    }
    else
    {
        tach_list.reserve(tach_name_list.size());
        for(const auto &pstr : tach_name_list)
        {
            tach_list.push_back(std::make_unique<ItemStat>(pstr));
        }
    }
    return tach_list;
}

void scan_sysfs(int tests, std::vector<std::unique_ptr<ItemStat>> &tach_list)
{
    std::stringstream debug_str;
    for(int i=0; i<tests; i++)
    {
        for(auto &tach : tach_list)
        {
            auto time_to_read = measure<std::chrono::microseconds>::execution([&tach]()
                {
                    std::ifstream fs {tach->file};
                    try
                    {
                        fs >> tach->last_value;
                        fs.exceptions(fs.failbit);
                        store_value(*tach, tach->last_value);
                        tach->good_cnt++;
                        tach->nan_count++;
                        if(tach->val_min == 0)
                            tach->val_min = tach->last_value;
                        else if(tach->val_min > tach->last_value)
                            tach->val_min = tach->last_value;
                        if(tach->val_max < tach->last_value)
                            tach->val_max = tach->last_value;
                    }
                    catch(const std::ios_base::failure& e)
                    {
                        tach->flt_cnt++;
                        if(tach->max_nonan < tach->nan_count)
                            tach->max_nonan = tach->nan_count;
                        tach->nan_count = 0;
                        store_value(*tach, -1);
                    }
                });
            if(tach->max_nonan < tach->nan_count)
                tach->max_nonan = tach->nan_count;
            // debug_str << "type " << typeid(time_to_read).name() << " value " << time_to_read << "\n";
        }
        std::cout << "." << std::flush;
    }
    std::cout << "\n" << debug_str.str() << std::endl;
}

std::vector<std::unique_ptr<DbusItemStat>> create_dbus_list(std::vector<int> &crop)
{
    std::vector<std::unique_ptr<DbusItemStat>> tach_list;
    if(crop.size() != 0)
    {
        tach_list.reserve(crop.size());
        for(const int idx : crop)
        {
            if(idx < tach_name_list.size())
            {
                tach_list.push_back(std::make_unique<DbusItemStat>(fan_dbuspath_list[idx]));
            }
        }
    }
    else
    {
        tach_list.reserve(fan_dbuspath_list.size());
        for(const auto &pstr : fan_dbuspath_list)
        {
            tach_list.push_back(std::make_unique<DbusItemStat>(pstr));
        }
    }
    return tach_list;
}

void scan_dbus(int tests, std::vector<std::unique_ptr<DbusItemStat>> &tach_list,
               std::shared_ptr<sdbusplus::asio::connection>& conn)
{
    for(int i=0; i<tests; i++)
    {
        for(auto &tach : tach_list)
        {
            for(auto &req : tach->requests)
            {
                req.timer = measure<std::chrono::microseconds>::execution([&tach, &req, &conn]()
                    {
                        auto mapper = conn->new_method_call(
                                          tach->service.c_str(),
                                          tach->file.c_str(),
                                          "org.freedesktop.DBus.Properties", "Get");
                        mapper.append(req.interface);
                        mapper.append(req.property);
                        std::variant<double, bool> respData;
                        try
                        {
                            auto resp = conn->call(mapper);
                            resp.read(respData);
                            // val = std::get<double>(respData);
                            if(const double *pval = std::get_if<double>(&respData))
                            {
                                if(std::isnan(*pval))
                                {
                                    req.value = "nan";
                                    store_value(*tach, -1);
                                    if(tach->max_nonan < tach->nan_count)
                                        tach->max_nonan = tach->nan_count;
                                }
                                else
                                {
                                    req.value = std::to_string(int(*pval));
                                    store_value(*tach, int(*pval));
                                    tach->nan_count++;
                                }
                            }
                            else if(const bool *pval = std::get_if<bool>(&respData))
                                req.value = std::to_string(*pval);
                            // else if(const std::string *pval = std::get_if<std::string>(&respData))
                            //     req.value = *pval;
                        }
                        catch (const sdbusplus::exception_t& e)
                        {
                            std::cerr << "err " << e.what() << "\n";
                            req.value = "err";
                            if(tach->max_nonan < tach->nan_count)
                                tach->max_nonan = tach->nan_count;
                        }
                    });
                if(req.timer_min == 0)
                    req.timer_min = req.timer;
                else if(req.timer < req.timer_min)
                    req.timer_min = req.timer;
                if(req.timer > req.timer_max)
                    req.timer_max = req.timer;
            }

            if(tach->requests[0].value == "nan")
            {
                tach->flt_cnt++;
                tach->requests[0].flt_cnt++;
                if(tach->requests[1].value == "0")
                    tach->requests[1].good_cnt++;
                else
                    tach->requests[1].flt_cnt++;
                if(tach->requests[2].value == "0")
                    tach->requests[2].good_cnt++;
                else
                    tach->requests[2].flt_cnt++;
            }
            else
            {
                tach->good_cnt++;
                tach->requests[0].good_cnt++;
                if(tach->requests[1].value == "0")
                    tach->requests[1].flt_cnt++;
                else
                    tach->requests[1].good_cnt++;
                if(tach->requests[2].value == "0")
                    tach->requests[2].flt_cnt++;
                else
                    tach->requests[2].good_cnt++;
            }
        }
    }
}

int main(int argc, char const *argv[])
{
    std::cout << "shajtest version 1" << std::endl;

    args::ArgumentParser parser {"shajtest", "Rikor"};
    args::HelpFlag help {parser, "help", "Display this help menu", {'h', "help"}};
    parser.Prog(argv[0]);
    parser.ProglinePostfix("{command options}");
    args::Flag ver {parser, "ver", "version ", {'v', "version"}};
    args::ValueFlag<int> test_number {parser, "number", "test number ", {'t', "test"}, 1};
    args::ValueFlag<int> test_count {parser, "count", "count of iterations ", {'n', "count"}, 10};
    args::ValueFlagList<int> list_crop {parser, "crop", "List crop", {'l'}};

    try
    {
        parser.ParseCLI(argc, argv);
    }
    catch (args::Help)
    {
        std::cout << parser;
        return 0;
    }
    catch (args::ParseError e)
    {
        std::cerr << e.what() << std::endl;
        std::cerr << parser;
        return 1;
    }
    catch (args::ValidationError e)
    {
        std::cerr << e.what() << std::endl;
        std::cerr << parser;
        return 1;
    }

    // std::cout << "Test number: " << *test_number << "\n";
    // std::cout << "Iterations count:  " << *test_count << "\n";
    // std::vector<int> crop;
    // if(list_crop)
    // {
    //     std::copy(list_crop.cbegin(), list_crop.cend(), std::back_inserter(crop));
    // }
    if(*test_number == 1)
    {
        try
        {
            auto tach_list = create_sysfs_list(args::get(list_crop));
            scan_sysfs(*test_count, tach_list);
            report(tach_list);
            values_to_file(tach_list);
        }
        catch (const std::exception &e)
        {
            std::cerr << "Test 1 err " << e.what() << "\n";
        }
    }
    
    if(*test_number == 2)
    {
        try
        {
            boost::asio::io_service io;
            auto conn = std::make_shared<sdbusplus::asio::connection>(io, sdbusplus::bus::new_system().release());

            auto dbus_list = create_dbus_list(args::get(list_crop));
            scan_dbus(*test_count, dbus_list, conn);
            report(dbus_list);
            values_to_file(dbus_list);
        }
        catch (const std::exception &e)
        {
            std::cerr << "Test 2 err " << e.what() << "\n";
        }
    }

    return 0;
}

