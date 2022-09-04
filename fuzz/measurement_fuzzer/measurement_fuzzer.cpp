/******************************************************************************
 *
 * Copyright (C) 2022 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *****************************************************************************
 */
#include <fuzzer/FuzzedDataProvider.h>

#include "minikin/Measurement.h"
using namespace minikin;

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    FuzzedDataProvider fdp(data, size);
    float advance = fdp.ConsumeFloatingPoint<float>();
    size_t start = fdp.ConsumeIntegral<size_t>();
    size_t count = fdp.ConsumeIntegral<size_t>();
    size_t offset = fdp.ConsumeIntegral<size_t>();
    int remaining = fdp.remaining_bytes();
    int buf_size = fdp.ConsumeIntegralInRange<int>(0, remaining / (int)sizeof(uint16_t));
    remaining = fdp.remaining_bytes();
    int advances_size = fdp.ConsumeIntegralInRange<int>(0, remaining / (int)sizeof(float));
    uint16_t buf[buf_size];
    float advances[advances_size];
    for (int i = 0; i < buf_size; i++) buf[i] = fdp.ConsumeIntegral<uint16_t>();
    for (int i = 0; i < advances_size; i++) advances[i] = fdp.ConsumeFloatingPoint<float>();
    size_t advance_offset = getOffsetForAdvance(advances, buf, start, count, advance);
    float advance_run = getRunAdvance(advances, buf, start, count, offset);
    return 0;
}
