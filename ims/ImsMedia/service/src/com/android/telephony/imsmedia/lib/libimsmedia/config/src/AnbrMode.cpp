/**
 * Copyright (C) 2024 The Android Open Source Project
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

#include <AnbrMode.h>

namespace android
{

namespace telephony
{

namespace imsmedia
{

/** Native representation of android.telephony.imsmedia.AnbrMode */
AnbrMode::AnbrMode()
{
    setDefaultAnbrMode();
}

AnbrMode::AnbrMode(const AnbrMode& param)
{
    this->mAnbrUplinkMode = param.mAnbrUplinkMode;
    this->mAnbrDownlinkMode = param.mAnbrDownlinkMode;
}

AnbrMode::~AnbrMode() {}

AnbrMode& AnbrMode::operator=(const AnbrMode& param)
{
    if (this != &param)
    {
        this->mAnbrUplinkMode = param.mAnbrUplinkMode;
        this->mAnbrDownlinkMode = param.mAnbrDownlinkMode;
    }
    return *this;
}

bool AnbrMode::operator==(const AnbrMode& param) const
{
    return (this->mAnbrUplinkMode == param.mAnbrUplinkMode &&
            this->mAnbrDownlinkMode == param.mAnbrDownlinkMode);
}

bool AnbrMode::operator!=(const AnbrMode& param) const
{
    return (this->mAnbrUplinkMode != param.mAnbrUplinkMode ||
            this->mAnbrDownlinkMode != param.mAnbrDownlinkMode);
}

status_t AnbrMode::writeToParcel(Parcel* out) const
{
    status_t err;
    if (out == nullptr)
    {
        return BAD_VALUE;
    }

    err = out->writeInt32(mAnbrUplinkMode);
    if (err != NO_ERROR)
    {
        return err;
    }

    err = out->writeInt32(mAnbrDownlinkMode);
    if (err != NO_ERROR)
    {
        return err;
    }
    return NO_ERROR;
}

status_t AnbrMode::readFromParcel(const Parcel* in)
{
    status_t err;
    if (in == nullptr)
    {
        return BAD_VALUE;
    }

    err = in->readInt32(&mAnbrUplinkMode);
    if (err != NO_ERROR)
    {
        return err;
    }

    err = in->readInt32(&mAnbrDownlinkMode);
    if (err != NO_ERROR)
    {
        return err;
    }

    return NO_ERROR;
}

void AnbrMode::setAnbrUplinkCodecMode(const int32_t uplinkMode)
{
    mAnbrUplinkMode = uplinkMode;
}

int32_t AnbrMode::getAnbrUplinkCodecMode()
{
    return mAnbrUplinkMode;
}

void AnbrMode::setAnbrDownlinkCodecMode(const int32_t downlinkMode)
{
    mAnbrDownlinkMode = downlinkMode;
}

int32_t AnbrMode::getAnbrDownlinkCodecMode()
{
    return mAnbrDownlinkMode;
}

void AnbrMode::setDefaultAnbrMode()
{
    mAnbrUplinkMode = 0;
    mAnbrDownlinkMode = 0;
}

}  // namespace imsmedia

}  // namespace telephony

}  // namespace android
