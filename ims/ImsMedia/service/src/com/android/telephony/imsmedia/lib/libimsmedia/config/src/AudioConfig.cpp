/**
 * Copyright (C) 2022 The Android Open Source Project
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

#include <AudioConfig.h>

#include <string>

namespace android
{

namespace telephony
{

namespace imsmedia
{

const std::string kClassNameAmrParams("android.telephony.imsmedia.AmrParams");
const std::string kClassNameEvsParams("android.telephony.imsmedia.EvsParams");

AudioConfig::AudioConfig() :
        RtpConfig(RtpConfig::TYPE_AUDIO)
{
    mPTimeMillis = 0;
    mMaxPTimeMillis = 0;
    mDtxEnabled = false;
    mCodecType = 0;
    mDtmfTxPayloadTypeNumber = 0;
    mDtmfRxPayloadTypeNumber = 0;
    mDtmfSamplingRateKHz = 0;
}

AudioConfig::AudioConfig(AudioConfig* config) :
        RtpConfig(config)
{
    if (config != nullptr)
    {
        mPTimeMillis = config->mPTimeMillis;
        mMaxPTimeMillis = config->mMaxPTimeMillis;
        mDtxEnabled = config->mDtxEnabled;
        mCodecType = config->mCodecType;
        mDtmfTxPayloadTypeNumber = config->mDtmfTxPayloadTypeNumber;
        mDtmfRxPayloadTypeNumber = config->mDtmfRxPayloadTypeNumber;
        mDtmfSamplingRateKHz = config->mDtmfSamplingRateKHz;
        mAmrParams = config->mAmrParams;
        mEvsParams = config->mEvsParams;
    }
}

AudioConfig::AudioConfig(const AudioConfig& config) :
        RtpConfig(config),
        mAmrParams(config.mAmrParams),
        mEvsParams(config.mEvsParams)
{
    mPTimeMillis = config.mPTimeMillis;
    mMaxPTimeMillis = config.mMaxPTimeMillis;
    mDtxEnabled = config.mDtxEnabled;
    mCodecType = config.mCodecType;
    mDtmfTxPayloadTypeNumber = config.mDtmfTxPayloadTypeNumber;
    mDtmfRxPayloadTypeNumber = config.mDtmfRxPayloadTypeNumber;
    mDtmfSamplingRateKHz = config.mDtmfSamplingRateKHz;
}

AudioConfig::~AudioConfig() {}

AudioConfig& AudioConfig::operator=(const AudioConfig& config)
{
    if (this != &config)
    {
        RtpConfig::operator=(config);
        mPTimeMillis = config.mPTimeMillis;
        mMaxPTimeMillis = config.mMaxPTimeMillis;
        mDtxEnabled = config.mDtxEnabled;
        mCodecType = config.mCodecType;
        mDtmfTxPayloadTypeNumber = config.mDtmfTxPayloadTypeNumber;
        mDtmfRxPayloadTypeNumber = config.mDtmfRxPayloadTypeNumber;
        mDtmfSamplingRateKHz = config.mDtmfSamplingRateKHz;
        mAmrParams = config.mAmrParams;
        mEvsParams = config.mEvsParams;
    }
    return *this;
}

bool AudioConfig::operator==(const AudioConfig& config) const
{
    return (RtpConfig::operator==(config) && this->mPTimeMillis == config.mPTimeMillis &&
            this->mMaxPTimeMillis == config.mMaxPTimeMillis &&
            this->mDtxEnabled == config.mDtxEnabled && this->mCodecType == config.mCodecType &&
            this->mDtmfTxPayloadTypeNumber == config.mDtmfTxPayloadTypeNumber &&
            this->mDtmfRxPayloadTypeNumber == config.mDtmfRxPayloadTypeNumber &&
            this->mDtmfSamplingRateKHz == config.mDtmfSamplingRateKHz &&
            this->mAmrParams == config.mAmrParams && this->mEvsParams == config.mEvsParams);
}

bool AudioConfig::operator!=(const AudioConfig& config) const
{
    return (RtpConfig::operator!=(config) || this->mPTimeMillis != config.mPTimeMillis ||
            this->mMaxPTimeMillis != config.mMaxPTimeMillis ||
            this->mDtxEnabled != config.mDtxEnabled || this->mCodecType != config.mCodecType ||
            this->mDtmfTxPayloadTypeNumber != config.mDtmfTxPayloadTypeNumber ||
            this->mDtmfRxPayloadTypeNumber != config.mDtmfRxPayloadTypeNumber ||
            this->mDtmfSamplingRateKHz != config.mDtmfSamplingRateKHz ||
            this->mAmrParams != config.mAmrParams || this->mEvsParams != config.mEvsParams);
}

status_t AudioConfig::writeToParcel(Parcel* out) const
{
    status_t err;
    if (out == nullptr)
    {
        return BAD_VALUE;
    }

    err = RtpConfig::writeToParcel(out);
    if (err != NO_ERROR)
    {
        return err;
    }

    err = out->writeByte(mPTimeMillis);
    if (err != NO_ERROR)
    {
        return err;
    }

    err = out->writeInt32(mMaxPTimeMillis);
    if (err != NO_ERROR)
    {
        return err;
    }

    int32_t value = 0;
    mDtxEnabled ? value = 1 : value = 0;
    err = out->writeInt32(value);
    if (err != NO_ERROR)
    {
        return err;
    }

    err = out->writeInt32(mCodecType);
    if (err != NO_ERROR)
    {
        return err;
    }

    err = out->writeByte(mDtmfTxPayloadTypeNumber);
    if (err != NO_ERROR)
    {
        return err;
    }

    err = out->writeByte(mDtmfRxPayloadTypeNumber);
    if (err != NO_ERROR)
    {
        return err;
    }

    err = out->writeByte(mDtmfSamplingRateKHz);
    if (err != NO_ERROR)
    {
        return err;
    }

    String16 classNameAmr(kClassNameAmrParams.c_str());
    err = out->writeString16(classNameAmr);
    if (err != NO_ERROR)
    {
        return err;
    }

    err = mAmrParams.writeToParcel(out);
    if (err != NO_ERROR)
    {
        return err;
    }

    String16 classNameEvs(kClassNameEvsParams.c_str());
    err = out->writeString16(classNameEvs);
    if (err != NO_ERROR)
    {
        return err;
    }

    err = mEvsParams.writeToParcel(out);
    if (err != NO_ERROR)
    {
        return err;
    }

    return NO_ERROR;
}

status_t AudioConfig::readFromParcel(const Parcel* in)
{
    status_t err;
    if (in == nullptr)
    {
        return BAD_VALUE;
    }

    err = RtpConfig::readFromParcel(in);
    if (err != NO_ERROR)
    {
        return err;
    }

    err = in->readByte(&mPTimeMillis);
    if (err != NO_ERROR)
    {
        return err;
    }

    err = in->readInt32(&mMaxPTimeMillis);
    if (err != NO_ERROR)
    {
        return err;
    }

    int32_t value = 0;
    err = in->readInt32(&value);
    if (err != NO_ERROR)
    {
        return err;
    }

    value == 0 ? mDtxEnabled = false : mDtxEnabled = true;

    err = in->readInt32(&mCodecType);
    if (err != NO_ERROR)
    {
        return err;
    }

    err = in->readByte(&mDtmfTxPayloadTypeNumber);
    if (err != NO_ERROR)
    {
        return err;
    }

    err = in->readByte(&mDtmfRxPayloadTypeNumber);
    if (err != NO_ERROR)
    {
        return err;
    }

    err = in->readByte(&mDtmfSamplingRateKHz);
    if (err != NO_ERROR)
    {
        return err;
    }

    String16 className;
    err = in->readString16(&className);

    if (err == NO_ERROR)
    {
        // read AmrParams
        err = mAmrParams.readFromParcel(in);
        if ((mCodecType == CODEC_AMR || mCodecType == CODEC_AMR_WB) && err != NO_ERROR)
        {
            return err;
        }
    }
    else if (err == UNEXPECTED_NULL)
    {
        mAmrParams.setDefaultAmrParams();
    }
    else
    {
        return err;
    }

    err = in->readString16(&className);
    if (err == NO_ERROR)
    {
        // read EvsParams
        err = mEvsParams.readFromParcel(in);
        if (mCodecType == CODEC_EVS && err != NO_ERROR)
        {
            return err;
        }
    }
    else if (err == UNEXPECTED_NULL)
    {
        mEvsParams.setDefaultEvsParams();
    }
    else
    {
        return err;
    }

    return NO_ERROR;
}

void AudioConfig::setPTimeMillis(const int8_t ptime)
{
    mPTimeMillis = ptime;
}

int8_t AudioConfig::getPTimeMillis()
{
    return mPTimeMillis;
}

void AudioConfig::setMaxPTimeMillis(const int32_t maxPtime)
{
    mMaxPTimeMillis = maxPtime;
}

int32_t AudioConfig::getMaxPTimeMillis()
{
    return mMaxPTimeMillis;
}

void AudioConfig::setDtxEnabled(const bool enable)
{
    mDtxEnabled = enable;
}

bool AudioConfig::getDtxEnabled()
{
    return mDtxEnabled;
}

void AudioConfig::setCodecType(const int32_t type)
{
    mCodecType = type;
}

int32_t AudioConfig::getCodecType()
{
    return mCodecType;
}

void AudioConfig::setTxDtmfPayloadTypeNumber(const int8_t num)
{
    mDtmfTxPayloadTypeNumber = num;
}

void AudioConfig::setRxDtmfPayloadTypeNumber(const int8_t num)
{
    mDtmfRxPayloadTypeNumber = num;
}

int8_t AudioConfig::getTxDtmfPayloadTypeNumber()
{
    return mDtmfTxPayloadTypeNumber;
}

int8_t AudioConfig::getRxDtmfPayloadTypeNumber()
{
    return mDtmfRxPayloadTypeNumber;
}

void AudioConfig::setDtmfSamplingRateKHz(const int8_t sampling)
{
    mDtmfSamplingRateKHz = sampling;
}

int8_t AudioConfig::getDtmfSamplingRateKHz()
{
    return mDtmfSamplingRateKHz;
}

void AudioConfig::setAmrParams(const AmrParams& param)
{
    mAmrParams = param;
}

AmrParams AudioConfig::getAmrParams()
{
    return mAmrParams;
}

void AudioConfig::setEvsParams(const EvsParams& param)
{
    mEvsParams = param;
}

EvsParams AudioConfig::getEvsParams()
{
    return mEvsParams;
}

}  // namespace imsmedia

}  // namespace telephony

}  // namespace android
