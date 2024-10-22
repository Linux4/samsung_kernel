/*
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

package vendor.qti.hardware.display.config;

@VintfStability
parcelable CacV2ConfigExt {
    double redCenterPhaseStep;             // Red color's phase step for center position
    double redSecondOrderPhaseStep;        // Red color's second order phase step
    double blueCenterPhaseStep;            // Blue color's phase step for center position
    double blueSecondOrderPhaseStep;       // Blue color's second-order phase step
    double pixelPitch;                     // Pixel pitch is device dependent
    double normalization;                  // Normalization factor
    int verticalCenter;                    // Lens' vertical center position with respect to display panel. If 0, treated as (height/2 - 1) by display service.
    int horizontalCenter;                  // Lens' horizontal center position with respect to display panel. If 0, treated as (width/2 - 1) by display service.
}
