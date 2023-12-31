/*
 * Copyright (C) 2020 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

//#define LOG_NDEBUG 0
#define LOG_TAG "mediametrics::stringutils"
#include <utils/Log.h>

#include "StringUtils.h"

#include <charconv>

#include "AudioTypes.h"

namespace android::mediametrics::stringutils {

std::string tokenizer(std::string::const_iterator& it,
        const std::string::const_iterator& end, const char *reserved)
{
    // consume leading white space
    for (; it != end && std::isspace(*it); ++it);
    if (it == end) return {};

    auto start = it;
    // parse until we hit a reserved keyword or space
    if (strchr(reserved, *it)) return {start, ++it};
    for (;;) {
        ++it;
        if (it == end || std::isspace(*it) || strchr(reserved, *it)) return {start, it};
    }
}

std::vector<std::string> split(const std::string& flags, const char *delim)
{
    std::vector<std::string> result;
    for (auto it = flags.begin(); ; ) {
        auto flag = tokenizer(it, flags.end(), delim);
        if (flag.empty() || !std::isalnum(flag[0])) return result;
        result.emplace_back(std::move(flag));

        // look for the delimeter and discard
        auto token = tokenizer(it, flags.end(), delim);
        if (token.size() != 1 || strchr(delim, token[0]) == nullptr) return result;
    }
}

bool parseVector(const std::string &str, std::vector<int32_t> *vector) {
    std::vector<int32_t> values;
    const char *p = str.c_str();
    const char *last = p + str.size();
    while (p != last) {
        if (*p == ',' || *p == '{' || *p == '}') {
            p++;
        }
        int32_t value = -1;
        auto [ptr, error] = std::from_chars(p, last, value);
        if (error == std::errc::invalid_argument || error == std::errc::result_out_of_range) {
            return false;
        }
        p = ptr;
        values.push_back(value);
    }
    *vector = std::move(values);
    return true;
}

std::vector<std::pair<std::string, std::string>> getDeviceAddressPairs(const std::string& devices)
{
    std::vector<std::pair<std::string, std::string>> result;

    // Currently, the device format is EXACTLY
    // (device1, addr1)|(device2, addr2)|...

    static constexpr char delim[] = "()|,";
    for (auto it = devices.begin(); ; ) {
        auto token = tokenizer(it, devices.end(), delim);
        if (token != "(") return result;

        auto device = tokenizer(it, devices.end(), delim);
        if (device.empty() || !std::isalnum(device[0])) return result;

        token = tokenizer(it, devices.end(), delim);
        if (token != ",") return result;

        // special handling here for empty addresses
        auto address = tokenizer(it, devices.end(), delim);
        if (address.empty() || !std::isalnum(device[0])) return result;
        if (address == ")") {  // no address, just the ")"
            address.clear();
        } else {
            token = tokenizer(it, devices.end(), delim);
            if (token != ")") return result;
        }

        result.emplace_back(std::move(device), std::move(address));

        token = tokenizer(it, devices.end(), delim);
        if (token != "|") return result;  // this includes end of string detection
    }
}

size_t replace(std::string &str, const char *targetChars, const char replaceChar)
{
    size_t replaced = 0;
    for (char &c : str) {
        if (strchr(targetChars, c) != nullptr) {
            c = replaceChar;
            ++replaced;
        }
    }
    return replaced;
}

template <types::AudioEnumCategory CATEGORY>
std::pair<std::string /* external statsd */, std::string /* internal */>
parseDevicePairs(const std::string& devicePairs) {
    std::pair<std::string, std::string> result{};
    const auto devaddrvec = stringutils::getDeviceAddressPairs(devicePairs);
    for (const auto& [device, addr] : devaddrvec) { // addr ignored for now.
        if (!result.second.empty()) {
            result.second.append("|"); // delimit devices with '|'.
            result.first.append("|");
        }
        result.second.append(device);
        result.first.append(types::lookup<CATEGORY, std::string>(device));
    }
    return result;
}

std::pair<std::string /* external statsd */, std::string /* internal */>
parseOutputDevicePairs(const std::string& devicePairs) {
    return parseDevicePairs<types::OUTPUT_DEVICE>(devicePairs);
}

std::pair<std::string /* external statsd */, std::string /* internal */>
parseInputDevicePairs(const std::string& devicePairs) {
    return parseDevicePairs<types::INPUT_DEVICE>(devicePairs);
}

} // namespace android::mediametrics::stringutils
