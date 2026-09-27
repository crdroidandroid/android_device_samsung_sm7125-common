/**
 * Copyright (C) 2023 The Android Open Source Project
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

#include <MediaQualityStatus.h>

namespace android
{

namespace telephony
{

namespace imsmedia
{

MediaQualityStatus::MediaQualityStatus()
{
    mRtpInactivityTimeMillis = 0;
    mRtcpInactivityTimeMillis = 0;
    mRtpPacketLossRate = 0;
    mRtpJitterMillis = 0;
}

MediaQualityStatus::MediaQualityStatus(const MediaQualityStatus& status)
{
    mRtpInactivityTimeMillis = status.mRtpInactivityTimeMillis;
    mRtcpInactivityTimeMillis = status.mRtcpInactivityTimeMillis;
    mRtpPacketLossRate = status.mRtpPacketLossRate;
    mRtpJitterMillis = status.mRtpJitterMillis;
}

MediaQualityStatus::~MediaQualityStatus() {}

MediaQualityStatus& MediaQualityStatus::operator=(const MediaQualityStatus& status)
{
    if (this != &status)
    {
        mRtpInactivityTimeMillis = status.mRtpInactivityTimeMillis;
        mRtcpInactivityTimeMillis = status.mRtcpInactivityTimeMillis;
        mRtpPacketLossRate = status.mRtpPacketLossRate;
        mRtpJitterMillis = status.mRtpJitterMillis;
    }

    return *this;
}

bool MediaQualityStatus::operator==(const MediaQualityStatus& status) const
{
    return (mRtpInactivityTimeMillis == status.mRtpInactivityTimeMillis &&
            mRtcpInactivityTimeMillis == status.mRtcpInactivityTimeMillis &&
            mRtpPacketLossRate == status.mRtpPacketLossRate &&
            mRtpJitterMillis == status.mRtpJitterMillis);
}

bool MediaQualityStatus::operator!=(const MediaQualityStatus& status) const
{
    return (mRtpInactivityTimeMillis != status.mRtpInactivityTimeMillis ||
            mRtcpInactivityTimeMillis != status.mRtcpInactivityTimeMillis ||
            mRtpPacketLossRate != status.mRtpPacketLossRate ||
            mRtpJitterMillis != status.mRtpJitterMillis);
}

status_t MediaQualityStatus::writeToParcel(Parcel* out) const
{
    if (out == nullptr)
    {
        return BAD_VALUE;
    }

    status_t err;
    err = out->writeInt32(mRtpInactivityTimeMillis);
    if (err != NO_ERROR)
    {
        return err;
    }

    err = out->writeInt32(mRtcpInactivityTimeMillis);
    if (err != NO_ERROR)
    {
        return err;
    }

    err = out->writeInt32(mRtpPacketLossRate);
    if (err != NO_ERROR)
    {
        return err;
    }

    err = out->writeInt32(mRtpJitterMillis);
    if (err != NO_ERROR)
    {
        return err;
    }

    return NO_ERROR;
}

status_t MediaQualityStatus::readFromParcel(const Parcel* in)
{
    if (in == nullptr)
    {
        return BAD_VALUE;
    }

    status_t err;
    err = in->readInt32(&mRtpInactivityTimeMillis);
    if (err != NO_ERROR)
    {
        return err;
    }

    err = in->readInt32(&mRtcpInactivityTimeMillis);
    if (err != NO_ERROR)
    {
        return err;
    }

    err = in->readInt32(&mRtpPacketLossRate);
    if (err != NO_ERROR)
    {
        return err;
    }

    err = in->readInt32(&mRtpJitterMillis);
    if (err != NO_ERROR)
    {
        return err;
    }

    return NO_ERROR;
}

void MediaQualityStatus::setRtpInactivityTimeMillis(int32_t time)
{
    mRtpInactivityTimeMillis = time;
}

int32_t MediaQualityStatus::getRtpInactivityTimeMillis()
{
    return mRtpInactivityTimeMillis;
}

void MediaQualityStatus::setRtcpInactivityTimeMillis(int32_t time)
{
    mRtcpInactivityTimeMillis = time;
}

int32_t MediaQualityStatus::getRtcpInactivityTimeMillis()
{
    return mRtcpInactivityTimeMillis;
}

void MediaQualityStatus::setRtpPacketLossRate(int32_t rate)
{
    mRtpPacketLossRate = rate;
}

int32_t MediaQualityStatus::getRtpPacketLossRate()
{
    return mRtpPacketLossRate;
}

void MediaQualityStatus::setRtpJitterMillis(int32_t jitter)
{
    mRtpJitterMillis = jitter;
}

int32_t MediaQualityStatus::getRtpJitterMillis()
{
    return mRtpJitterMillis;
}

}  // namespace imsmedia

}  // namespace telephony

}  // namespace android
