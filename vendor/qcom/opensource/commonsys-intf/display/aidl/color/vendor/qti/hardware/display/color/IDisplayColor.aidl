/*
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

package vendor.qti.hardware.display.color;

import vendor.qti.hardware.display.color.Result;
import vendor.qti.hardware.display.color.SprModeInfo;

@VintfStability
interface IDisplayColor {
    /**
     * Initialize a color context.
     *
     * Each client is expected to call init interface and acquire a context
     * handle before exercising any other interface.
     *
     * @param flags client identifier.
     * @return context handle on success or negative value if anything wrong.
     */
    int init(in int flags);

    /**
     * De-initialize a color context.
     *
     * Client must free the context after use.
     *
     * @param  ctxHandle context handle.
     * @param  flags reserved.
     * @return OK on success or BAD_VALUE if any parameters are invalid.
     */
    Result deInit(in int ctxHandle, in int flags);

    /**
     * Initialize the qdcm socket service.
     *
     * Client needs to call this function to start/stop the socket service
     */
    void toggleSocketService(in boolean enable);

    /**
     * Get render intents map.
     *
     * Clients can get the render intents string and enums map for display specified by id.
     *
     * @param  disp_id is display ID.
     * @param out render_intent_string is string vector for all the render intents.
     * @param out render_intent_enum is numbers for all the render intents.
     * @return OK on success or BAD_VALUE if any parameters are invalid.
     */
    Result getRenderIntentsMap(in int disp_id,
        out String[] render_intent_string, out int[] render_intent_enum);

    /**
     * Get spr mode configuration.
     *
     * @param  ctxHandle context handle.
     * @param  dispId display id.
     * @param out info spr mode configuration.
     * @return OK on success or BAD_VALUE if any parameters are invalid.
     */
    Result getSPRMode(in int ctxHandle, in int dispId, out SprModeInfo info);

    /**
     * Set spr mode.
     *
     * @param  ctxHandle context handle.
     * @param  dispId display id.
     * @param  cfg spr mode configuration.
     * @return OK on success or error if any parameters are invalid.
     */
    Result setSPRMode(in int ctxHandle, in int dispId, in SprModeInfo info);
}
