//===- Xrt.cpp - ESI XRT device backend -------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// DO NOT EDIT!
// This file is distributed as part of an ESI package. The source for this file
// should always be modified within CIRCT
// (lib/dialect/ESI/runtime/cpp/lib/backends/Cosim.cpp).
//
//===----------------------------------------------------------------------===//

#include "esi/backends/Xrt.h"
#include "esi/Services.h"

// XRT includes
#include "experimental/xrt_bo.h"
#include "experimental/xrt_device.h"
#include "experimental/xrt_ip.h"
#include "experimental/xrt_xclbin.h"

#include <fstream>
#include <iostream>
#include <set>

using namespace std;

using namespace esi;
using namespace esi::services;
using namespace esi::backends::xrt;

/// Parse the connection string and instantiate the accelerator. Connection
/// string format:
///  <xclbin>[:<device_id>]
/// wherein <device_id> is in BDF format.
unique_ptr<AcceleratorConnection>
XrtAccelerator::connect(string connectionString) {
  string xclbin;
  string device_id;

  size_t colon = connectionString.find(':');
  if (colon == string::npos) {
    xclbin = connectionString;
  } else {
    xclbin = connectionString.substr(0, colon);
    device_id = connectionString.substr(colon + 1);
  }

  return make_unique<XrtAccelerator>(xclbin, device_id);
}

struct esi::backends::xrt::XrtAccelerator::Impl {
  constexpr static char kernel[] = "esi_kernel";

  Impl(string xclbin, string device_id) {
    if (device_id.empty())
      device = ::xrt::device(0);
    else
      device = ::xrt::device(device_id);

    printf("Loaded device %s\n",
           device.get_info<::xrt::info::device::name>().c_str());
    auto uuid = device.load_xclbin(xclbin);
    printf("Loaded xclbin %s\n", xclbin.c_str());
    ip = ::xrt::ip(device, uuid, kernel);
    printf("Loaded kernel %s\n", kernel);
  }

  ::xrt::device device;
  ::xrt::ip ip;
};

/// Construct and connect to a cosim server.
XrtAccelerator::XrtAccelerator(string xclbin, string kernel) {
  impl = make_unique<Impl>(xclbin, kernel);
}

namespace {
class XrtMMIO : public MMIO {
public:
  XrtMMIO(::xrt::ip &ip) : ip(ip) {}

  uint32_t read(uint32_t addr) const override {
    printf("Reading from addr %d\n", addr);
    return ip.read_register(addr);
  }
  void write(uint32_t addr, uint32_t data) override {
    ip.write_register(addr, data);
  }

private:
  ::xrt::ip &ip;
};
} // namespace

Service *XrtAccelerator::createService(Service::Type svcType, AppIDPath id,
                                       std::string implName,
                                       const ServiceImplDetails &details,
                                       const HWClientDetails &clients) {
  if (svcType == typeid(MMIO))
    return new XrtMMIO(impl->ip);
  else if (svcType == typeid(SysInfo))
    return new MMIOSysInfo(getService<MMIO>());
  return nullptr;
}

REGISTER_ACCELERATOR("xrt", backends::xrt::XrtAccelerator);
