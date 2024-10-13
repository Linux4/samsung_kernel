/*
 * SpeakerProtCommon.h
 *
 *  Created on: Oct 10, 2023
 *      Author: MASHKURAHMB
 */

#ifndef DEVICE_INC_SPEAKERPROTCOMMON_H_
#define DEVICE_INC_SPEAKERPROTCOMMON_H_

struct agmMetaData {
    uint8_t *buf;
    uint32_t size;
    agmMetaData(uint8_t *b, uint32_t s)
        :buf(b),size(s) {}
};

#endif /* DEVICE_INC_SPEAKERPROTCOMMON_H_ */