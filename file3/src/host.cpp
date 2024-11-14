/**
* Copyright (C) 2019-2021 Xilinx, Inc
*
* Licensed under the Apache License, Version 2.0 (the "License"). You may
* not use this file except in compliance with the License. A copy of the
* License is located at
*
*     http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
* WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
* License for the specific language governing permissions and limitations
* under the License.
*/

#include <iostream>
#include <cstring>
#include <time.h>

// XRT includes
#include "experimental/xrt_bo.h"
#include "experimental/xrt_device.h"
#include "experimental/xrt_kernel.h"

#define DATA_SIZE 32768

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cout << "Usage: " << argv[0] << " <XCLBIN File>" << std::endl;
        return EXIT_FAILURE;
    }
    
    //////////////////////////////////////////
    // Open xclbin
    //////////////////////////////////////////
    auto device = xrt::device(0); //device index=0
    char* xclbinFilename=argv[1];
    std::cout << "Open the device " << 0 << std::endl;
    std::cout << "Load the xclbin " << xclbinFilename << std::endl;
	auto uuid = device.load_xclbin(xclbinFilename);
	auto dhdl = xrtDeviceOpenFromXcl(device);
    size_t vector_size_bytes = sizeof(int) * DATA_SIZE;
    auto krnl = xrt::kernel(device, uuid, "vadd");

    //Allocate Buffer in Global Memory
    auto bo0 = xrt::bo(device, vector_size_bytes, krnl.group_id(0));
    auto bo1 = xrt::bo(device, vector_size_bytes, krnl.group_id(1));
    auto bo_out = xrt::bo(device, vector_size_bytes, krnl.group_id(2));

    // Map the contents of the buffer object into host memory
    auto bo0_map = bo0.map<int*>();
    auto bo1_map = bo1.map<int*>();
    auto bo_out_map = bo_out.map<int*>();
    std::fill(bo0_map, bo0_map + DATA_SIZE, 0);
    std::fill(bo1_map, bo1_map + DATA_SIZE, 0);
    std::fill(bo_out_map, bo_out_map + DATA_SIZE, 0);

    // Create the test data
    int bufReference[DATA_SIZE];
    for (int i = 0; i < DATA_SIZE; ++i) {
        bo0_map[i] = i;
        bo1_map[i] = i;
        bufReference[i] = bo0_map[i] + bo1_map[i];
    }

    // Synchronize buffer content with device side
    bo0.sync(XCL_BO_SYNC_BO_TO_DEVICE, vector_size_bytes, 0);
    bo1.sync(XCL_BO_SYNC_BO_TO_DEVICE, vector_size_bytes, 0);

    double kernel_time_in_sec = 0;
    std::chrono::duration<double> kernel_time(0);
    auto kernel_start = std::chrono::high_resolution_clock::now();

    //Execution of the kernel
    auto run = krnl(bo_out, bo0, bo1);
    run.wait();

    auto kernel_end = std::chrono::high_resolution_clock::now();
    kernel_time = std::chrono::duration<double>(kernel_end - kernel_start);
    kernel_time_in_sec = kernel_time.count();
    std::cout << "Execution time = " << kernel_time_in_sec << std::endl;

    // Get the output data from the device;
    bo_out.sync(XCL_BO_SYNC_BO_FROM_DEVICE, vector_size_bytes, 0);

    // Validate our results
    if (std::memcmp(bo_out_map, bufReference, DATA_SIZE))
        throw std::runtime_error("Value read back does not match reference");

    std::cout << "TEST PASSED\n";
    return 0;
}
