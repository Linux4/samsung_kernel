/*
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

package vendor.qti.hardware.display.config;

@VintfStability
parcelable CacV2Config {
    double k0r;             // Red color center position phase step
    double k1r;             // Red color second-order phase step
    double k0b;             // Blue color center position phase step
    double k1b;             // Blue color second-order phase step
    double pixel_pitch;     // Pixel pitch is device dependent
    double normalization;   // Normalization factor
}