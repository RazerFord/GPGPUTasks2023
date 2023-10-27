#include <cstddef>
#include <libgpu/context.h>
#include <libgpu/shared_device_buffer.h>
#include <libutils/fast_random.h>
#include <libutils/misc.h>
#include <libutils/timer.h>

// Этот файл будет сгенерирован автоматически в момент сборки - см. convertIntoHeader в CMakeLists.txt:18
#include "cl/radix_cl.h"
#include "libgpu/work_size.h"

#include <iostream>
#include <stdexcept>
#include <tuple>
#include <vector>


template<typename T>
void raiseFail(const T &a, const T &b, std::string message, std::string filename, int line) {
    if (a != b) {
        std::cerr << message << " But " << a << " != " << b << ", " << filename << ":" << line << std::endl;
        throw std::runtime_error(message);
    }
}

#define EXPECT_THE_SAME(a, b, message) raiseFail(a, b, message, __FILE__, __LINE__)


int main(int argc, char **argv) {
    gpu::Device device = gpu::chooseGPUDevice(argc, argv);

    gpu::Context context;
    context.init(device.device_id_opencl);
    context.activate();

    int benchmarkingIters = 10;
    const unsigned work_size = 128;
    const unsigned int n = 32 * 1024 * 1024;
    const unsigned int nd4 = 4 * n / work_size;
    std::vector<unsigned int> as(n, 0);
    FastRandom r(n);
    for (unsigned int i = 0; i < n; ++i) {
        as[i] = ((unsigned int) r.next(0, std::numeric_limits<int>::max()));
    }
    std::cout << "Data generated for n=" << n << "!" << std::endl;

    std::vector<unsigned int> cpu_sorted;
    {
        timer t;
        for (int iter = 0; iter < benchmarkingIters; ++iter) {
            cpu_sorted = as;
            std::sort(cpu_sorted.begin(), cpu_sorted.end());
            t.nextLap();
        }
        std::cout << "CPU: " << t.lapAvg() << "+-" << t.lapStd() << " s" << std::endl;
        std::cout << "CPU: " << (n / 1000 / 1000) / t.lapAvg() << " millions/s" << std::endl;
    }
    gpu::gpu_mem_32u as_gpu, bs_gpu, cs_gpu, ds_gpu;
    as_gpu.resizeN(n);
    bs_gpu.resizeN(nd4);
    cs_gpu.resizeN(nd4);
    ds_gpu.resizeN(n);

    {
        ocl::Kernel fill_zero(radix_kernel, radix_kernel_length, "fill_zero");
        ocl::Kernel counting(radix_kernel, radix_kernel_length, "counting");
        ocl::Kernel prefix_sum(radix_kernel, radix_kernel_length, "prefix_sum");
        ocl::Kernel shift(radix_kernel, radix_kernel_length, "shift");
        ocl::Kernel radix(radix_kernel, radix_kernel_length, "radix");
        fill_zero.compile();
        counting.compile();
        prefix_sum.compile();
        shift.compile();
        radix.compile();

        timer t;
        for (int iter = 0; iter < benchmarkingIters; ++iter) {
            as_gpu.writeN(as.data(), n);

            gpu::WorkSize wsnd4(work_size, nd4);
            gpu::WorkSize wsn(work_size, n);

            t.restart();// Запускаем секундомер после прогрузки данных, чтобы замерять время работы кернела, а не трансфер данных

            for (unsigned i = 0; i < 32; i += 2) {
                fill_zero.exec(wsnd4, bs_gpu, nd4);
                fill_zero.exec(wsnd4, cs_gpu, nd4);

                counting.exec(wsn, as_gpu, bs_gpu, i, n);
                for (unsigned offset = 1; offset < nd4; offset *= 2) {
                    prefix_sum.exec(wsnd4, bs_gpu, cs_gpu, offset, nd4);
                    std::swap(bs_gpu, cs_gpu);
                }
                shift.exec(wsnd4, bs_gpu, cs_gpu, nd4);
                radix.exec(wsn, as_gpu, ds_gpu, cs_gpu, i, n);
                std::swap(as_gpu, ds_gpu);
            }

            t.nextLap();
        }
        std::cout << "GPU: " << t.lapAvg() << "+-" << t.lapStd() << " s" << std::endl;
        std::cout << "GPU: " << (n / 1000 / 1000) / t.lapAvg() << " millions/s" << std::endl;

        as_gpu.readN(as.data(), n);
    }

    // Проверяем корректность результатов
    for (int i = 0; i < n; ++i) {
        EXPECT_THE_SAME(as[i], cpu_sorted[i], "GPU results should be equal to CPU results!");
    }
    return 0;
}